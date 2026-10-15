# 火眼载板 · 嘉立创EDA 绘制进度

> 更新：2026-10-11（第二轮：按网表复核结果修复缺陷后重写）
> 工程：**火眼载板**（uuid `71e441cf048443cfac191eeba5dea199`）
> 原理图页 P1：uuid `8cafcafa25d6eaef`（docType=1）｜ PCB：uuid `d430dbec181065a4`
> 工具：easyeda-api skill + bridge server（`http://localhost:49620`）

## 一、原理图当前实测状态（数据来自 EDA 自己导出的网表）

| 项 | 数值 |
|---|---|
| 图元总数 | 48 = 45 个器件 + 1 个 A4 图框 + 2 个电源符号（GND / VCC） |
| 导线 | 189 条 |
| 网络 | 95 个 = 35 个功能网络 + 59 个母座引脚标记（JP5_n / JP6_n）+ 1 个空网络（INA228 的 ALERT、继电器 NO 两个悬空脚） |
| 位号 | EDA 自动编号，**与设计文档的位号不一致**，对照表见 `fireeye_carrier_board_design.md` 第十章 |

**复核方法（比"看导线位置"可靠）**：用 `eda.sch_ManufactureData.getNetlistFile()` 取回 EDA
自己生成的网表（`.enet`，JSON），逐个器件读 `pinInfoMap`，得到"每个引脚实际挂在哪个网络"。
EDA 就是按这份数据生成 PCB 网络的，所以它是权威依据。

> 注意：`sch_Net.getAllNets()` / `getAllNetsName()` 在这台机器上始终返回空，`sch_PrimitivePin.getAll()`
> 也返回空（商城库只读），**不要再用这两个接口判断连接**。

## 二、已放置器件（45 个，位号为 EDA 实际编号）

| 位号 | 器件 | 立创编号 | 用途 |
|---|---|---|---|
| U1 | INA228AIDGSR | C2887910 | 20 位电流/电压监测 |
| U2 | MP1584WN | C2916141 | 24V→5V 降压（SOIC-8-EP） |
| U3 | AMS1117-3.3 | C6186 | 5V→3.3V |
| RLY1 | SRD-05VDC-SL-C | C35449 | 断电继电器 |
| D1 | SS54 | C22452 | 24V 防反接 |
| D2 | SS34 | C8678 | DC-DC 续流 |
| D3 | 1N4148W | C81598 | 线圈续流 |
| U4 | 47µH / 2A 电感 | C9900013584 | DC-DC 储能 |
| R1 | LMN25HF3P0R002S（2mΩ 3W 2512） | C41406285 | 分流电阻 |
| C1–C4 | 100nF 0603 | C14663 | BST / 去耦 / NTC / 差分滤波 |
| C5–C6 | 10nF 0603 | C57112 | 采样共模滤波 |
| C7–C8 | 10µF/25V 0805 | C15850 | LDO 输入/输出 |
| C9 | 3.3nF 0603 | C1613 | COMP 补偿 |
| C10–C11 | 10µF/50V 1206 | C13585 | 24V 输入 / 5V 输出滤波 |
| R2–R3 | 10Ω | C22859 | INA228 输入串阻 |
| R4–R6 | 10kΩ | C25804 | NTC 分压 / 继电器 / 蜂鸣器下拉 |
| R7–R9 | 100kΩ | C22936 | FB 上臂 / FREQ / EN |
| R10 | 6.8kΩ | C23212 | COMP |
| R11–R12 | 4.7kΩ | C23162 | I2C 上拉 |
| R13–R14 | 1kΩ | C21190 | 蜂鸣器 / 继电器基极限流 |
| R15 | 19.1kΩ | C22897 | FB 下臂（决定 5V 值） |
| F1 | 1812L200/36GR | C49196649 | 2A 自恢复保险丝 |
| Q1–Q2 | SS8050 | C2150 | 继电器 / 蜂鸣器驱动 |
| U5–U6 | KH-2.54FH-2X19P-H5.0 | C55160397 | 2×19 母座（JP6 / JP5） |
| U7–U8 | KF128-5.08-2P-AA | C474952 | 24V 输入 / 负载输出端子 |
| H1 | PZ254-1-04-W-8.5（弯插） | C3012225 | OLED 座 |
| U9 | PZ254-1-07-W-8.5KK | C3012228 | W5500 座 |
| U10 | PZ254-1-03-W-2.5-G1（弯插） | **C18197979** | 调试串口座 |
| H2 | HX PZ-254-1-02-Z-11.5 | C42391536 | NTC 座 |

> 上面这一列立创编号是从 EDA 网表 props 里逐个读出来的，可直接用于下单。
> 注意与设计文档 9.3 节原先记的"串口座 C20363580"不一致，**以 C18197979 为准**。

## 三、已核对正确（网表 + START 板原理图双向验证过）

- **采样链路**：R1（2mΩ）两端分别经 R3 / R2（10Ω）进 INA228 的 IN+ / IN−；IN+ 在 `24V_P`（电源侧）、
  IN− 在 `24V_SW`（负载侧），极性正确；C4（100nF）跨差分，C5 / C6（10nF）各自对地，串阻与滤波齐全。
- **INA228（U1）**：A0/A1 接地（地址 0x40）、VS=`VCC`、VBUS=`24V_SW`、ALERT 悬空（设计如此）。
- **继电器 RLY1**：1=5V（线圈+）、4=`RELAY_COIL_N`（线圈−）、3=`24V_SW`（COM）、5=`LOAD_P`（NC）、2=悬空（NO）。
- **二极管极性**：SS54 阳极=24V_F、阴极=24V_P；SS34 阴极=SW_NODE、阳极=GND；1N4148W 阴极=5V、阳极=RELAY_COIL_N。
- **MP1584 外围（U2）**：BST=C1、SW=U4+C1+D2、FB=R7/R15、COMP=R10+C9、FREQ=R8、EN=R9、EP=GND。
- **母座信号**：JP6（U5）：1/3=GND、2=VCC、13=UART_RX、16=UART_TX；
  JP5（U6）：9/13/15/16/17/19/25/26/27/29/30/31 = NTC_ADC / RELAY_CTRL / W5500_SCS / BUZZ_CTRL /
  W5500_RST / W5500_INT / I2C1_SDA / I2C1_SCL / SPI1_SCK / SPI1_MOSI / SPI1_MISO / LED_CTRL，
  与 START 板原理图的 **PA6 / PB0 / PE7 / PB1 / PE9 / PE11 / PB11 / PB10 / PB13 / PB15 / PB14 / PD9** 逐个对上。
- **没有压线短路**：189 条导线里没有任何两条属于不同网络的线段互相重叠。

## 四、2026-10-11 已在 EDA 内修复

1. **继电器驱动断路（致命）**：Q1 集电极上原是一条没有名字的导线（EDA 自动命名 `$1N213`），
   与继电器线圈没有电气连接 → 继电器永远不会吸合。
   处理：删掉那条导线、重建为 `RELAY_COIL_N`。现在 `RELAY_COIL_N = { RLY1-4, D3-2, Q1-3 }` ✓
   > 踩坑：先尝试直接把那条导线的网络名改成 `RELAY_COIL_N`，API 返回成功但 `get()` 读回是空、网表不变；
   > 换成"删除 + 用 `sch_PrimitiveWire.create(line, net)` 重建"才真正生效。
2. **23 组母座同号脚误连（严重）**：JP5 与 JP6 上原先贴着同一套数字网络标签，
   凡两边都有的号就是同一个网络（`4 5 6 7 8 10 11 12 14 18 20 21 22 23 24 28 32 33 34 35 36 37 38`），
   其中 `32` 一对是 **PA13（SWDIO）接 PD8**，会干扰烧录。
   处理：JP6（U5）侧 33 条、JP5（U6）侧 26 条全部改名为 `JP6_n` / `JP5_n`，
   现在没有任何跨母座的多点网络 ✓
   > 踩坑：第一遍改完有 30 条静默失效（返回成功但 readback 为空）；按 `primitiveId` 逐条重试并校验才全部生效。
3. **MP1584 第 9 脚接错**：封装是 `SOIC-8-EP`（第 9 脚是散热焊盘），立创符号却把它标成 `EN`，
   原先接在 `24V_P` 上。处理：改接 **GND**（U2-9 = GND ✓）。

## 五、仍待处理

1. **报警灯座、蜂鸣器座（缺 2 个接插件）**：
   API 的 `sch_PrimitiveComponent.create` 现在**连续返回 500**（换了位置、换了器件，试了 4 次都一样），
   所以这两个座子**需要手动从库里拖进来**：
   - 报警灯座：1×3 排针，用 **C18197979**（PZ254-1-03-W-2.5-G1，与串口座同型号）
   - 蜂鸣器座：1×2 排针，用 **C42391536**（HX PZ-254-1-02-Z-11.5，与 NTC 座同型号）

   放好后我来挂网络：报警灯 = 5V / GND / LED_CTRL，蜂鸣器 = 5V / BUZZ_N。
2. **W5500 座缺 GND**：U9 是 1×7，7 个脚已被 SCK / MISO / MOSI / SCS / RST / INT / VCC 占满。
   需要按**模块实物丝印**确认引脚数与顺序（给我一张排针丝印的照片即可），再决定是加一个 GND 脚还是让出某个信号。
3. **载板 3.3V 与 START 板 3V3 并联**：U5-2 = `VCC`，而 U5-2 就是 START 板的 3V3 输出，
   等于两片 AMS1117 输出并联、互相倒灌；与设计文档 3.1 节"不并联"的结论相反。
   需要你决定走哪条路（见设计文档 3.1 的待决策项）。
4. 切换到 PCB 文档（uuid `d430dbec181065a4`），按设计文档 7.3 摆两个母座（**第 1 脚在下端**），再布局布线。

## 六、母座引脚定义（实测结果已并入设计文档 6.5 节）

- **JP6（U5）**：1、3 = GND，2 = +3V3，其余 35 脚为 GPIO（PC13 / PE5 / PE6 / PE3 / PE4 / PE1 / PE2 /
  PB9 / PE0 / PB7 / PB8 / PB5 / PB6 / PB3 / PB4 / PD6 / PD7 / PD4 / PD5 / PD3 / PD2 / PD1 / PD0 /
  PC12 / PC11 / PC10 / PA15 / PA14 / PA13 / PA10 / PA9 / PA8 / PC9 / PC8 / PC7）。
- **JP5（U6）**：**38 脚全是 GPIO，没有任何电源脚**（所以"JP5 的 1/2/3 要接 GND/VCC"这个待办不成立）。

数据来源：`GD32F470V-START-V1.0.pdf` 第 1 页 Extension，按坐标逐脚取出（`PyMuPDF` 提取文字 + 坐标比对）。

## 七、复核用的核心脚本

```javascript
// 读 EDA 自己的网表，逐器件列"引脚 -> 网络"
const f = await eda.sch_ManufactureData.getNetlistFile();
const j = JSON.parse(await f.text());          // f 是浏览器 File 对象，用 .text()
const out = {};
for (const k in j.components) {
  const c = j.components[k];
  const d = (c.props || {}).Designator;
  const m = c.pinInfoMap || {};
  const rows = [];
  for (const p in m) rows.push(p + "(" + m[p].name + ")=" + String(m[p].net));
  out[d] = rows.join(" ");
}
return out;
```

判读规则：网络名为**空**的引脚 = 没有接到任何电路；纯数字网络名 = 母座脚号临时标签（本轮已全部改名）。

## 八、环境与常见故障

1. **EDA 扩展会周期性断连**：报 `No EDA window connected` 或 `EDA window "..." disconnected` 时，
   在 EDA 里把扩展 `run-api-gateway` 停用再启用（或重启 EDA），bridge 本身不用重启。
2. **`sch_PrimitiveComponent.create` 目前必失败**：返回 500 或把扩展直接搞断，换位置、换器件都一样；
   暂时只能手动拖器件进原理图。
3. **`sch_PrimitiveWire.modify` 会静默失效**：返回成功但实际没改（本轮 30/59 条如此）。
   改完必须用 `get(primitiveId)` 读回或重跑网表核对。
4. **bridge 进程**：端口 49620，重启电脑后需重新启动
   （`node <skill>/scripts/bridge-server.mjs`，skill 目录 `C:\Users\刘艳秋\.codex\skills\easyeda-api-skill`）。
5. **单次执行 30 秒上限**：立创商城器件搜索后期会慢到 25 秒以上，一次只处理 1-2 个器件。
6. **元件移动时导线不跟随**：会造成"标签还在、连接已断"，DRC 未必报错——每轮移动后都要跑网表复核。
