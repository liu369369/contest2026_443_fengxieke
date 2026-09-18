# openvela 公共仓适配补丁（GD32F407VKT6）

本目录保存火眼项目为跑通 GD32F407 板卡所做的**公共仓改动**，以补丁形式留档，
避免这些改动只存在于本地构建树、`repo sync` 之后丢失。

导出时间：**2026-09-18 18:20**（继电器极性修定后重导；上一版 2026-09-16）
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

## 串口静音改动说明（2026-09-18 新增）

现象：程序运行时串口被刷屏，几乎看不到关键事件，nsh 输入也被打断。

根因：板级 `gd32f4xx_w5500.c` 的轮询线程里有两处周期性诊断输出——

1. 每 10 秒 dump 一行芯片寄存器（VERR/PHY/MR/SR/RSR/MAC…），一行一百多字符；
2. 中断计数在 `count <= 5 || count % 500 == 0` 时打印，而轮询线程每 10ms 会主动
   调一次 ISR，等于每 5 秒就打印一行。

处理：新增编译开关 `W5500_VERBOSE_DIAG`（**默认 0，关闭**），把上述两处诊断输出
以及只被它们使用的 `w5500_dbg_read16()` 一起收进开关内。接线排查时把它改成 1
重新编译即可恢复原有诊断。MAC 看门狗逻辑本身保留不变（只在 MAC 丢失时告警）。

配套的应用侧改动在专属仓：`fireeye` 支持 `-q/-v/-vv` 三档输出（默认只留告警与错误，
`-v` 打开周期数据行，`-vv` 全开），实现方式是 `setlogmask()`——flat build 下 syslog
掩码是全局的，因此这一处设置同时收敛了应用与驱动的 INFO 输出。

## 继电器触发极性与上电默认状态（2026-09-18 修定）

现象：负载接在 COM+NC 上时，不运行 `fireeye` 风扇转；运行后 NORMAL 反而停转，
同时电流读数长期为负（例如 -1.05A）。

根因：本机所用继电器模块是**高电平触发**（HIGH 吸合），板级宏却按低电平处理，
"吸合/释放"整体反相；连带后果是开机零点校准那一步以为负载已断开，实际负载仍在
工作，负载电流被算进零点，之后所有读数都带上这个偏移。

定标方法（决定性，不依赖模块标称极性）：**负载接 COM+NC、不运行 fireeye**
（此时 PB0 由板级 bring-up 驱动为高电平）：

| 接法 | 观察 | 结论 |
|---|---|---|
| IN 悬空 | 继电器释放、NC 导通、风扇转 | 悬空不能导通光耦，任何极性都是释放态 |
| IN 接 PB0 (HIGH) | 继电器吸合、NC 断开、风扇停 | HIGH 是有效电平，即高电平触发 |

处理（`gd32f4xx_fireeye_hw.c`）：

1. `FIREEYE_RELAY_ACTIVE_LOW` 改为 **0**（高电平触发）；
2. bring-up 默认输出由释放改为**吸合**：上电即断开负载，只有 fireeye 应用进入
   NORMAL 才恢复供电（"上电不转、运行才转"，同时等于"未监测不给充电回路通电"）；
3. 早期记录的"低电平触发"是用负电流读数反推的，属循环论证，已作废。

注意：IN 悬空 = 释放 = 负载通电，IN 线脱落会失去断电能力；要"掉线也断电"，
在模块 IN 与 5V 之间加 10kΩ 上拉（先确认能可靠吸合，必要时 4.7kΩ）。

继电器更换、或模块 HIGH/LOW 跳线位置改变后，都要用上表的方法重新定标。

## 存证区修正（2026-09-18 夜，定版：单块 128KB @0x08080000）

现象：`nsh> fireeye store` 报 `FireEye storage: write failed (-5)`（EIO），
历史记录一条也写不进去。

定位过程（每一步都有上板实测）：

1. defconfig 里 `CONFIG_ARCH_CHIP_GD32F407VG`（1MB）与 `FLASH_CONFIG_K`（3MB）
   自相矛盾；芯片实测 `FMC_SIZE=3072KB`，是 3MB，错的是前者；
2. 0x08100000（原区域）编程无效：`up_progmem_write()` 返回 EIO；板级
   `fireeye flash` 证明 FMC 通路本身正常（0x08080000 的字节写/字写都读回一致）；
3. 改用 `FLASH_CONFIG_G` 时踩到兜底判断漏了 G 的坑：`FLASH_CONFIG_DEFAULT`
   被自动定义，区域落到 0x08040000（正好压在 321KB 镜像中间），写入必失败、
   写满擦除还会擦掉固件；
4. 改用 0x080C0000 仍然 EIO ⇒ 该扇区不可写；
5. 改用 0x08080000–0x080BFFFF（2 块）后，block 0 因调试残迹被跳过、指针落到
   block 1（0x080A0000）—— 仍然 EIO ⇒ 0x080A0000 也不可写。

定版方案：

1. `gd32f4xx_progmem.c` 的 GD32F407 分支：progmem 区域 = **单个 128KB 扇区
   0x08080000–0x0809FFFF**（NUM=1）—— 这颗片子上唯一实测可写的区域；
2. `fireeye_storage.c`：允许单块（`g_blocks < 1`）；扫描区分"空白槽（全 0xFF）"
   与"残迹"，没有有效记录时取第一个还有空白槽的块并从第一条空白槽写起；
3. `fireeye flash` 增加 0x08080000 / 0x080A0000 / 0x080C0000 / 0x08100000
   四个地址的**只读**快照，便于复现排查；
4. 应用侧护栏：存证区 < 0x08080000 时拒绝使用。

验证：编译产物里核对 `up_progmem_getaddress` 常量池 = 0x08080000、页数上限 1、
扇区表 = 1 × 0x20000；上板用 `fireeye store` → 复位 → `fireeye -e 6` 验证。

### 追加：写入校验误判（2026-09-18 深夜，20260918m）

在单块区域上仍然报 `write failed (-5)`，但日志显示：唯一"写成功"的地址其值本来
就没变（同一探针重复写），而所有**真正需要编程**的地址都失败 —— 它们的共同点是
**在写之前被读过**（存证扫描读整片区域、探针快照先读）。

判断：GD32F4 的 flash 数据缓存（ART 加速器）导致"编程后立刻回读命中旧值"，
`up_progmem_write()` 因此把成功的写入误判成 EIO。

处理：`gd32f4xx_progmem.c` 的 `up_progmem_write()` 改为**先全部编程 → 复位
flash/ART 缓存（FMC_WS 的 bit12/bit11，自清位）→ 再统一校验**；板级 `fireeye flash`
增加 `WS(ACR)` 与区域前 64 字节的只读快照，便于复现。

## 决定：演示固件关闭板端存证（2026-09-18 深夜）

结论：这颗 GD32F407VK 上 NuttX 的 flash 编程路径只在极少数地址有效
（0x08080000 能写；同一扇区同一 4KB 页内的 0x08080018、以及 0x080A0000、
0x080C0000、0x08100000 都写不进，重启后扫描确认数据确实没落盘）。属于芯片与
该端口驱动的兼容性问题，不再投入时间排查。

处理：

1. defconfig 增加 `# CONFIG_FIREYEYE_STORAGE is not set` —— 存证代码不再编入
   演示固件，状态变化与 60 秒心跳不再打印 `write failed (-5)`；
2. 应用侧用 `FIREEYE_STORAGE_ON` 统一开关，启动时（-v）打印一行
   `FireEye: local storage disabled in this build`，`fireeye -e` 明确提示该功能
   在本固件中已关闭；
3. 保留 `fireeye flash` 与 `fireeye adc` 两条诊断命令作为可复现的排查证据；
4. 材料口径：板端存证写"已实现设计、受芯片/驱动限制未能在本板跑通"；
   报警事件留痕改由上位机承担（`tools/fireeye_monitor.py` 的 JSONL，已测过）。
