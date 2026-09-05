# 火眼——电动自行车充电电气安全监测终端

## 一、作品简介

基于 **openvela** 与 **兆易创新 GD32F407V-START**（GD32F407，openvela 官方未完整支持的新芯片），做一款部署在小区充电区/楼道充电点的智能监测终端：实时采集充电回路电流、温度与漏电状态，超限即时声光报警、联网上报并本地存证，从电气侧阻断电动自行车充电火灾。

亮点：

- **电气侧防控**：电流异常与温升是电池热失控的最直接前兆，比视频识别更本质；
- **端侧离线闭环**：感知、判断、报警全程本地，断网仍可报警与存证；
- **新硬件适配**：将 openvela 移植到官方未支持的 GD32F407 芯片，完成编译、烧录与时钟/外设适配，作为大赛"新硬件适配"方向的成果。

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

## 五、AI Coding 使用说明

- 需求拆解、方案设计、C 代码编写、构建排错与硬件调试全程与 Codex（GPT-5）协作，并以 MiMo Token 辅助调研；
- AI 主要贡献：赛道选题分析、四点申报文档、openvela 环境搭建、GD32F407 适配、固件构建与烧录排错；
- 完整对话日志见 `logs/liu369369/`。

---

*2026 首届 openvela AI 硬件开发者大赛 · contest2026_443_fengxieke*
