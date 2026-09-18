# 火眼应用 - FireEye Application

电动自行车充电电气安全监测终端应用代码

## 目录结构

```
fireeye/
├── src/
│   ├── include/
│   │   ├── fireeye_config.h    # 配置参数
│   │   ├── fireeye_filter.h    # 滤波器头文件
│   │   ├── fireeye_threshold.h # 阈值判定头文件
│   │   ├── fireeye_fsm.h       # 状态机头文件
│   │   └── oled_display.h      # OLED 显示头文件
│   ├── fireeye_main.c          # 主应用程序
│   ├── fireeye_filter.c        # 滤波器实现
│   ├── fireeye_threshold.c     # 阈值判定实现
│   ├── fireeye_fsm.c           # 状态机实现
│   ├── fireeye_sensors.c       # 真实传感器采样与报警输出
│   └── oled_display.c          # SSD1306 OLED 显示实现（I2C1）
├── Makefile                    # NuttX Makefile
├── Make.defs                   # NuttX Make.defs
├── Kconfig                     # NuttX Kconfig
└── README.md                   # 本文件
```

## 模块说明

### 1. 配置模块 (fireeye_config.h)

所有可配置参数的集中定义，包括：
- 采样率
- 滤波参数
- 阈值参数
- 任务优先级
- GPIO引脚定义
- ADC通道定义

### 2. 滤波器模块 (fireeye_filter.c/h)

数据滤波处理，包含：
- **滑动平均滤波**：去除高频噪声
- **一阶低通滤波**：平滑数据
- **组合滤波**：先滑动平均再低通

### 3. 阈值判定模块 (fireeye_threshold.c/h)

阈值检测和判定，包含：
- **单阈值判断**：超过预警/报警阈值
- **趋势分析**：检测异常上升斜率
- **防抖机制**：连续N次超限才触发

### 4. 状态机模块 (fireeye_fsm.c/h)

系统状态管理，包含：
- **状态转换**：正常 → 预警 → 报警 → 断电
- **事件处理**：处理各种系统事件
- **复位功能**：支持人工复位

### 5. 主应用 (fireeye_main.c)

应用程序入口，负责：
- 初始化所有模块
- 主监控循环
- 传感器数据采集
- 数据处理和判定
- 告警和控制

### 6. 传感器与执行器 (fireeye_sensors.c/h)

- 电流：ACS712-30A（66mV/A，5V 供电）+ 10k/10k 分压 → PA4（ADC0_IN4），上电自动零点校准；
- 温度：10kΩ + NTC(10K B3950) 分压 → PA6（ADC0_IN6），B 参数方程换算；
- 输出：PB1 蜂鸣器、PB0 继电器（联动断电），PA0 板载按键做人工复位；
- 底层 ADC/GPIO 由板级 `gd32f4xx_fireeye_hw.c` 提供（openvela 无 GD32F4 ADC 驱动，按用户手册寄存器实现）。

### 7. OLED 显示模块 (oled_display.c/h)

0.96" SSD1306（128x64，地址 0x3C），通过 `I2CIOC_TRANSFER` 访问 NuttX I2C 字符设备。

- 总线：**I2C1（`/dev/i2c1`）**。不用 I2C0 是因为 I2C0 的 SCL/SDA 与 USART0 控制台的
  PB6/PB7 硬件复用，两者不能同时使用；
- 现状：板级尚未注册 I2C1 设备节点，`oled_init()` 会返回 `-ENODEV`，
  主任务忽略该错误继续运行，因此不影响串口控制台与告警逻辑。

## 编译配置

在 `menuconfig` 中启用火眼应用：

```
Application Configuration  --->
  FireEye Electric Bicycle Charging Safety Monitor  --->
    [*] FireEye Electric Bicycle Charging Safety Monitor
    (200) FireEye main task priority
    (4096) FireEye main task stack size
    (10) Sensor sample rate (Hz)
    (5.0) Current warning threshold (A)
    (8.0) Current alarm threshold (A)
    (45.0) Temperature warning threshold (C)
    (60.0) Temperature alarm threshold (C)
    (3) Debounce count
```

## 编译和运行

```bash
# 在 openvela 工作区根目录
./build.sh <board-config-path> [-j8]

# 烧录固件后，在 NuttX shell 中运行
nsh> fireeye
```

## 待实现功能

当前代码使用模拟数据，需要实现以下实际功能：

### 驱动层
- [ ] ADC驱动 - 读取电流/温度传感器
- [ ] GPIO驱动 - 控制蜂鸣器和LED
- [ ] SPI驱动 - OLED显示和W5500网络
- [ ] I2C驱动 - 可选的I2C传感器

### 服务层
- [ ] OLED显示 - 实时数据显示
- [ ] 网络上报 - MQTT数据上报
- [ ] 数据存储 - Flash/SD卡存储

### 应用层
- [ ] 用户界面 - 按键交互
- [ ] 配置管理 - 参数持久化
- [ ] 固件升级 - OTA支持

### 已完成
- [x] OLED 显示驱动（SSD1306，I2C1；待板级注册 I2C1 总线后即可点亮）

## 硬件接口

| 功能 | 器件 | 接口 | 引脚 |
|-----|------|------|------|
| 电流采集 | ACS712 | ADC | PA4（ADC0_IN4） |
| 温度采集 | NTC | ADC | PA6（ADC0_IN6） |
| 漏电检测 | 漏电互感器 | ADC | 扩展项，当前未实现 |
| 声报警 | 蜂鸣器 | GPIO | PB1 |
| 光报警 | 报警灯 | GPIO | PD9 |
| 板载 LED | 板载心跳 LED | GPIO | PC6 |
| 手动复位按键 | 按键 | GPIO | PA0 |
| 继电器 | 继电器模块 | GPIO | PB0 |
| 显示 | OLED 0.96" SSD1306 | I2C1 | PB10(SCL)/PB11(SDA) |
| 网络 | W5500 | SPI | SPI1 |

> 串口控制台占用 USART0：PB6(TX)/PB7(RX)，115200-8-N-1，需外接 3.3V USB-TTL
> （板上 GD-Link 没有虚拟串口）。

## 调试建议

1. **使用syslog**：所有关键路径都有syslog输出
2. **模拟数据**：当前使用模拟数据，便于调试
3. **逐步集成**：先验证算法，再集成驱动
4. **单元测试**：为每个模块编写测试用例

## 参考文档

- `docs/architecture_design.md` - 架构设计文档
- `docs/development_guide.md` - 开发协同指南
- `app/fireeye_prototype/` - PC端算法原型

---

*最后更新: 2026-09-03*
