# 火眼算法原型

本目录包含火眼监测终端的 PC 端算法原型，用于在实际硬件开发前验证核心算法逻辑。

## 文件说明

- `algorithm_prototype.py` - 主算法原型程序
- `requirements.txt` - Python 依赖
- `README.md` - 本文件

## 算法验证内容

### 1. 数据滤波
- **滑动平均滤波**：去除高频噪声
- **一阶低通滤波**：平滑数据，减少毛刺
- **组合滤波**：先滑动平均再低通，效果最佳

### 2. 阈值判定
- **单阈值判断**：超过预警/报警阈值
- **趋势判断**：检测异常上升斜率
- **防抖机制**：连续 N 次超限才触发，避免误报

### 3. 状态机
- **状态转换**：正常 → 预警 → 报警 → 断电
- **复位功能**：支持人工复位
- **历史记录**：记录状态变化历史

## 运行方法

### 1. 安装依赖

```bash
pip3 install -r requirements.txt
```

### 2. 运行算法原型

```bash
python3 algorithm_prototype.py
```

### 3. 查看结果

程序会：
1. 生成模拟传感器数据（电流、温度、漏电）
2. 运行滤波、阈值、状态机算法
3. 打印统计信息
4. 生成可视化图表 `fireeye_algorithm_results.png`

## 模拟场景

> 说明：本目录是 **PC 端算法原型**，数据全部为**程序生成的模拟数据**，其中"漏电"也是**模拟输入而非实测值**（板端目前无漏电互感器，漏电监测为后续扩展项）。

程序模拟了一个完整的充电异常场景：

| 时间段 | 场景 | 电流 | 温度 | 漏电 |
|-------|------|------|------|------|
| 0-10s | 正常充电 | 2-3A | 25-27°C | 无 |
| 10-20s | 电流逐渐上升 | 3-5A | 25-45°C | 无 |
| 20-25s | 异常过流/高温 | 8-10A | 55-65°C | 有漏电 |
| 25-30s | 恢复正常 | 2-3A | 25-27°C | 无 |

## 算法参数说明

在 `algorithm_prototype.py` 的 `Config` 类中可以调整参数：

```python
class Config:
    # 滤波参数
    MOVING_AVG_WINDOW = 10      # 滑动平均窗口大小
    LOW_PASS_ALPHA = 0.3        # 低通滤波系数 (0-1)
    
    # 阈值参数 (电流, 单位: A)
    CURRENT_WARNING_THRESHOLD = 5.0   # 预警阈值
    CURRENT_ALARM_THRESHOLD = 8.0     # 报警阈值
    
    # 阈值参数 (温度, 单位: °C)
    TEMP_WARNING_THRESHOLD = 45.0     # 预警阈值
    TEMP_ALARM_THRESHOLD = 60.0       # 报警阈值
    
    # 防抖参数
    CONSECUTIVE_COUNT = 3       # 连续超限次数才触发
    
    # 趋势参数
    SLOPE_THRESHOLD = 0.5       # 斜率阈值 (A/s 或 °C/s)
```

## 从 Python 到 C 的移植

验证完算法后，需要将 Python 代码移植到 C 语言：

### 1. 滤波器移植

```c
// Python:
class CombinedFilter:
    def process(self, value):
        avg_value = self.moving_avg.process(value)
        return self.low_pass.process(avg_value)

// C:
float filter_combined(filter_ctx_t *ctx, float input) {
    float avg = filter_moving_average(ctx, input);
    return filter_low_pass(ctx, avg);
}
```

### 2. 阈值判定移植

```c
// Python:
def check(self, value):
    if value >= self.alarm_threshold:
        return 2
    elif value >= self.warning_threshold:
        return 1
    return 0

// C:
int threshold_check(threshold_config_t *config, threshold_state_t *state, float value) {
    if (value >= config->alarm_threshold) {
        return 2;
    } else if (value >= config->warning_threshold) {
        return 1;
    }
    return 0;
}
```

### 3. 状态机移植

```c
// Python:
def process_event(self, event):
    transitions = {
        SystemState.NORMAL: {
            SystemEvent.WARNING: SystemState.WARNING,
        },
    }
    return transitions[self.current_state].get(event, self.current_state)

// C:
system_state_t fsm_process_event(state_machine_t *fsm, system_event_t event) {
    // 状态转换表实现
}
```

## 调试建议

1. **调整参数**：修改 `Config` 类中的参数，观察算法响应
2. **修改场景**：修改 `SensorSimulator` 类，添加更多异常场景
3. **可视化**：查看生成的图表，分析算法效果
4. **日志输出**：在关键位置添加 print 语句，观察中间状态

## 下一步

1. 在 PC 端充分验证算法逻辑
2. 调整参数直到满足需求
3. 将验证后的算法移植到 C 语言
4. 集成到 openvela 应用中

---

*最后更新: 2026-09-03*
