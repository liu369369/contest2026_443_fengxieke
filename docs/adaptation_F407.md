# GD32F407 适配记录（openvela）

> 本文档记录将 openvela 移植到大赛所提供的 GD32F470V-START 开发板（板载芯片为 GD32F407）的过程、成果与未解决问题。

## 一、结论摘要

- 已实现：openvela 在 **GD32F407** 上**编译通过**、**成功烧录**（GD-Link，`Programming and Verification Successfully!`），火眼应用已编入固件（`strings nuttx | grep fireeye → 39`）。
- 已确认：芯片为 **GD32F407VKT6**；系统晶振 **25MHz**（板载另一颗 8MHz 给 GD-Link 的 GD103 使用）；正确时钟为 **168MHz（PLLM=25/PLLN=336/PLLP=2/PLLQ=7）**，与官方 Demo `system_gd32f4xx.c` 的 `__SYSTEM_CLOCK_168M_PLL_25M_HXTAL` 一致。
- 待解决：真机串口控制台暂未稳定输出（排查过程中串口呈乱码/无输出），怀疑为板级 UART 引脚/走线或驱动细节与官方标准存在差异，需原理图或示波器进一步确认。

## 二、适配内容（代码层面）

为支持 GD32F407，在 openvela 的 gd32f4 架构中补齐了 F407 的分发：

1. `arch/arm/include/gd32f4/chip.h`：将 `CONFIG_ARCH_CHIP_GD32F407VG` 纳入芯片能力分发（外设数量与 GD32F450/F470 一致）。
2. `arch/arm/src/gd32f4/hardware/gd32f4xx_pinmap.h` / `gd32f4xx_memorymap.h`：为 `CONFIG_GD32F4_GD32F407` 增加引脚/内存映射（复用 gd32f450 表）。
3. `arch/arm/src/gd32f4/gd32f4xx_progmem.c`：为 `CONFIG_GD32F4_GD32F407` 增加 Flash 编程扇区定义（4×16KB @ 0x08100000）。
4. 板级 `vendor/gigadevice/.../gd32f470v_start/include/board.h`：时钟按 25MHz 晶振配置为 168MHz（PLLM=25/PLLN=336/PLLP=2/PLLQ=7），对应 `CONFIG_GD32F470V_START_168MHZ`。
5. `app/fireeye`：新增 NuttX 应用（Kconfig/CMakeLists/Makefile），经 manifest `<linkfile>` 映射到 `packages/demos/contest2026_443_fireeye`。

## 三、关键发现

- 板卡名义为 GD32F470V-START，但实际芯片为 **GD32F407VKT6**（GD-Link 识别 + 丝印），openvela 官方 gd32f4 分支未完整支持 F407，需如上适配。
- 板级 `board.h` 默认晶振为 25MHz，与板上 `25.000` 晶振（GD32F407 使用）一致；`8.000` 晶振为板上 GD-Link（GD103）使用。
- 正确 168MHz 时钟配置与官方 Demo `system_clock_168m_25m_hxtal` 完全一致。

## 四、构建与验证流程

```bash
cd ~/openvela
source build/envsetup.sh
cmake -B out -S nuttx \
  -DBOARD_CONFIG=$PWD/vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/nsh \
  -DCUSTOM_MODULE_PATH=$PWD/build/cmake -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations -Werror" -GNinja
ninja -C out nuttx -j8
arm-none-eabi-objcopy -O binary -S out/nuttx out/nuttx.bin
arm-none-eabi-objcopy -O ihex    out/nuttx out/nuttx.hex
```

烧录：GD-Link Utility → Connect →（write-protected 先 Full erase）→ 选 `nuttx.hex` → Download → 复位。

## 五、未解决问题

- 控制台 USART0（PA9/PA10，board.h 定义）在真机暂未稳定输出；需结合板卡原理图确认 USART0 实际引脚/跳线，或用示波器测 PH1（OSC_OUT）确认真实时钟。
- 真机唤醒不影响编译/烧录/应用代码交付；作为"新硬件适配"后续优化点。
