# 火眼——电动自行车充电电气安全监测终端

## 一、作品简介

基于 **openvela** 与 **兆易创新 GD32F470V-START**（板载芯片 GD32F407VKT6，openvela 官方未完整支持的新芯片），做一款部署在小区充电区/楼道充电点的智能监测终端：实时采集充电回路电流与温度，超限即时声光报警并联网上报，报警事件同步留痕到上位机，从电气侧阻断电动自行车充电火灾（漏电监测为后续扩展项）。

亮点：

- **电气侧防控**：电流异常与温升是电池热失控的最直接前兆，比视频识别更本质；
- **端侧离线闭环**：感知、判断、报警与联动断电全程在板端完成，断网仍可声光报警与断电保护；
- **新硬件适配**：将 openvela 移植到官方未支持的 GD32F407 芯片，完成编译、烧录与时钟/外设适配，作为大赛"新硬件适配"方向的成果；
- **云端 AI 研判**：上位机在报警事件发生时调用大赛 MiMo 模型（OpenAI 兼容接口），生成可能原因与处置建议，显示在监控页面并写入事件记录（调用失败自动降级，不影响板端保护）。

## 二、选题方向

**AI 硬件产品创新 + 新硬件适配**。

在 openvela 官方已适配的 GD32F470V-START 板卡上，实际芯片为 GD32F407。本作品在为其补齐 GD32F407 架构分发（芯片头文件、引脚映射、内存映射、Flash 编程扇区）的基础上开发火眼监测应用：以真实监管痛点切入，是"应用 + 适配"的组合。

## 三、目录结构

- `app/fireeye/` — 火眼应用（滤波/阈值/状态机/主程序），映射到 `packages/demos/contest2026_443_fireeye`
- `app/hello_app/` — 官方样例（保留参考）
- `docs/` — 开发协同指南、F407 适配记录、提交清单
- `logs/liu369369/` — AI Coding 日志
- `README.md` — 本文件

## 四、运行方式

openvela 构建入口为 `build.sh`（或 `source build/envsetup.sh` + `lunch`/`m`）。针对 GD32F407V-START：

```bash
repo init -u https://github.com/open-vela/contest2026_443_fengxieke \
  -b dev-ai-contest-2026 -m contest2026_443_fengxieke.xml
repo sync -c -j8
source build/envsetup.sh
./build.sh vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh --cmake -j8
```

> 当前板卡（GD32F407）晶振为 25MHz、时钟 168MHz；烧录与串口调试见 `docs/adaptation_F407.md`。
>
> 串口控制台为 **USART0（PB6=TX / PB7=RX，115200-8-N-1）**；板上 GD-Link 只提供 JTAG/SWD，
> 没有虚拟串口，需要用 3.3V USB-TTL 接到 JP6 第 16 脚（PB6）/第 13 脚（PB7）/第 1 脚（GND）。
> 完整构建命令（含 openvela 自带 cmake 与 kconfiglib 环境）见 `docs/adaptation_F407.md` 第四节。

## 五、AI Coding 使用说明

- 需求拆解、方案设计、C 代码编写、构建排错与硬件调试全程与 Codex（GPT-5）协作，并以 MiMo Token 辅助调研；
- AI 主要贡献：赛道选题分析、四点申报文档、openvela 环境搭建、GD32F407 适配、固件构建与烧录排错；
- 完整对话日志见 `logs/liu369369/`。

---

队伍：风歇客（刘艳秋、李晓、王阳、龙潇）

*2026 首届 openvela AI 硬件开发者大赛 · contest2026_443_fengxieke*

## 六、已知限制（如实说明）

1. **板端本地存证未跑通。** 代码与设计完整（`app/fireeye/src/fireeye_storage.c`：24 字节定长记录、
   状态变化与 60 秒心跳落盘、环形区块、`fireeye -e` 回放），但这颗 GD32F407VK 上同一扇区、
   同一 4KB 页内只有个别地址可编程（0x08080000 可写，0x08080018 写不进；重启后重新扫描确认
   数据未落盘），判定为芯片与 openvla/NuttX gd32f4 端口 flash 驱动的兼容性问题。
   演示固件已关闭该功能（`CONFIG_FIREYEYE_STORAGE` 不使能）。**报警事件留痕改由上位机承担**：
   `tools/fireeye_monitor.py` 会把事件与报警前后 30 条采样写入 `tools/data/fireeye_events.jsonl`。
2. **漏电监测未实现。** 板上没有漏电互感器，`check_leakage_sensor()` 恒返回 false，属扩展项。
3. **断网补传、RTC / NTP 对时未实现。** 上报事件的时间戳取自系统运行时间。
4. **电流读数的量级与误差。** ACS712-30A 经 10k/10k 分压后 1 个 ADC 码约 0.024 A；负载为 5 V
   风扇（铭牌 0.15 A），实测读数在 0.1~0.2 A 量级，其中含继电器线圈耦合带来的约 0.1 A 残余偏移。
   过流保护演示采用**等效电压注入**模拟大电流工况（报告中已如实标注），不是真实大电流。
5. **W5500 联网上报。** 09-15 修复版固件已实测通过（串口出现 `W5500: eth0 MAC = 02:00:00:46:49:52`，
   数据可上报到上位机 192.168.1.100:8080）。09-18 调试中曾出现一次芯片读回全零
   （`MAC=00:00:00:00:00:00`）的接线/供电问题，属硬件接触层面，不影响修复版固件的结论。
6. **看门狗未实现**（设计项）。

以上都写在报告与仓库文档中，未实现的能力一律标注为扩展项，不做夸大。