# GD32F407 适配记录（openvela）

> 记录把 openvela 跑在大赛提供的开发板（板名 GD32F470V-START / GD32407V-START，
> 板载芯片 GD32F407VKT6）上的过程、结论与遗留问题。
> 资料依据：官方原理图 `GD32407V-START-V1.1.pdf`、官方数据手册 `GD32F407xx_Datasheet_Rev3.0.pdf`。

## 一、结论摘要（2026-09-11 更新）

- **芯片**：原理图 U1 = **GD32F407VKT6**（LQFP100）。按数据手册表 2-1：
  代码区 512KB + 数据区 2560KB（合计 3072KB），SRAM 192KB，Cortex-M4F，最高 168MHz。
  > 注意"512KB"只指代码区，不是整片容量。
- **晶振**：主 MCU 的系统晶振 Y1 = **25MHz**；原理图上那颗 8MHz 的 Y101 属于板上
  GD-Link 的 GD32F103C8T6，不是主芯片。
- **时钟**：已配置为 **168MHz**：`PLLPSC=25 / PLLN=336 / PLLP=2 / PLLQ=7`
  → VCO 336MHz、SYSCLK 168MHz、USB/SDIO/RNG 48MHz。
  数据手册要求 fPLLIN ∈ [1,4]MHz、fVCO ∈ [64,500]MHz，该组配置均满足。
- **串口控制台**：**USART0 = PB6(TX) / PB7(RX)**（AF7），115200-8-N-1。
  这块板的 GD-Link 只引出了 JTAG/SWD，**没有虚拟串口**，必须外接 3.3V USB-TTL；
  PB6/PB7 引到 JP6 排针：**PB7 = 第 13 脚、PB6 = 第 16 脚**，JP6 第 1 脚是 GND。
- **固件状态**：2026-09-11 用干净构建目录 `out3` 重新生成配置并编译通过，
  `out3/nuttx.hex` / `out3/nuttx.bin` 已产出，RCU_PLL 常量实测为 `0x07405419`。
- **实测结果（2026-09-11）**：烧录修正后的固件，外接 3.3V USB-TTL 到 JP6 第 16/13 脚后，
  串口正常输出 `NuttShell (NSH)` 与 `nsh>` 提示符，`fireeye` 应用可运行。
  至此串口控制台打通，时钟配置错误的根因得到实机验证。

## 二、适配内容（代码层面）

### 2.1 nuttx 公共仓（`nuttx`，本地已改，尚未提 PR）

openvela/nuttx 的 Kconfig 里本来就有 `ARCH_CHIP_GD32F407VG/ZK`，但芯片头文件分发漏了 F407，
只有补上才能编译：

1. `arch/arm/include/gd32f4/chip.h`：把 `CONFIG_ARCH_CHIP_GD32F407VG` 纳入能力分发组
   （外设数量与 GD32F450/F470 一致）。
2. `arch/arm/src/gd32f4/hardware/gd32f4xx_memorymap.h`：F407 复用 `gd32f450_memorymap.h`。
3. `arch/arm/src/gd32f4/hardware/gd32f4xx_pinmap.h`：F407 复用 `gd32f450_pinmap.h`。
4. `arch/arm/src/gd32f4/gd32f4xx_progmem.c`：补 F407 的 Flash 扇区分发。
5. `arch/arm/src/gd32f4/CMakeLists.txt`：**新增文件**（openvela 用 CMake 构建，上游缺这个文件）。

### 2.2 vendor 板级仓（`vendor/gigadevice`，本地已改，尚未提 PR）

`boards/gd32f4/gd32f470v_start/`：

1. `include/board.h`
   - 时钟修正：200/240MHz 分支的 `GD32_PLL_PLLPSC` 恢复为 `25`（原来被改成 8，见第三节）；
     默认 HXTAL 兜底值恢复 25MHz。
   - `GPIO_USART0_RX/TX` = `_3`（PB7/PB6），控制台从 PA9/PA10 改到 PB6/PB7。
   - 新增 I2C1 引脚别名：`GPIO_I2C1_SCL`(PB10)、`GPIO_I2C1_SDA`(PB11)。
2. `configs/nsh/defconfig`
   - `CONFIG_GD32F470V_START_168MHZ=y`、`CONFIG_GD32F470V_START_HXTAL_VALUE=25000000`
   - `CONFIG_USART0_SERIAL_CONSOLE=y`（115200）
   - 去掉 `CONFIG_GD32F4_I2C0`，改为 `CONFIG_GD32F4_I2C1`（避免与串口抢 PB6/PB7）
   - `CONFIG_FIREYEYE=y`（火眼应用）

### 2.3 火眼应用（本专属仓 `app/fireeye`）

- 新增 NuttX 应用（Kconfig / CMakeLists / Makefile），经 manifest `<linkfile>` 映射到
  `packages/demos/contest2026_443_fireeye`。
- 本次把构建树里多出来的 OLED 模块（`src/oled_display.c`、`src/include/oled_display.h`）
  和 `fireeye_main.c` 的 OLED 调用同步回仓库，保证"仓库 = 固件"；OLED 总线由
  `/dev/i2c0` 改为 `/dev/i2c1`。

## 三、关键问题与根因

### 3.1 串口无输出（已定位并修复）

- **现象**：真机串口长期无输出/乱码，一度怀疑是 PA9/PA10 与 PB6/PB7 的引脚走线问题。
- **实机验证**：2026-09-11 烧录修正后的固件，串口立刻输出 NSH 提示符（不是"乱码"也不是"无输出"），
  证明问题确实在被测时钟配置上，而不是 UART 引脚走线。
- **真正根因**：旧固件把 PLL 预分频写成 8，而晶振是 25MHz：
  `VCO = 25MHz / 8 × 400 = 1250MHz`，远超数据手册 500MHz 上限，PLL 根本锁不住；
  启动代码在等待 PLL 就绪标志处死循环，串口自然一个字符都没有。
  旧固件的 RCU_PLL 常量实测为 `0x08406408`（PLLPSC=8/PLLN=400），修复后为 `0x07405419`。
- **另一个坑**：9/10 那次"改好时钟再编译"实际上没生效——`out2/.config` 停留在旧的
  200MHz 配置（文件时间戳还是 9/5），defconfig 里新加的 168MHz 从未进入固件。
  所以本次改用干净的 `out3` 目录重新配置，并核对 `.config` 确为 `..._168MHZ=y`。

### 3.2 openvela 对 F407 的支持并不完整

"芯片层已支持"只对了一半：Kconfig 里有 F407 选项，但 memorymap / pinmap / progmem 的
分发和 CMake 构建文件都要自己补（见 2.1）。板级结构可参考 `nuttx/boards/arm/gd32f4/gd32f450zk-eval`。

### 3.3 PB6/PB7 复用冲突

USART0 与 I2C0 在 PB6/PB7 上硬件复用（数据手册引脚复用表）。
因此启用串口控制台时不能再启用 I2C0；火眼 OLED 改用 I2C1（PB10/PB11）。

### 3.4 板载 IO 与火眼配置的冲突（未解决）

官方板上：**PA0 是用户按键 K2**、**PC6 是板载 LED**，没有蜂鸣器/继电器。
而 `app/fireeye/src/include/fireeye_config.h` 把 PA0 当蜂鸣器、PA1/PA2 当报警灯、PA3 当继电器，
真机接外设时必须重新分配引脚。

### 3.5 串口仍无输出时的板级自检（LED）

2026-09-11 修正时钟后烧录，真机串口**仍然没有任何输出**。此时必须先区分
"芯片没跑起来"和"串口链路有问题"，因此新增板级自检文件
`vendor/gigadevice/boards/gd32f4/gd32f470v_start/src/gd32f4xx_selftest.c`：

- `gd32_selftest_early()`：在 `gd32_boardinitialize()` 中调用（此时 `gd32_clockconfig()`
  已执行完），用板载 LED（PC6）快闪 2 次，证明时钟配置完成、固件没卡在启动阶段；
- `gd32_selftest_start()`：在 `gd32_bringup()` 末尾启动心跳线程，LED 依次给出
  "5 次快闪（应用已起来）→ 1Hz 心跳 → 按住 PA0 按键变 5Hz 快闪（GPIO 输入正常）"；
  心跳周期可用秒表核对，若明显慢于 1Hz 说明实际晶振与 25MHz 配置不符；
- 串口辅助（诊断用，默认已关闭）：心跳线程每秒绕过 NuttX 协议栈、直接写 USART0 寄存器
  发送 `FIREYE\r\n`；启动时做一次回环自检（临时关闭接收中断约 0.4 秒，发 4 行数据并轮询接收），
  若把 PB6/PB7 短接且能收全数据，则 LED 再极快闪 10 次，表示 MCU 侧串口通路正常。
  这段代码会与控制台驱动争用 USART0，因此由 `FIREYE_SELFTEST_UART` 宏控制，
  串口打通后已置 0（编译时被裁掉）；LED 心跳部分保留。

据此可快速判读：LED 完全不亮 → 时钟/启动失败；LED 有正常心跳但串口无输出 →
问题在接线/USB-TTL/引脚，而非固件。

### 3.6 应用层硬件接入（P1，2026-09-11）

板级新增 `gd32f4xx_fireeye_hw.c`，向应用提供 ADC 采样、报警输出、按键和 I2C1 注册：

- **ADC**：openvela/nuttx 没有 GD32F4 的 ADC 驱动与寄存器头文件，按《GD32F4xx User Manual Rev3.4》
  第 14 章实现：ADC0、12 位、`ADCCK = PCLK2/4 = 21MHz`（手册上限 40MHz）、480 周期采样时间、
  前台校准（RSTCLB → CLB）、单通道单次转换（RL=0、RSQ0=通道号、SWRCST 触发、读 RDATA 清 EOC）。
  通道分配：PA4 = ADC0_IN4（电流）、PA6 = ADC0_IN6（温度）。
- **换算**：ACS712-30A（66mV/A，5V 供电）输出经 10kΩ+10kΩ 分压后进 PA4，
  分压后零点约 1.25V、灵敏度 33mV/A，上电做一次零点校准；
  温度用 10kΩ + NTC(10K B3950) 分压，按 B 参数方程换算。
- **输出**：PB0 继电器（默认低电平触发，宏 `FIREEYE_RELAY_ACTIVE_LOW` 可改），
  PB1 蜂鸣器（预警 1Hz 间歇 / 报警长鸣），PA0 板载按键做人工复位。
- **I2C1**：板级注册 `/dev/i2c1`（OLED 用），需 `CONFIG_I2C_DRIVER=y`。
  注意 I2C0 与 USART0 控制台共用 PB6/PB7，故 OLED 必须走 I2C1。
- **OLED 上机踩到并修掉的两个驱动 bug（2026-09-11 实机验证 `OLED init OK`）**：
  1. 初始化命令缓冲区只有 16 字节却按 `n+1=25` 字节发送：既越界读栈，又把命令截断，
     导致收尾的 `0x8D 0x14`（电荷泵使能）从未发出 —— SSD1306 不开电荷泵，屏幕必然全黑。
     改为按 15 条一包循环发送；命令表补全 `0xAF`（display on）。
  2. `struct i2c_msg_s` 是未初始化栈变量，`frequency` 字段从未赋值，而 GD32 的 I2C 驱动
     用 `gd32_i2c_setclock(priv, msgs->frequency)` 计算 SCK 分频 —— 于是时钟是随机值，
     器件不应答（现象即 `OLED init FAILED`）。现显式设为 100kHz。
  另外增加了 0x3D 地址自动重试、刷新前显式设置显示窗口、`i2c` 命令行工具（信号扫描）。
- 接线表见 `docs/开发板接线.md`。

#### P1 实机验证结果（2026-09-12）

在 GD32F407VKT6 开发板上逐项实测通过：

| 项目 | 验证方式 | 结果 |
|---|---|---|
| 串口控制台 | USB-TTL 接 JP6-16/13，115200 | 正常输出 NSH 提示符 |
| OLED（SSD1306，I2C1） | 板级注册 /dev/i2c1 | `OLED init OK`，正常显示状态/电流/温度 |
| 温度采集（NTC 10K B3950） | 10kΩ 分压中点接 PA6 | 室温读数正常（分压 1.65V 附近） |
| 电流采集（ACS712-30A） | 输出经 10k+10k 分压接 PA4 | 上电零点校准正常 |
| 报警灯模块（PD9） | `fireeye test` | 亮灭受控，预警慢闪/报警常亮 |
| 蜂鸣器（PB1） | `fireeye test` | 短鸣/长鸣/间歇音型均正常 |
| 继电器联动（PB0） | `fireeye test` + 万用表量 COM-NC | 吸合释放正常（可实现报警断电） |
| 按键复位（PA0） | 按下板载 K2 | 状态机回 NORMAL，报警复位 |

调试期间在实机上定位并修复的驱动问题（**都是软件问题，不是接线问题**）：

1. `%f` 打印成 `*float*` —— NuttX 默认关闭 printf 浮点，需 `CONFIG_LIBC_FLOATINGPOINT=y`；
2. OLED 全黑 —— 初始化命令缓冲区只有 16 字节却按 24 字节发送，越界且丢掉电荷泵命令 `0x8D 0x14`；
3. OLED 不应答 —— `struct i2c_msg_s` 未初始化、`frequency` 为随机值，被 GD32 I2C 驱动用于计算分频；
4. 温度"虚高/误报警" —— 旧逻辑在采样失败时回退模拟数据，并模拟漏电直接把状态机推到 ALARM；
   现已改为：采样无效时保留上次有效值并置无效标记（OLED 显示 `--`），漏电通道恒 false。

## 四、构建与烧录

```bash
cd ~/openvela
export PATH=$PWD/prebuilts/tools/python/bin:$PATH
export PYTHONPATH=$PWD/prebuilts/tools/python/dist-packages/kconfiglib:$PWD/prebuilts/tools/python/dist-packages/pyelftools:$PWD/prebuilts/tools/python/dist-packages/Mako:$PWD/prebuilts/tools/python/dist-packages/ply:$PWD/prebuilts/tools/python/dist-packages/jsonpath:$PWD/prebuilts/tools/python/dist-packages/construct

cmake -B out3 -S nuttx \
  -DBOARD_CONFIG=$PWD/vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh \
  -DCUSTOM_MODULE_PATH=$PWD/build/cmake \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations -Werror" -GNinja
ninja -C out3 nuttx -j8
arm-none-eabi-objcopy -O binary -S out3/nuttx out3/nuttx.bin
arm-none-eabi-objcopy -O ihex    out3/nuttx out3/nuttx.hex
```

> 必须显式加 `PATH` / `PYTHONPATH`（等价于 `source build/envsetup.sh` 的全局段），
> 否则 cmake 会报 `Kconfig environment depends on kconfiglib`。

烧录：GD-Link Utility → Connect →（有写保护先 Full erase）→ 选 `nuttx.hex` → Download → 复位。

串口自检：

```
USB-TTL RX  <-> JP6 第 16 脚 (PB6 / USART0_TX)
USB-TTL TX  <-> JP6 第 13 脚 (PB7 / USART0_RX)
USB-TTL GND <-> JP6 第 1 脚 (GND)
终端：115200-8-N-1，无流控，应看到 nsh> 提示符
```

## 五、遗留问题

1. ~~真机串口未打通~~ **已解决（2026-09-11 实测）**：根因是 PLL 预分频写成 8 导致
   25MHz 晶振算出 1250MHz 的 VCO、PLL 无法锁定；改为 168MHz 标准配置后，
   串口输出 NSH 提示符正常。板级 LED 自检（3.5）保留，便于后续换板/换环境时快速判断
   "芯片是否跑起来"。
2. **芯片口径不统一**：实测芯片是 GD32F407VK（代码区 512KB + 数据区 2560KB），
   但 nuttx Kconfig 只有 VG / ZK 选项，当前用 `ARCH_CHIP_GD32F407VG` +
   `CONFIG_GD32F4_FLASH_CONFIG_K` 组合，属于将就用法；要规范化需要改公共仓 Kconfig 并单独提 PR。
3. **公共仓改动尚未提交**：nuttx 与 vendor/gigadevice 的改动目前只存在于本地 WSL 树，
   需要在对应仓库建分支、推 fork、提 PR，否则 `repo sync` 之后会丢。
4. ~~传感器与执行器未接~~ **P1 已完成（2026-09-11）**：电流（ACS712-30A 走 PA4）与
   温度（NTC 10K B3950 走 PA6）已接真实 ADC，蜂鸣器（PB1）/继电器（PB0）/按键（PA0）已接入；
   漏电监测仍未做（需要漏电互感器与调理电路）。
5. ~~OLED 总线未注册~~ **已注册（2026-09-11）**：板级注册 `/dev/i2c1`（需 `CONFIG_I2C_DRIVER=y`），
   OLED 可正常打开；未插屏时写失败不影响其他功能。
6. **AI Coding 日志**：`logs/liu369369/` 目前仍是占位内容，需要在提交前导出真实会话日志。
