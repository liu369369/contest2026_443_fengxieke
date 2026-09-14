#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
火眼（FireEye）上位机 —— 接收终端上报的电流/温度数据并实时显示。

设计目标：
  * 只用 Python 标准库，不需要 pip 安装任何东西；
  * 终端通过 HTTP POST 上报 JSON，页面在浏览器里看（手机也能看）；
  * 支持 --demo 模式自带模拟数据，方便没有硬件时先验证界面。

用法：
    python fireeye_monitor.py                 # 监听 0.0.0.0:8080
    python fireeye_monitor.py --port 9000     # 换端口
    python fireeye_monitor.py --demo          # 用模拟数据自测界面

终端上报格式（POST /api/data，Content-Type: application/json）：
    {"device":"fireeye-01","ts":1757851234,"current_a":0.32,"temp_c":26.4,
     "state":"NORMAL","alarm":false,"uptime_s":1234}

接口：
    POST /api/data    上报一条数据
    GET  /api/latest  查询最新数据与历史（供页面轮询）
    GET  /            监控页面
"""

import argparse
import json
import math
import random
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_POINTS = 600           # 保留最近 600 个点（5 秒一条 ≈ 50 分钟）
STATE_COLORS = {
    "NORMAL": "#2ecc71",
    "WARNING": "#f39c12",
    "ALARM": "#e74c3c",
    "SHUTDOWN": "#8e44ad",
}

LOCK = threading.Lock()
SAMPLES = []               # 最近的数据点
ALARMS = []                # 报警事件记录
LAST_SEEN = {"ts": None, "device": None}


def record(sample: dict) -> None:
    """保存一条上报数据，并记录报警事件。"""
    now = time.time()
    item = {
        "device": str(sample.get("device", "unknown")),
        "ts": float(sample.get("ts") or now),
        "recv_ts": now,
        "current_a": float(sample.get("current_a", 0.0)),
        "temp_c": float(sample.get("temp_c", 0.0)),
        "state": str(sample.get("state", "UNKNOWN")),
        "alarm": bool(sample.get("alarm", False)),
        "uptime_s": sample.get("uptime_s"),
    }

    with LOCK:
        prev_state = SAMPLES[-1]["state"] if SAMPLES else None
        SAMPLES.append(item)
        del SAMPLES[:-MAX_POINTS]
        LAST_SEEN["ts"] = now
        LAST_SEEN["device"] = item["device"]

        # 状态变化（含报警）记录成事件，作为"存证"的第一版形态
        if item["state"] != prev_state:
            ALARMS.append({
                "time": time.strftime("%H:%M:%S", time.localtime(item["ts"])),
                "state": item["state"],
                "current_a": round(item["current_a"], 2),
                "temp_c": round(item["temp_c"], 1),
            })
            del ALARMS[:-100]


PAGE = """<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>火眼 · 充电安全监测</title>
<style>
  body { font-family: "Microsoft YaHei", Arial, sans-serif; margin: 0; background: #f4f6f8; color: #222; }
  header { background: #1f2d3d; color: #fff; padding: 14px 20px; font-size: 20px; }
  header small { color: #9fb3c8; font-size: 13px; margin-left: 10px; }
  .wrap { max-width: 1000px; margin: 16px auto; padding: 0 12px; }
  .cards { display: flex; gap: 12px; flex-wrap: wrap; }
  .card { flex: 1 1 200px; background: #fff; border-radius: 8px; padding: 14px 16px;
          box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  .card .label { color: #6b7c93; font-size: 13px; }
  .card .value { font-size: 30px; font-weight: 600; margin-top: 6px; }
  .state { display: inline-block; padding: 4px 12px; border-radius: 14px;
           color: #fff; font-size: 14px; }
  canvas { width: 100%; height: 260px; background: #fff; border-radius: 8px;
           box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  table { width: 100%; border-collapse: collapse; background: #fff; border-radius: 8px;
          overflow: hidden; box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  th, td { padding: 8px 12px; text-align: left; font-size: 14px; border-bottom: 1px solid #eef1f4; }
  th { background: #fafbfc; color: #6b7c93; font-weight: 500; }
  h2 { font-size: 16px; margin: 22px 0 10px; color: #35495e; }
  .offline { color: #e74c3c; }
</style>
</head>
<body>
<header>火眼 · 电动自行车充电电气安全监测 <small>上位机监控台</small></header>
<div class="wrap">
  <div class="cards">
    <div class="card"><div class="label">当前状态</div>
      <div class="value"><span id="state" class="state" style="background:#95a5a6">--</span></div></div>
    <div class="card"><div class="label">电流 (A)</div><div class="value" id="cur">--</div></div>
    <div class="card"><div class="label">温度 (℃)</div><div class="value" id="tmp">--</div></div>
    <div class="card"><div class="label">设备 / 最后上报</div>
      <div class="value" style="font-size:16px" id="dev">--</div></div>
  </div>

  <h2>实时曲线（最近 5 分钟）</h2>
  <canvas id="chart" width="1000" height="260"></canvas>

  <h2>事件记录（状态变化 / 报警）</h2>
  <table><thead><tr><th>时间</th><th>状态</th><th>电流 (A)</th><th>温度 (℃)</th></tr></thead>
  <tbody id="events"><tr><td colspan="4" style="color:#aaa">暂无事件</td></tr></tbody></table>
</div>

<script>
function drawChart(points) {
  const c = document.getElementById('chart');
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height, pad = 34;
  ctx.clearRect(0, 0, W, H);
  if (points.length < 2) { ctx.fillStyle = '#aaa'; ctx.fillText('等待数据…', 20, 30); return; }

  const cur = points.map(p => p.current_a), tmp = points.map(p => p.temp_c);
  const yMax = Math.max(10, ...cur.map(Math.abs).map(v => v * 1.2), ...tmp.map(v => v * 1.2));

  // 坐标轴与网格
  ctx.strokeStyle = '#e6eaee'; ctx.fillStyle = '#8a99ab'; ctx.font = '12px sans-serif';
  for (let i = 0; i <= 4; i++) {
    const y = pad + (H - 2 * pad) * i / 4;
    ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(W - pad, y); ctx.stroke();
    ctx.fillText((yMax * (4 - i) / 4).toFixed(0), 6, y + 4);
  }

  const xAt = i => pad + (W - 2 * pad) * i / (points.length - 1);
  const yAt = v => pad + (H - 2 * pad) * (1 - v / yMax);

  const line = (arr, color) => {
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
    arr.forEach((v, i) => i ? ctx.lineTo(xAt(i), yAt(v)) : ctx.moveTo(xAt(i), yAt(v)));
    ctx.stroke();
  };
  line(cur, '#2980b9');   // 电流
  line(tmp, '#e67e22');   // 温度

  // 报警区间着色
  ctx.fillStyle = 'rgba(231,76,60,.12)';
  points.forEach((p, i) => {
    if (p.state === 'ALARM' || p.state === 'SHUTDOWN') {
      ctx.fillRect(xAt(i) - 1, pad, 2, H - 2 * pad);
    }
  });

  ctx.fillStyle = '#2980b9'; ctx.fillText('— 电流(A)', W - 200, 20);
  ctx.fillStyle = '#e67e22'; ctx.fillText('— 温度(℃)', W - 100, 20);
}

async function tick() {
  try {
    const r = await fetch('/api/latest');
    const d = await r.json();
    const s = d.last;
    if (s) {
      const st = document.getElementById('state');
      st.textContent = s.state;
      st.style.background = d.state_colors[s.state] || '#95a5a6';
      document.getElementById('cur').textContent = s.current_a.toFixed(2);
      document.getElementById('tmp').textContent = s.temp_c.toFixed(1);
      const age = Math.round(Date.now() / 1000 - s.recv_ts);
      document.getElementById('dev').innerHTML = s.device +
        (age > 15 ? ' <span class="offline">(离线 ' + age + 's)</span>' : ' (' + age + 's 前)');
    }
    drawChart(d.samples || []);
    const tb = document.getElementById('events');
    if (d.events && d.events.length) {
      tb.innerHTML = d.events.slice().reverse().map(e =>
        '<tr><td>' + e.time + '</td><td>' + e.state + '</td><td>' +
        e.current_a.toFixed(2) + '</td><td>' + e.temp_c.toFixed(1) + '</td></tr>').join('');
    }
  } catch (e) { /* 忽略瞬时错误，下个周期重试 */ }
}
setInterval(tick, 2000); tick();
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path.startswith("/api/latest"):
            with LOCK:
                payload = {
                    "last": SAMPLES[-1] if SAMPLES else None,
                    "samples": SAMPLES[-150:],
                    "events": ALARMS[-20:],
                    "state_colors": STATE_COLORS,
                }
            self._send(200, json.dumps(payload).encode("utf-8"), "application/json; charset=utf-8")
        else:
            self._send(200, PAGE.encode("utf-8"), "text/html; charset=utf-8")

    def do_POST(self) -> None:  # noqa: N802
        if not self.path.startswith("/api/data"):
            self._send(404, b"not found", "text/plain")
            return

        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            sample = json.loads(raw.decode("utf-8", "replace"))
        except json.JSONDecodeError:
            self._send(400, b"bad json", "text/plain")
            return

        record(sample)
        print(f"[{time.strftime('%H:%M:%S')}] {sample.get('device','?')} "
              f"I={sample.get('current_a')} A  T={sample.get('temp_c')} C  "
              f"state={sample.get('state')}", flush=True)
        self._send(200, b'{"ok":true}', "application/json")

    def log_message(self, fmt, *args):  # 静音默认访问日志
        return


def demo_loop() -> None:
    """--demo：自带模拟数据，用于没有硬件时验证界面。"""
    t = 0
    while True:
        t += 1
        if t < 30:
            cur, temp, state = 0.3 + 0.05 * math.sin(t / 3), 25 + 0.5 * math.sin(t / 5), "NORMAL"
        elif t < 50:
            cur, temp, state = 5.5, 48 + (t - 30) * 0.6, "WARNING"
        elif t < 70:
            cur, temp, state = 9.0, 63, "ALARM"
        else:
            cur, temp, state = 0.2, 26, "NORMAL"
            if t > 90:
                t = 0
        record({
            "device": "fireeye-demo",
            "ts": time.time(),
            "current_a": round(cur + random.uniform(-0.05, 0.05), 2),
            "temp_c": round(temp + random.uniform(-0.3, 0.3), 1),
            "state": state,
            "alarm": state in ("ALARM", "SHUTDOWN"),
        })
        time.sleep(2)


def main() -> None:
    parser = argparse.ArgumentParser(description="火眼上位机监控台")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址（默认 0.0.0.0）")
    parser.add_argument("--port", type=int, default=8080, help="监听端口（默认 8080）")
    parser.add_argument("--demo", action="store_true", help="使用模拟数据自测界面")
    args = parser.parse_args()

    if args.demo:
        threading.Thread(target=demo_loop, daemon=True).start()
        print("[demo] 已启动模拟数据源")

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"火眼上位机已启动： http://127.0.0.1:{args.port}")
    print("终端上报地址：     http://<本机IP>:%d/api/data" % args.port)
    print("按 Ctrl+C 退出")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已退出")


if __name__ == "__main__":
    main()
