#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
火眼（FireEye）上位机 —— 接收终端上报的电流/温度数据并实时显示。

设计目标：
  * 只用 Python 标准库，不需要 pip 安装任何东西；
  * 终端通过 HTTP POST 上报 JSON，页面在浏览器里看（手机也能看）；
  * 报警事件与报警前后的采样窗口会追加写入磁盘（默认 tools/data/），
    上位机重启后仍能读回，和板子端的 Flash 存证互为备份；
  * 支持 --demo 模式自带模拟数据，方便没有硬件时先验证界面。

用法：
    python fireeye_monitor.py                 # 监听 0.0.0.0:8080
    python fireeye_monitor.py --port 9000     # 换端口
    python fireeye_monitor.py --demo          # 用模拟数据自测界面
    python fireeye_monitor.py --data-dir D:\\fireeye_data   # 换留存目录

终端上报格式（POST /api/data，Content-Type: application/json）：
    {"device":"fireeye-01","ts":1757851234,"current_a":0.32,"temp_c":26.4,"bus_v":24.1,
     "state":"NORMAL","alarm":false,"uptime_s":1234}

接口：
    POST /api/data        上报一条数据
    GET  /api/latest      查询最新数据与历史（供页面轮询）
    GET  /api/ai          查询 AI 实时风险评估结果
    POST /api/ai          立即触发一次风险评估
    POST /api/ask         AI 问答 {"question": "..."}（限频 5s/次）
    GET  /api/events.csv  导出事件记录（Excel 可直接打开）
    GET  /                监控页面

AI 能力：
    * 报警事件研判 —— 状态变化时后台调用 MiMo，给出可能原因与处置建议；
    * 实时风险评估 —— 每 90 秒对近期采样做一次风险分级（低/中/高）+ 依据 + 建议；
    * AI 问答 —— 页面输入框提问，基于当前状态与近期采样回答；
    * 全部 AI 调用都在后台线程，失败不影响报警与上报链路。
    * key 只从环境变量 MIMO_API_KEY 或 ~/.fireeye_mimo.key 读取，不写入仓库。
"""

import argparse
import json
import math
import os
import random
import threading
import time
import urllib.request
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MAX_POINTS = 600           # 保留最近 600 个点（5 秒一条 ≈ 50 分钟）
ALARM_WINDOW = 30          # 报警时额外留存的采样条数（5 秒一条 ≈ 150 秒）
MAX_BODY_BYTES = 64 * 1024  # 上报请求体上限，防误推大文件
SAMPLE_LIMIT = 150         # /api/latest 返回的采样条数
EVENT_LIMIT = 50           # /api/latest 返回的事件条数

DATA_DIR = Path(__file__).resolve().parent / "data"
EVENTS_FILE = DATA_DIR / "fireeye_events.jsonl"

# ---- MiMo（云端 AI 研判）：key 只从环境变量 MIMO_API_KEY 或本机文件读取，绝不写入仓库 ----

MIMO_BASE = os.environ.get("MIMO_BASE_URL", "https://token-plan-cn.xiaomimimo.com/v1")
MIMO_MODEL = os.environ.get("MIMO_MODEL", "mimo-v2.6-pro")
MIMO_KEY_FILE = Path(os.environ.get("MIMO_KEY_FILE", str(Path.home() / ".fireeye_mimo.key")))
MIMO_TIMEOUT_S = 30
MIMO_ENABLED = True

STATE_COLORS = {
    "NORMAL": "#2ecc71",
    "WARNING": "#f39c12",
    "ALARM": "#e74c3c",
    "SHUTDOWN": "#8e44ad",
}

LOCK = threading.Lock()
SAMPLES = deque(maxlen=MAX_POINTS)   # 最近的数据点（自动淘汰最旧）
ALARMS = deque(maxlen=100)           # 报警事件记录
LAST_SEEN = {"ts": None, "device": None}


# ---------------------------------------------------------------------------
# 数值解析：坏字段不能让整条上报 500
# ---------------------------------------------------------------------------

def safe_float(value, default=0.0):
    """把任意输入转成有限浮点数；NaN/Inf/类型错误一律回退默认值。"""
    try:
        out = float(value)
    except (TypeError, ValueError):
        return default
    return out if math.isfinite(out) else default


def persist_event(event: dict, window: list) -> None:
    """把报警事件与它前后的采样追加到磁盘，程序退出或断电都不丢。"""
    try:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        line = json.dumps(
            {"logged_at": time.time(), "event": event, "window": window},
            ensure_ascii=False,
        )
        with open(EVENTS_FILE, "a", encoding="utf-8") as fh:
            fh.write(line + "\n")
            fh.flush()
            os.fsync(fh.fileno())
        print(f"[留存] {event['time']} {event['state']} 已写入 {EVENTS_FILE.name}",
              flush=True)
    except OSError as exc:
        print(f"[警告] 报警数据留存失败：{exc}", flush=True)


def load_history(limit: int = 100) -> int:
    """启动时把磁盘上留存的事件读回内存，页面上就能看到上一次的记录。"""
    if not EVENTS_FILE.exists():
        return 0

    try:
        with open(EVENTS_FILE, "r", encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    item = json.loads(line)
                except json.JSONDecodeError:
                    continue
                event = item.get("event")
                if isinstance(event, dict):
                    ALARMS.append(event)
    except OSError as exc:
        print(f"[警告] 历史事件读取失败：{exc}", flush=True)
        return 0

    while len(ALARMS) > limit:
        ALARMS.popleft()
    return len(ALARMS)


def mimo_key():
    """环境变量优先，其次本机 key 文件；取不到就返回 None（AI 研判自动关闭）。"""
    key = os.environ.get("MIMO_API_KEY", "").strip()
    if key:
        return key
    try:
        if MIMO_KEY_FILE.exists():
            return MIMO_KEY_FILE.read_text(encoding="utf-8").strip() or None
    except OSError:
        return None
    return None


def mimo_prompt(event: dict, window: list) -> str:
    """把报警事件与前后采样拼成提示词：只给事实，不下结论。"""
    if not window:
        return "充电回路状态变化：%s，电流 %.2fA，温度 %.1fC。" % (
            event["state"], event["current_a"], event["temp_c"])
    first, last = window[0], window[-1]
    return ("电动自行车充电监测终端上报：状态由 %s 变为 %s；"
            "窗口内电流 %.2fA → %.2fA，温度 %.1fC → %.1fC（共 %d 条采样）。"
            % (first["state"], last["state"], first["current_a"], last["current_a"],
               first["temp_c"], last["temp_c"], len(window)))


def mimo_analyze(event: dict, window: list):
    """调用 MiMo 生成一到两句中文研判；失败返回 None，绝不影响报警链路。"""
    key = mimo_key()
    if not key:
        return None
    body = json.dumps({
        "model": MIMO_MODEL,
        "messages": [
            {"role": "system",
             "content": "你是电动自行车充电安全监测系统的助手上位机。"
                        "根据给定的采样事实，用一到两句中文给出可能原因与处置建议，"
                        "不要复述推理过程，不要编造未给出的数据。"},
            {"role": "user", "content": mimo_prompt(event, window)},
        ],
        "max_tokens": 1200,
        "temperature": 0.3,
    }, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(MIMO_BASE + "/chat/completions", data=body,
                                 headers={"Authorization": "Bearer " + key,
                                          "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=MIMO_TIMEOUT_S) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    text = (data["choices"][0]["message"].get("content") or "").strip()
    usage = data.get("usage") or {}
    if usage:
        print("[MiMo] 研判完成，tokens=%s" % usage.get("total_tokens"), flush=True)
    return text or None


def mimo_worker(event: dict, window: list) -> None:
    """后台线程：拿到研判后补进事件并追加一条 AI 记录（不阻塞接收循环）。"""
    try:
        text = mimo_analyze(event, window)
    except Exception as exc:
        print("[MiMo] 研判失败（不影响报警）：%s" % exc, flush=True)
        return
    if not text:
        return
    with LOCK:
        event["ai_summary"] = text
    try:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        line = json.dumps({"logged_at": time.time(), "ai_summary": text,
                           "event_time": event.get("time"), "state": event.get("state")},
                          ensure_ascii=False)
        with open(EVENTS_FILE, "a", encoding="utf-8") as fh:
            fh.write(line + "\n")
            fh.flush()
            os.fsync(fh.fileno())
        print("[MiMo] %s %s | %s" % (event.get("time"), event.get("state"), text[:60]), flush=True)
    except OSError as exc:
        print("[警告] AI 研判留存失败：%s" % exc, flush=True)


# ---------------------------------------------------------------------------
# 运行时 AI：实时风险评估 + 问答
# ---------------------------------------------------------------------------

RISK_INTERVAL_S = 90        # 周期性风险评估间隔
RISK_WINDOW = 60            # 评估时取最近多少条采样
ASK_MAX_LEN = 200           # 问题长度上限
ASK_MIN_INTERVAL_S = 5      # 两次问答的最小间隔，防刷

AI_RISK = {"ts": 0, "level": None, "text": "等待首轮评估…", "facts": None}
_ai_last_ask = 0.0


def _samples_facts(limit: int = RISK_WINDOW) -> str:
    """把最近采样压成几行事实，给模型当输入；不带任何结论。"""
    with LOCK:
        pts = list(SAMPLES)[-limit:]
    if not pts:
        return "当前没有采样数据。"
    first, last = pts[0], pts[-1]
    cur = [p["current_a"] for p in pts]
    tmp = [p["temp_c"] for p in pts]
    span = max(1.0, last["recv_ts"] - first["recv_ts"])
    return (
        "最近 %d 条采样（约 %.0f 秒）："
        "电流 %.2f→%.2fA（min %.2f / max %.2f / 均值 %.2f），"
        "温度 %.1f→%.1fC（min %.1f / max %.1f），母线电压 %.2fV，"
        "当前状态 %s，最近 60 秒数据点 %d 个。"
        % (len(pts), span,
           first["current_a"], last["current_a"], min(cur), max(cur), sum(cur) / len(cur),
           first["temp_c"], last["temp_c"], min(tmp), max(tmp),
           last.get("bus_v", 0.0), last["state"], len(pts))
    )


def _mimo_chat(system: str, user: str, max_tokens: int = 400):
    """一次对话调用；返回文本或 None。不抛异常到调用方。"""
    if not MIMO_ENABLED:
        return None
    key = mimo_key()
    if not key:
        return None
    body = json.dumps({
        "model": MIMO_MODEL,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": user},
        ],
        "max_tokens": max_tokens,
        "temperature": 0.3,
    }, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(MIMO_BASE + "/chat/completions", data=body,
                                 headers={"Authorization": "Bearer " + key,
                                          "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=MIMO_TIMEOUT_S) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    return (data["choices"][0]["message"].get("content") or "").strip() or None


def ai_risk_assess() -> dict:
    """对最近采样做一次风险评估，返回 {level, text, ts}。失败不影响主链路。"""
    if not (MIMO_ENABLED and mimo_key()):
        return {"level": None, "text": "AI 未启用（缺少 MIMO_API_KEY 或已用 --no-mimo 关闭）",
                "ts": time.time()}
    facts = _samples_facts()
    prompt = (
        "以下是充电监测终端的真实采样事实：%s\n\n"
        "请只依据这些事实，输出三行（不要加编号和多余解释）：\n"
        "风险等级：低/中/高\n"
        "判断依据：一句话，引用具体数值\n"
        "处置建议：一句话"
        % facts
    )
    system = ("你是电动自行车充电安全监测系统的风险评估助手。"
              "只依据给定的采样事实判断，不要编造数据；"
              "电流持续快速上升或温度偏高时要提高风险等级。")
    try:
        text = _mimo_chat(system, prompt, max_tokens=300)
    except Exception as exc:
        print("[AI] 风险评估失败：%s" % exc, flush=True)
        return {"level": None, "text": "评估失败：%s" % exc, "ts": time.time()}
    if not text:
        return {"level": None, "text": "评估无返回", "ts": time.time()}

    level = None
    for tag in ("高", "中", "低"):
        if ("风险等级：" + tag) in text or ("风险等级:" + tag) in text:
            level = tag
            break
    result = {"level": level, "text": text, "ts": time.time(), "facts": facts}
    with LOCK:
        AI_RISK.update(result)
    print("[AI] 风险评估：%s | %s" % (level or "?", text.replace("\n", " ")[:80]), flush=True)
    return result


def ai_risk_loop() -> None:
    """后台线程：周期性风险评估；状态变化时立即插播一次。"""
    last_state = None
    while True:
        time.sleep(RISK_INTERVAL_S)
        try:
            with LOCK:
                cur_state = SAMPLES[-1]["state"] if SAMPLES else None
            if cur_state != last_state:
                last_state = cur_state
            ai_risk_assess()
        except Exception as exc:
            print("[AI] 风险评估循环异常：%s" % exc, flush=True)


def ai_ask(question: str) -> dict:
    """AI 问答：基于当前状态与近期采样回答用户问题。"""
    global _ai_last_ask
    question = (question or "").strip()[:ASK_MAX_LEN]
    if not question:
        return {"ok": False, "error": "问题为空"}
    if not (MIMO_ENABLED and mimo_key()):
        return {"ok": False, "error": "AI 未启用（缺少 MIMO_API_KEY 或已用 --no-mimo 关闭）"}
    now = time.time()
    if now - _ai_last_ask < ASK_MIN_INTERVAL_S:
        return {"ok": False, "error": "提问太频繁，请 %d 秒后再试" % ASK_MIN_INTERVAL_S}
    _ai_last_ask = now

    facts = _samples_facts(30)
    with LOCK:
        events = [dict(e) for e in list(ALARMS)[-5:]]
    event_text = ("；".join("%s %s 电流%.2fA 温度%.1fC" %
                            (e.get("time"), e.get("state"), e.get("current_a"), e.get("temp_c"))
                            for e in events)) or "无"
    prompt = (
        "当前充电监测数据：%s\n最近事件：%s\n\n用户问题：%s\n\n"
        "请用中文简洁回答（不超过三句），只依据上面的数据，不要编造。"
        % (facts, event_text, question)
    )
    system = ("你是电动自行车充电安全监测终端的助手。"
              "根据给定的采样与事件事实回答问题，语气专业、简短；"
              "如果数据不足以回答，直接说明数据不足。")
    try:
        answer = _mimo_chat(system, prompt, max_tokens=400)
    except Exception as exc:
        return {"ok": False, "error": "AI 调用失败：%s" % exc}
    if not answer:
        return {"ok": False, "error": "AI 无返回"}
    return {"ok": True, "answer": answer}


def record(sample: dict) -> None:
    """保存一条上报数据，并记录报警事件。"""
    now = time.time()
    item = {
        "device": str(sample.get("device", "unknown"))[:64],
        "ts": safe_float(sample.get("ts"), now) or now,
        "recv_ts": now,
        "current_a": safe_float(sample.get("current_a")),
        "temp_c": safe_float(sample.get("temp_c")),
        "bus_v": safe_float(sample.get("bus_v")),
        "state": str(sample.get("state", "UNKNOWN"))[:16],
        "alarm": bool(sample.get("alarm", False)),
        "uptime_s": safe_float(sample.get("uptime_s"), 0.0),
    }

    with LOCK:
        prev_state = SAMPLES[-1]["state"] if SAMPLES else None
        SAMPLES.append(item)
        LAST_SEEN["ts"] = now
        LAST_SEEN["device"] = item["device"]

        # 状态变化（含报警）记录成事件，并立刻落盘留存；
        # 上位机刚启动时还没有上一条状态，这种情况只在"一上来就是报警态"
        # 才记一条，避免每次开程序都留一条无意义的 NORMAL。
        is_transition = item["state"] != prev_state
        is_startup_alarm = prev_state is None and item["alarm"]

        if is_transition and (prev_state is not None or is_startup_alarm):
            event = {
                "time": time.strftime("%H:%M:%S", time.localtime(item["ts"])),
                "state": item["state"],
                "current_a": round(item["current_a"], 2),
                "temp_c": round(item["temp_c"], 1),
                "bus_v": round(item["bus_v"], 2),
            }
            ALARMS.append(event)

            window = [
                {
                    "ts": round(point["ts"], 3),
                    "current_a": round(point["current_a"], 2),
                    "temp_c": round(point["temp_c"], 1),
                    "state": point["state"],
                }
                for point in list(SAMPLES)[-ALARM_WINDOW:]
            ]
            persist_event(event, window)

            # 云端 AI 研判：放后台线程，网络慢或失败都不影响报警链路

            if MIMO_ENABLED and mimo_key():
                threading.Thread(target=mimo_worker, args=(event, window),
                                 daemon=True).start()


PAGE = """<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>火眼 · 充电安全监测</title>
<style>
  body { font-family: "Microsoft YaHei", Arial, sans-serif; margin: 0; background: #f4f6f8; color: #222; }
  header { background: #1f2d3d; color: #fff; padding: 14px 20px; font-size: 20px; display: flex;
           justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; }
  header small { color: #9fb3c8; font-size: 13px; margin-left: 10px; }
  header button { background: #35495e; color: #fff; border: 0; border-radius: 6px;
                  padding: 7px 14px; font-size: 13px; cursor: pointer; }
  header button:hover { background: #4a6785; }
  .wrap { max-width: 1000px; margin: 16px auto; padding: 0 12px; }
  .cards { display: flex; gap: 12px; flex-wrap: wrap; }
  .card { flex: 1 1 160px; background: #fff; border-radius: 8px; padding: 14px 16px;
          box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  .card .label { color: #6b7c93; font-size: 13px; }
  .card .value { font-size: 30px; font-weight: 600; margin-top: 6px; }
  .state { display: inline-block; padding: 4px 12px; border-radius: 14px;
           color: #fff; font-size: 14px; }
  canvas { width: 100%; height: 300px; background: #fff; border-radius: 8px;
           box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  table { width: 100%; border-collapse: collapse; background: #fff; border-radius: 8px;
          overflow: hidden; box-shadow: 0 1px 4px rgba(0,0,0,.08); }
  th, td { padding: 8px 12px; text-align: left; font-size: 14px; border-bottom: 1px solid #eef1f4; }
  th { background: #fafbfc; color: #6b7c93; font-weight: 500; }
  h2 { font-size: 16px; margin: 22px 0 10px; color: #35495e; }
  .offline { color: #e74c3c; }
  .muted { color: #98a6b8; font-size: 12px; margin-top: 6px; }
</style>
</head>
<body>
<header>
  <div>火眼 · 电动自行车充电电气安全监测 <small>上位机监控台</small></div>
  <button onclick="location.href='/api/events.csv'">导出事件 CSV</button>
</header>
<div class="wrap">
  <div class="cards">
    <div class="card"><div class="label">当前状态</div>
      <div class="value"><span id="state" class="state" style="background:#95a5a6">--</span></div></div>
    <div class="card"><div class="label">电流 (A)</div><div class="value" id="cur">--</div></div>
    <div class="card"><div class="label">温度 (℃)</div><div class="value" id="tmp">--</div></div>
    <div class="card"><div class="label">母线电压 (V)</div><div class="value" id="bus">--</div></div>
    <div class="card"><div class="label">设备 / 最后上报</div>
      <div class="value" style="font-size:16px" id="dev">--</div>
      <div class="muted" id="rate">采样率 --</div></div>
  </div>

  <h2>AI 实时风险评估</h2>
  <div class="card" style="display:flex;gap:14px;align-items:flex-start">
    <div style="flex:0 0 auto">
      <div class="label">风险等级</div>
      <div class="value"><span id="ai-level" class="state" style="background:#95a5a6">--</span></div>
      <div class="muted" id="ai-time">尚未评估</div>
    </div>
    <div style="flex:1">
      <div class="label">AI 判断</div>
      <div id="ai-text" style="margin-top:6px;line-height:1.7;white-space:pre-wrap">等待首轮评估…</div>
    </div>
    <button onclick="askRisk()" style="flex:0 0 auto;background:#35495e;color:#fff;border:0;
            border-radius:6px;padding:8px 14px;cursor:pointer">立即评估</button>
  </div>

  <h2>AI 问答</h2>
  <div class="card">
    <div style="display:flex;gap:8px">
      <input id="ask-input" placeholder="例如：现在充电安全吗？刚才为什么报警？"
             style="flex:1;padding:9px 12px;border:1px solid #d5dde6;border-radius:6px;font-size:14px"
             onkeydown="if(event.key==='Enter')ask()">
      <button onclick="ask()" style="background:#2980b9;color:#fff;border:0;border-radius:6px;
              padding:9px 18px;cursor:pointer">提问</button>
    </div>
    <div id="ask-answer" style="margin-top:10px;line-height:1.7;white-space:pre-wrap;color:#35495e">--</div>
  </div>

  <h2>实时曲线（双 Y 轴，最近 5 分钟）</h2>
  <canvas id="chart" width="1000" height="300"></canvas>
  <div class="muted">左轴 = 电流 (A，蓝)　右轴 = 温度 (℃，橙)　红色竖带 = ALARM / SHUTDOWN 区间</div>

  <h2>事件记录（状态变化 / 报警）</h2>
  <table><thead><tr><th>时间</th><th>状态</th><th>电流 (A)</th><th>温度 (℃)</th><th>电压 (V)</th><th>AI 研判 (MiMo)</th></tr></thead>
  <tbody id="events"><tr><td colspan="6" style="color:#aaa">暂无事件</td></tr></tbody></table>
</div>

<script>
function drawChart(points) {
  const c = document.getElementById('chart');
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height, padL = 46, padR = 46, padT = 22, padB = 34;
  ctx.clearRect(0, 0, W, H);
  if (points.length < 2) { ctx.fillStyle = '#aaa'; ctx.fillText('等待数据…', 20, 30); return; }

  const cur = points.map(p => p.current_a), tmp = points.map(p => p.temp_c);
  // 双轴：电流/温度各自缩放，互不挤压
  const curMax = Math.max(1, ...cur.map(v => Math.abs(v))) * 1.2;
  const tmpMax = Math.max(1, ...tmp.map(v => Math.abs(v))) * 1.2;
  const plotW = W - padL - padR, plotH = H - padT - padB;
  const xAt = i => padL + plotW * i / (points.length - 1);
  const yCur = v => padT + plotH * (1 - v / curMax);
  const yTmp = v => padT + plotH * (1 - v / tmpMax);

  // 报警区间着色（先画，垫在网格下面）
  ctx.fillStyle = 'rgba(231,76,60,.10)';
  points.forEach((p, i) => {
    if (p.state === 'ALARM' || p.state === 'SHUTDOWN') {
      ctx.fillRect(xAt(i) - 1, padT, 2, plotH);
    }
  });

  // 网格与双轴刻度
  ctx.strokeStyle = '#e6eaee'; ctx.fillStyle = '#8a99ab'; ctx.font = '12px sans-serif';
  for (let i = 0; i <= 4; i++) {
    const y = padT + plotH * i / 4;
    ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(W - padR, y); ctx.stroke();
    ctx.fillStyle = '#2980b9';
    ctx.fillText((curMax * (4 - i) / 4).toFixed(1), 6, y + 4);
    ctx.fillStyle = '#e67e22';
    ctx.fillText((tmpMax * (4 - i) / 4).toFixed(0), W - padR + 8, y + 4);
    ctx.fillStyle = '#8a99ab';
  }

  const line = (arr, color, yFn) => {
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
    arr.forEach((v, i) => i ? ctx.lineTo(xAt(i), yFn(v)) : ctx.moveTo(xAt(i), yFn(v)));
    ctx.stroke();
  };
  line(cur, '#2980b9', yCur);   // 电流（左轴）
  line(tmp, '#e67e22', yTmp);   // 温度（右轴）

  ctx.fillStyle = '#2980b9'; ctx.fillText('— 电流(A)', padL + 10, 16);
  ctx.fillStyle = '#e67e22'; ctx.fillText('— 温度(℃)', padL + 110, 16);
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
      document.getElementById('bus').textContent = s.bus_v ? s.bus_v.toFixed(2) : '--';
      const age = Math.round(Date.now() / 1000 - s.recv_ts);
      document.getElementById('dev').innerHTML = s.device +
        (age > 15 ? ' <span class="offline">(离线 ' + age + 's)</span>' : ' (' + age + 's 前)');
      if (s.recv_ts > 0 && s.ts > 0) {
        const rate = (d.samples.length / Math.max(1, s.recv_ts - d.samples[0].recv_ts)).toFixed(2);
        document.getElementById('rate').textContent = '采样率 ' + rate + ' 点/秒 · 共 ' + d.samples.length + ' 点';
      }
    }
    drawChart(d.samples || []);
    const tb = document.getElementById('events');
    if (d.events && d.events.length) {
      tb.innerHTML = d.events.slice().reverse().map(e =>
        '<tr><td>' + e.time + '</td><td>' + e.state + '</td><td>' +
        e.current_a.toFixed(2) + '</td><td>' + e.temp_c.toFixed(1) +
        '</td><td>' + (e.bus_v != null ? e.bus_v.toFixed(2) : '--') +
        '</td><td style="text-align:left">' + (e.ai_summary || '--') + '</td></tr>').join('');
    }
  } catch (e) { /* 忽略瞬时错误，下个周期重试 */ }
  refreshAi();
}

const AI_LEVEL_COLORS = { '低': '#2ecc71', '中': '#f39c12', '高': '#e74c3c' };

async function refreshAi() {
  try {
    const r = await fetch('/api/ai');
    const d = await r.json();
    const lv = document.getElementById('ai-level');
    lv.textContent = d.level || '未评级';
    lv.style.background = AI_LEVEL_COLORS[d.level] || '#95a5a6';
    document.getElementById('ai-text').textContent = d.text || '--';
    document.getElementById('ai-time').textContent = d.ts
      ? new Date(d.ts * 1000).toLocaleTimeString() + ' 评估' : '尚未评估';
  } catch (e) { /* 忽略 */ }
}

async function askRisk() {
  document.getElementById('ai-text').textContent = '评估中…';
  try {
    const r = await fetch('/api/ai', { method: 'POST' });
    await r.json();
    refreshAi();
  } catch (e) {
    document.getElementById('ai-text').textContent = '评估失败：' + e;
  }
}

async function ask() {
  const input = document.getElementById('ask-input');
  const out = document.getElementById('ask-answer');
  const q = input.value.trim();
  if (!q) return;
  out.textContent = '思考中…';
  try {
    const r = await fetch('/api/ask', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ question: q }),
    });
    const d = await r.json();
    out.textContent = d.ok ? d.answer : ('⚠ ' + d.error);
  } catch (e) {
    out.textContent = '⚠ 请求失败：' + e;
  }
}
setInterval(tick, 2000); tick();
</script>
</body>
</html>
"""
PAGE_BYTES = PAGE.encode("utf-8")


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
                    "samples": list(SAMPLES)[-SAMPLE_LIMIT:],
                    "events": list(ALARMS)[-EVENT_LIMIT:],
                    "state_colors": STATE_COLORS,
                }
            body = json.dumps(payload).encode("utf-8")
            self._send(200, body, "application/json; charset=utf-8")
        elif self.path.startswith("/api/ai"):
            with LOCK:
                payload = dict(AI_RISK)
            body = json.dumps(payload).encode("utf-8")
            self._send(200, body, "application/json; charset=utf-8")
        elif self.path.startswith("/api/events.csv"):
            with LOCK:
                rows = list(ALARMS)
            lines = ["time,state,current_a,temp_c,bus_v,ai_summary"]
            for e in rows:
                ai = (e.get("ai_summary") or "").replace('"', "'").replace("\n", " ")
                lines.append("%s,%s,%.2f,%.1f,%.2f,\"%s\"" % (
                    e.get("time", ""), e.get("state", ""),
                    e.get("current_a", 0.0), e.get("temp_c", 0.0),
                    e.get("bus_v", 0.0), ai))
            body = ("\ufeff" + "\n".join(lines) + "\n").encode("utf-8")  # BOM 便于 Excel 打开
            self._send(200, body, "text/csv; charset=utf-8")
        elif self.path.startswith("/api/"):
            self._send(404, b'{"error":"not found"}', "application/json")
        else:
            self._send(200, PAGE_BYTES, "text/html; charset=utf-8")

    def do_POST(self) -> None:  # noqa: N802
        if self.path.startswith("/api/ai"):
            result = ai_risk_assess()
            self._send(200, json.dumps(result).encode("utf-8"),
                       "application/json; charset=utf-8")
            return

        if self.path.startswith("/api/ask"):
            length = int(self.headers.get("Content-Length", 0) or 0)
            raw = self.rfile.read(length) if 0 < length <= MAX_BODY_BYTES else b"{}"
            try:
                payload = json.loads(raw.decode("utf-8", "replace"))
                question = str(payload.get("question", ""))
            except (json.JSONDecodeError, ValueError):
                self._send(400, b'{"ok":false,"error":"bad json"}', "application/json")
                return
            result = ai_ask(question)
            code = 200 if result.get("ok") else 400
            self._send(code, json.dumps(result, ensure_ascii=False).encode("utf-8"),
                       "application/json; charset=utf-8")
            return

        if not self.path.startswith("/api/data"):
            self._send(404, b'{"error":"not found"}', "application/json")
            return

        length = int(self.headers.get("Content-Length", 0) or 0)
        if length <= 0:
            self._send(400, b'{"error":"empty body"}', "application/json")
            return
        if length > MAX_BODY_BYTES:
            self._send(413, b'{"error":"payload too large"}', "application/json")
            return

        raw = self.rfile.read(length)
        try:
            sample = json.loads(raw.decode("utf-8", "replace"))
            if not isinstance(sample, dict):
                raise ValueError("object required")
        except (json.JSONDecodeError, ValueError):
            self._send(400, b'{"error":"bad json"}', "application/json")
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
            "bus_v": 24.1,
            "state": state,
            "alarm": state in ("ALARM", "SHUTDOWN"),
        })
        time.sleep(2)


def main() -> None:
    global DATA_DIR, EVENTS_FILE

    parser = argparse.ArgumentParser(description="火眼上位机监控台")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址（默认 0.0.0.0）")
    parser.add_argument("--port", type=int, default=8080, help="监听端口（默认 8080）")
    parser.add_argument("--demo", action="store_true", help="使用模拟数据自测界面")
    parser.add_argument("--data-dir", default=str(DATA_DIR),
                        help="报警数据留存目录（默认 tools/data）")
    parser.add_argument("--no-mimo", action="store_true", help="关闭 MiMo 云端 AI 研判")
    parser.add_argument("--mimo-model", default=None, help="MiMo 模型名（默认 mimo-v2.6-pro）")
    args = parser.parse_args()

    DATA_DIR = Path(args.data_dir).expanduser()
    EVENTS_FILE = DATA_DIR / "fireeye_events.jsonl"
    restored = load_history()

    global MIMO_MODEL, MIMO_ENABLED
    if args.mimo_model:
        MIMO_MODEL = args.mimo_model
    if args.no_mimo:
        MIMO_ENABLED = False
    print("报警数据留存文件：", EVENTS_FILE)
    print("MiMo 云端 AI 研判：", ("已启用（模型 %s）" % MIMO_MODEL)
          if (MIMO_ENABLED and mimo_key()) else "未启用（没有 key 或已用 --no-mimo 关闭）")
    if restored:
        print(f"已从磁盘读回 {restored} 条历史事件（上位机重启也不丢）")

    if args.demo:
        threading.Thread(target=demo_loop, daemon=True).start()
        print("[demo] 已启动模拟数据源")

    # 运行时 AI：周期性风险评估（无 key 时线程空转，不影响主链路）
    threading.Thread(target=ai_risk_loop, daemon=True).start()
    if MIMO_ENABLED and mimo_key():
        print("AI 实时风险评估：已启用（每 %d 秒一次）" % RISK_INTERVAL_S)
        print("AI 问答：          页面输入框直接提问")
    else:
        print("AI 实时风险评估：未启用（没有 key 或已用 --no-mimo 关闭）")

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"火眼上位机已启动： http://127.0.0.1:{args.port}")
    print("终端上报地址：     http://<本机IP>:%d/api/data" % args.port)
    print("事件导出地址：     http://127.0.0.1:%d/api/events.csv" % args.port)
    print("按 Ctrl+C 退出")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已退出")


if __name__ == "__main__":
    main()
