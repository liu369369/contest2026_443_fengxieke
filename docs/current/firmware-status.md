# 当前固件状态

## 一、版本

- 应用版本：`FireEye 2.0.1`
- 状态更新日期：2026-10-31
- 当前构建时间：2026-10-31 13:18
- 构建结果：通过
- HEX：935444 字节，MD5 `82ce08b207ede6f1f3c984a96e430a84`
- BIN：332548 字节，MD5 `5e255edb7cfb6c27fd433d1c92ab5fc3`
- 构建环境：WSL `/home/liu/openvela/out3`
- 继电器目标模式：标准 `COM -> NO -> LOAD`，
  `CONFIG_FIREYEYE_RELAY_WIRED_NC` 保持关闭
- 继电器硬件：1/4脚线圈和Q1驱动正常；已用一根线短接3/5脚，把
  `24V_SW`接到真正的COM5脚，带载通路已验证
- 状态：2.0.1 网络超时修复版已编译，待烧录持续验收；前一版网络收发已上板通过
- 基础带载监测：`I=0.065–0.17A`、`Vbus=0.7–1.6V`、`state=NORMAL`；
  继电器、负载端和INA228基础带载路径已通过
- 网络：W5500 直连电脑时 Ping 和 HTTP 上报已通过；`connect/send/recv`
  超时与连接回收修复待持续上板验收

## 二、本版改动

- 新增 INA228 驱动，默认从 I2C1 地址 `0x40` 读取电流和母线电压。
- 保留 ACS712 后端作为旧演示板兼容选项，通过 Kconfig 选择。
- 新增 `fireeye current` 电流链路自检命令。
- 新增 `fireeye oled` 全屏显示自检命令。
- OLED主页面改为整帧一次刷新，减少重复I2C传输和闪屏；新增状态栏、负载状态、
  双倍字高电流、电压/温度分区和跳闸阈值。
- 移除 INA228/OLED 重复的硬件 I2C reset，修复实时监测中第二次 I2C
  初始化失败的问题；硬件 I2C 接收状态机本身仍未修复。
- INA228 按数据手册图 7-9 使用“写指针 STOP，再独立读”的寄存器事务。
- 当前诊断构建使用 I2C 中断模式，并在应用层通过 PB10/PB11 软件 I2C
  访问 INA228 和 OLED，绕过已确认有故障的 NuttX 硬件 I2C 接收状态机。
- 启动自检会打印软件位翻转、寄存器级硬件 I2C 和 NuttX 驱动的状态日志；
  正式版应移除或收敛这些诊断输出。
- 继电器改为明确的负载接通/断开语义。
- 板级上电默认改为继电器释放、负载断开。
- 蜂鸣器统一为高电平有效。
- 修复滑动平均窗口参数未生效。
- 修复“连续 N 点”防抖实际会衰减计数的问题。
- 趋势判定只使用正斜率，不再把下降趋势当成快速上升。
- 低通滤波器首个样本直接初始化，避免滤波器暖机被误判为温度上升。
- 第一个有效温度样本预填充趋势历史，趋势预警等待完整窗口，并同时要求
  斜率与累计升幅超限，消除启动阶段假预警。
- Kconfig 中的采样率和阈值现在真正传入应用。
- HTTP JSON 增加 `bus_v` 字段。
- 关键传感器连续 3 次失效时强制进入 ALARM 并断开负载，不再沿用旧读数。
- 增加临时 `CONFIG_FIREYEYE_RELAY_WIRED_NC` 选项用于故障排查；新载板实际
  使用 NO，该选项默认关闭，不能作为最终方案。

## 三、独立 I2C 验证

2026-10-25 使用独立 STM32F103C8T6 硬件 I2C2 测试，OLED 和 INA228 同时连接时：

```text
found 0x3C
found 0x40
OLED 0x3C: ACK
INA228 0x40: ACK
MANUFACTURER_ID: 0x5449
DEVICE_ID: 0x2281
```

结论是 OLED、INA228 和共享 I2C 总线正常，GD32 侧问题仍需继续定位。
证据见 [i2c-validation-20261025.md](i2c-validation-20261025.md)。

## 四、GD32 上板验证

2026-10-25 在 GD32F407VK 上实测：

```text
FireEye I2C bitbang idle: SCL=1 SDA=1
FireEye I2C bitbang 0x3c: ACK
FireEye I2C bitbang 0x40: ACK
FireEye I2C hw 0x3c: ACK STAT0=0x0082
FireEye I2C hw 0x40: ACK STAT0=0x0082
```

这证明器件、供电、PB10/PB11 和 AF4 硬件路径正常。随后确认：

- NuttX I2C 轮询模式无法完成地址访问。
- 中断模式可扫描 `0x3C`、`0x40`，OLED 写入正常。
- 中断模式的 INA228 多字节读返回错误数据，并把下一次访问带入 AERR。
- 应用改用软件 I2C 后，`fireeye current` 和 `fireeye oled` 均通过。

当前实测结果：

```text
INA228: mfg=0x5449 id=0x2281 config=0x0010 adc=0xb142
raw20=2 current=0.000A bus=0.01V
OLED test OK
```

未接被测回路时，零电流和接近零的母线电压符合预期。NTC 常温实测示例为
`raw=2073`、`1.670V`、`24.45℃`；超温测试在 `65.5℃` 进入 ALARM，
降至 `44.8℃` 后恢复 NORMAL。

`fireeye test` 已验证蜂鸣器 3 短声、预警间歇音和报警长鸣，报警灯亮灭正常。
后续逐脚测量确认Q1和线圈1/4脚驱动正常，但新载板把`24V_SW`接到3脚NC，
真正的COM5脚未连接，因此不能继续声明继电器功率触点已经通过自检。

标准 NO 固件此前曾读到 NTC `4095` 并进入 ALARM；2026-10-28 再次监测时已恢复
到约 `23.8–23.9℃` 和 `state=NORMAL`。该值异常通常表示 PA6 分压中点接近
3.3V，应检查 NTC 开路、插接和分压焊点，并继续观察连接稳定性。

3/5脚短接后，`1.2V + 10Ω` 负载实测读到 `Vbus=0.8V`、`I=0.08A`、
`state=NORMAL`，说明继电器COM/NO、JP2和INA228带载路径已经接通。后续仍需
标定量程和方向，并验证ALARM断载与掉电断载。

2026-10-29再次验证VBUS为独立测量：电源1.00V时约为`Vbus=0.7V`、
`I=0.065A`；电源2.00V时约为`Vbus=1.6V`、`I=0.17A`；拔掉负载后
`I≈0`、`Vbus≈1.1V`。这些变化来自INA228 VBUS寄存器，不是用10Ω负载反算。

当前Kconfig仍保留`5A/8A`电流阈值，但载板F1为2A保险丝。该阈值只能视为
未校准的历史默认值，不能用于当前硬件的5A/8A实流验收。演示时应按安全负载
把阈值降到可达到的`0.8–1.5A`范围。

## 五、W5500 探测

新模块实测：

```text
VERR=0x04
SN_SR=0x42
PHYCFGR=0xB8
```

SPI 寄存器访问和 MACRAW socket 初始化正常。2026-10-31 将电脑与 W5500
静态直连后，电脑网卡显示 100 Mbps Link，Ping `192.168.1.200` 为 4/4 成功，
HTTP JSON 上报和网页显示均通过。初始化阶段的 `PHYCFGR=0xB8` 不能单独作为
物理层失败判据。

2026-10-31 后续发现长时间运行后上报线程可能阻塞。已在 `fireeye_net_post()`
加入连接、发送和接收超时，服务端显式关闭连接，并将 TCP 预分配连接数提高到
16。该修复已编译，尚待烧录后持续验收。

## 六、构建

```bash
cd ~/openvela
make -C out_i2c_irq nuttx -j8
arm-none-eabi-objcopy -O ihex out_i2c_irq/nuttx out_i2c_irq/nuttx.hex
arm-none-eabi-objcopy -O binary out_i2c_irq/nuttx out_i2c_irq/nuttx.bin
```

板级改动已同步到 `patches/vendor-gigadevice-gd32f407v-start.patch`，并已从
公共仓基线 `838aae8` 验证可应用。NuttX 侧硬件 I2C 缺陷仍需按公共仓 PR
流程单独处理。

## 七、上板自检

```text
nsh> fireeye current
nsh> fireeye temp
nsh> fireeye oled
nsh> fireeye test
nsh> fireeye -v
```

`fireeye current` 应打印 INA228 厂商 ID、设备 ID、20 位原始码、电流和母线电压。

`fireeye` 和 `fireeye -v` 会进入无限监测循环，当前固件没有实现 Ctrl+C 或命令
退出；当前构建还关闭了 `CONFIG_TTY_SIGINT`，串口 Ctrl+C 不会终止前台任务，
只能复位开发板返回 `nsh>`。`fireeye current`、`temp`、`oled` 和 `test`
执行完会返回提示符。

`fireeye current` 在初始化传感器时会释放继电器，因此适用于 INA228 通信和空载
读数，不适用于标准 NO 接法下的带载电流测试。NO 带载观察应运行 `fireeye -v`，
让 NORMAL 状态持续励磁，再读取每 10 秒输出一行的 `I`、`T` 和 `Vbus`。

## 八、仍未验证

- 新载板 12V、5V、3.3V 的正式电压和纹波记录。
- NTC 当前约 24℃，仍需验证接插件和长期稳定性。
- INA228全量程、零点、温度和长期稳定性。
- 常开继电器在告警、复位和掉电条件下的带载断开。
- 人工复位后故障仍存在时的重复告警。
- 5 分钟以上持续 HTTP 上报、断线恢复和路由器环境。
- 1.2A WARNING、1.5A ALARM 与继电器断载。

## 九、已知代码缺陷（待修复）

2026-10-29 只读审查 `app/fireeye/src/*.c` 与板级补丁，结论见
[code-review-20261029.md](code-review-20261029.md)。

**已修复（77958b3）**：
- ✅ H-1 间歇性传感器失效绕过失效保护：改为滑动窗口判定
  （`SENSOR_FAIL_WINDOW=10`、`SENSOR_FAIL_THRESHOLD=3`）。
- ✅ H-2 INA228 通信失败后无自动恢复：连续 5 次失败后自动重新初始化。

**已修复（2026-10-31）**：
- ✅ M-1 网络上报阻塞：加入非阻塞连接和连接/发送/接收超时，并提高 TCP
  预分配连接数。

**待修复**：
- **M-2 `check_leakage_sensor()` 口径待明确**：新载板标注漏电未实现，
  但该函数参与失效判定。

低优先级项（死文件句柄、子命令重复初始化、滤波计数回绕）见同文档 L 系列。
