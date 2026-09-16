# openvela 公共仓适配补丁（GD32F407VKT6）

本目录保存火眼项目为跑通 GD32F407 板卡所做的**公共仓改动**，以补丁形式留档，
避免这些改动只存在于本地构建树、`repo sync` 之后丢失。

导出时间：**2026-09-16**（上一版 2026-09-11）
基线：

- `nuttx`：`dd92bcf425738734d1b8aed09c2bd4dbe3f2e438`（2026-07-10）
- `vendor/gigadevice`：`838aae8ecb474675e1f350528909ddedc7ac741a`（2026-06-09）

## 文件

| 文件 | 对应仓库 | 内容 |
|---|---|---|
| `nuttx-gd32f407-support.patch` | `nuttx` | chip.h / memorymap.h / pinmap.h / progmem.c 的 F407 分发，新增 `arch/arm/src/gd32f4/CMakeLists.txt`；`drivers/net/w5500.c` 在轮询模式下把中断空转日志由 warn 降为 info |
| `vendor-gigadevice-gd32f407v-start.patch` | `vendor/gigadevice` | 板级适配：时钟与串口（USART0 = PB6/PB7）、I2C1 引脚别名、nsh defconfig（168MHz / 25MHz 晶振 / 串口控制台 / I2C1 / 火眼应用 / W5500 的 MAC 配置）、火眼硬件驱动（ADC 寄存器级实现、蜂鸣器 / 继电器 / 报警灯 / 按键）、W5500 以太网板级适配（SPI1 + INT + 轮询兜底 + ifup 前写入 MAC）、板级自检线程 |

## 应用方法

```bash
# nuttx
cd <workspace>/nuttx
git apply /path/to/patches/nuttx-gd32f407-support.patch

# vendor/gigadevice
cd <workspace>/vendor/gigadevice
git apply /path/to/patches/vendor-gigadevice-gd32f407v-start.patch
```

> `nuttx` 补丁里包含一个**新增文件** `arch/arm/src/gd32f4/CMakeLists.txt`，
> 若用 `git apply` 失败，可先 `git apply -p1 --directory=. <patch>` 或直接用 `patch -p1`。

## W5500 相关改动说明（2026-09-15/16 新增）

现象：PHY 链路正常（PHYCFGR=0xBF）、SPI 正常（VERR=0x04），但 `Sn_RX_RSR` 恒为 0，
收不到任何数据。

根因：NuttX 的 W5500 驱动 `w5500_unfence()`（每次 ifup 执行）把 `netdev->d_mac`
原样写进芯片 SHAR，而 `d_mac` 初始全零、只有 `netlib_setmacaddr` 才会赋值；
本板 NSH 启动时 netinit 先 ifup 且不设 MAC，导致 SHAR=00:00:00:00:00:00
配上 `Sn_MR` 的 MAC 过滤（MFEN=1），芯片把收到的帧全部丢弃。

修复（两处，互为兜底）：

1. `defconfig` 增加 `CONFIG_NETINIT_NOMAC=y` / `NETINIT_SWMAC=y` /
   `NETINIT_MACADDR_1=0x00464952` / `NETINIT_MACADDR_2=0x0200`，
   让 netinit 在 ifup 之前写入 MAC `02:00:00:46:49:52`；
2. 板级 `gd32f4xx_w5500.c` 在注册网卡之后、任何 ifup 之前把 MAC 写进 `netdev->d_mac`，
   并修正轮询线程里 MAC 看门狗与寄存器 dump 共用同一个计数器导致看门狗永不触发的问题。

## 重要提醒

这两个仓库属于**公共仓**。按大赛规则，公共仓改动不能随专属仓 PR 一起提交，需要：

1. fork 公共仓（nuttx / vendor_gigadevice）；
2. 在 `dev-ai-contest-2026` 基础上建分支并提交本补丁内容；
3. 向组委会仓库发 PR 并说明用途（GD32F407 芯片分发 + gd32f470v_start 板级适配）。

在公共仓 PR 合入之前，本仓的构建依赖上面这两处本地改动。
