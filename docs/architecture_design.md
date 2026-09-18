# 火眼应用 - 架构设计文档

> 当前版本未实现的能力：漏电监测、断网补传、RTC/NTP、CAN/RS485 充电桩联动、TinyML、看门狗；文中相应模块均为设计项。
> 本地存证已实现：`fireeye_storage.c` 把状态变化与心跳写入片上 Flash（`up_progmem`），`fireeye -e` 可回放。

> 本文档描述火眼监测终端的整体软件架构、模块划分和数据流设计

## 一、系统概述

火眼是一个运行在 openvela (NuttX) 上的实时监测系统，用于电动自行车充电安全监控。系统采用分层架构，从底层硬件驱动到上层应用逻辑，各层职责清晰。

## 二、架构分层

```
┌─────────────────────────────────────────────────────────────┐
│                    应用层 (Application)                      │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │  主控任务    │ │  显示任务    │ │    联网上报任务      │   │
│  │ (Main Task) │ │(Display Task)│ │ (Network Task)      │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    服务层 (Service)                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │  状态机      │ │  告警管理    │ │    数据存证          │   │
│  │ (State FSM) │ │ (Alert Mgr) │ │ (Data Storage)      │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    算法层 (Algorithm)                        │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │  数据滤波    │ │  阈值判定    │ │    趋势分析          │   │
│  │ (Filter)    │ │ (Threshold) │ │ (Trend Analysis)    │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    驱动层 (Driver)                           │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────────────┐ │
│  │ ADC │ │ SPI │ │ GPIO│ │ UART│ │ I2C │ │ Ethernet    │ │
│  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    硬件层 (Hardware)                         │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐  │
│  │ ACS712  │ │  NTC    │ │ 蜂鸣器   │ │ OLED / W5500    │  │
│  │ 电流    │ │ 温度    │ │ 声报警   │ │ 显示 / 网络      │  │
│  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## 三、模块详细设计

### 3.1 驱动层 (Driver Layer)

负责与硬件外设的直接交互，提供统一的 API 给上层调用。

#### 3.1.1 ADC 驱动
```c
// adc_driver.h
typedef struct {
    int channel;        // ADC 通道号
    int resolution;     // 分辨率 (12bit)
    float vref;         // 参考电压
} adc_config_t;

int adc_init(const adc_config_t *config);
int adc_read_raw(int channel);
float adc_read_voltage(int channel);
```

#### 3.1.2 GPIO 驱动
```c
// gpio_driver.h
typedef enum {
    GPIO_OUTPUT,
    GPIO_INPUT,
    GPIO_PWM
} gpio_mode_t;

int gpio_init(int pin, gpio_mode_t mode);
int gpio_write(int pin, int value);
int gpio_pwm_start(int pin, uint32_t freq, uint32_t duty);
```

#### 3.1.3 SPI 驱动 (OLED / W5500)
```c
// spi_driver.h
int spi_init(int bus, uint32_t speed);
int spi_transfer(int bus, uint8_t *tx, uint8_t *rx, size_t len);
```

### 3.2 算法层 (Algorithm Layer)

数据处理的核心，负责从原始传感器数据中提取有意义的信息。

#### 3.2.1 数据滤波模块

```c
// filter.h
typedef struct {
    float *buffer;      // 滑动窗口缓冲区
    int window_size;    // 窗口大小
    int index;          // 当前索引
    float sum;          // 窗口内累加和
    float alpha;        // 低通滤波系数
    float last_output;  // 上次输出
} filter_ctx_t;

// 滑动平均滤波
float filter_moving_average(filter_ctx_t *ctx, float input);

// 一阶低通滤波
float filter_low_pass(filter_ctx_t *ctx, float input);

// 组合滤波：滑动平均 + 低通
float filter_combined(filter_ctx_t *ctx, float input);
```

**滤波算法流程：**
```
原始数据 → 滑动平均 (去噪) → 一阶低通 (平滑) → 滤波后数据
```

#### 3.2.2 阈值判定模块

```c
// threshold.h
typedef struct {
    float warning_threshold;    // 预警阈值
    float alarm_threshold;      // 报警阈值
    int consecutive_count;      // 连续触发次数阈值
    float slope_threshold;      // 斜率阈值（趋势）
    float variance_threshold;   // 方差阈值（波动）
} threshold_config_t;

typedef struct {
    int over_count;             // 超限计数
    float last_values[10];      // 历史值（用于趋势计算）
    int history_index;
} threshold_state_t;

// 阈值判定：返回 0=正常, 1=预警, 2=报警
int threshold_check(
    const threshold_config_t *config,
    threshold_state_t *state,
    float value
);
```

**判定逻辑：**
1. 单阈值判断：value > warning_threshold 或 value > alarm_threshold
2. 趋势判断：计算斜率（变化率）和方差（波动）
3. 防抖：连续 N 个采样点都超限才触发

#### 3.2.3 趋势分析模块

```c
// trend.h
typedef struct {
    float slope;        // 斜率（每秒变化量）
    float variance;     // 方差
    float mean;         // 均值
    float min;          // 最小值
    float max;          // 最大值
} trend_result_t;

// 计算趋势
trend_result_t trend_analyze(float *data, int length);

// 判断是否异常上升
bool trend_is_abnormal_rising(trend_result_t *result, float threshold);
```

### 3.3 服务层 (Service Layer)

基于算法层的结果，执行相应的业务逻辑。

#### 3.3.1 状态机模块

```c
// state_machine.h
typedef enum {
    STATE_NORMAL,       // 正常状态
    STATE_WARNING,      // 预警状态
    STATE_ALARM,        // 报警状态
    STATE_SHUTDOWN      // 联动断电状态
} system_state_t;

typedef enum {
    EVENT_NORMAL,       // 恢复正常
    EVENT_WARNING,      // 触发预警
    EVENT_ALARM,        // 触发报警
    EVENT_SHUTDOWN,     // 触发断电
    EVENT_RESET         // 人工复位
} system_event_t;

typedef struct {
    system_state_t current_state;
    int state_duration;         // 当前状态持续时间
    time_t last_state_change;   // 上次状态变化时间
} state_machine_t;

// 状态机初始化
void fsm_init(state_machine_t *fsm);

// 处理事件，返回新状态
system_state_t fsm_process_event(state_machine_t *fsm, system_event_t event);

// 获取当前状态
system_state_t fsm_get_state(const state_machine_t *fsm);
```

**状态转换图：**
```
                 ┌──────────────────────────────────────┐
                 │                                      │
                 ▼                                      │
            ┌─────────┐   超限   ┌─────────┐   超限   ┌─────────┐
            │  正常    │ ──────→ │  预警    │ ──────→ │  报警    │
            │ NORMAL  │         │ WARNING │         │  ALARM  │
            └─────────┘         └─────────┘         └─────────┘
                 ▲                   │                   │
                 │                   │                   │
                 │    恢复正常       │    恢复正常       │ 持续超限
                 └───────────────────┘                   │
                                                         ▼
                                                   ┌─────────┐
                                                   │  断电    │
                                                   │SHUTDOWN │
                                                   └─────────┘
                                                         │
                                                         │ 人工复位
                                                         ▼
                                                   ┌─────────┐
                                                   │  正常    │
                                                   └─────────┘
```

#### 3.3.2 告警管理模块

```c
// alert_manager.h
typedef enum {
    ALERT_TYPE_CURRENT,     // 电流异常
    ALERT_TYPE_TEMPERATURE, // 温度异常
    ALERT_TYPE_LEAKAGE      // 漏电异常
} alert_type_t;

typedef struct {
    alert_type_t type;
    system_state_t level;
    float value;
    time_t timestamp;
    char message[64];
} alert_record_t;

// 初始化告警管理器
void alert_manager_init(void);

// 触发声光报警
void alert_trigger_alarm(system_state_t level);

// 停止报警
void alert_stop_alarm(void);

// 记录告警事件
int alert_record_event(const alert_record_t *record);
```

#### 3.3.3 数据存证模块

```c
// data_storage.h
typedef struct {
    float current;          // 电流值
    float temperature;      // 温度值
    bool leakage;           // 漏电状态
    system_state_t state;   // 系统状态
    time_t timestamp;       // 时间戳
} sensor_data_t;

// 初始化存储
int storage_init(void);

// 保存传感器数据
int storage_save_data(const sensor_data_t *data);

// 保存告警记录
int storage_save_alert(const alert_record_t *alert);

// 查询历史数据
int storage_query_data(time_t start, time_t end, sensor_data_t *buffer, int max_count);
```

### 3.4 应用层 (Application Layer)

系统的主入口，负责任务调度和协调各模块。

#### 3.4.1 主控任务

```c
// main_task.c
void main_task_entry(void *arg)
{
    // 1. 初始化所有模块
    adc_driver_init();
    filter_init(&current_filter, WINDOW_SIZE, ALPHA);
    filter_init(&temp_filter, WINDOW_SIZE, ALPHA);
    fsm_init(&state_machine);
    alert_manager_init();
    storage_init();
    
    // 2. 主循环
    while (1) {
        // 读取传感器数据
        float current = read_current_sensor();
        float temperature = read_temperature_sensor();
        bool leakage = check_leakage();
        
        // 数据滤波
        float filtered_current = filter_combined(&current_filter, current);
        float filtered_temp = filter_combined(&temp_filter, temperature);
        
        // 阈值判定
        int current_status = threshold_check(&current_config, &current_state, filtered_current);
        int temp_status = threshold_check(&temp_config, &temp_state, filtered_temp);
        
        // 取最高告警级别
        int max_status = MAX(current_status, temp_status);
        if (leakage) max_status = STATE_ALARM;
        
        // 状态机处理
        system_event_t event = status_to_event(max_status);
        system_state_t new_state = fsm_process_event(&state_machine, event);
        
        // 执行相应动作
        if (new_state == STATE_WARNING || new_state == STATE_ALARM) {
            alert_trigger_alarm(new_state);
        } else if (new_state == STATE_SHUTDOWN) {
            trigger_shutdown_relay();
        }
        
        // 保存数据
        sensor_data_t data = {
            .current = filtered_current,
            .temperature = filtered_temp,
            .leakage = leakage,
            .state = new_state,
            .timestamp = time(NULL)
        };
        storage_save_data(&data);
        
        // 延时（采样周期）
        usleep(100000);  // 100ms = 10Hz
    }
}
```

#### 3.4.2 显示任务

```c
// display_task.c
void display_task_entry(void *arg)
{
    oled_init();
    
    while (1) {
        // 获取最新数据
        sensor_data_t data = get_latest_data();
        system_state_t state = fsm_get_state(&state_machine);
        
        // 更新显示
        oled_clear();
        oled_display_string(0, 0, "FireEye Monitor");
        oled_display_string(0, 16, "Current: %.2f A", data.current);
        oled_display_string(0, 32, "Temp: %.1f C", data.temperature);
        oled_display_string(0, 48, "Status: %s", state_to_string(state));
        
        // 告警状态闪烁
        if (state == STATE_WARNING || state == STATE_ALARM) {
            oled_flash_warning();
        }
        
        usleep(500000);  // 500ms 刷新
    }
}
```

#### 3.4.3 联网上报任务

```c
// network_task.c
void network_task_entry(void *arg)
{
    // 初始化网络
    ethernet_init();
    mqtt_client_init();
    
    while (1) {
        // 检查是否有告警需要上报
        if (has_pending_alert()) {
            alert_record_t alert = get_pending_alert();
            
            // 构建 JSON 消息
            char json_msg[256];
            snprintf(json_msg, sizeof(json_msg),
                "{\"type\":\"%s\",\"level\":\"%s\",\"value\":%.2f,\"time\":%ld}",
                alert_type_to_string(alert.type),
                state_to_string(alert.level),
                alert.value,
                alert.timestamp
            );
            
            // 发送到 MQTT 服务器
            mqtt_publish("fireeye/alert", json_msg);
        }
        
        // 定期上报状态
        if (should_report_status()) {
            sensor_data_t data = get_latest_data();
            report_status(&data);
        }
        
        sleep(1);  // 1秒检查一次
    }
}
```

## 四、任务优先级设计

| 任务 | 优先级 | 周期 | 说明 |
|-----|--------|------|------|
| 主控任务 | 最高 | 100ms | 传感器采集、算法处理、状态机 |
| 告警任务 | 最高 | 事件触发 | 声光报警控制 |
| 显示任务 | 中等 | 500ms | OLED 显示更新 |
| 联网任务 | 较低 | 1s | 数据上报、命令接收 |
| 存证任务 | 最低 | 100ms | 异步写入存储 |

## 五、数据流设计

```
传感器硬件
    │
    ▼
┌─────────────────────────────────────────────────────┐
│                    ADC 采集                          │
│         (电流: ACS712, 温度: NTC, 漏电: 互感器)      │
└─────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────┐
│                    数据滤波                          │
│              滑动平均 + 一阶低通                      │
└─────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────┐
│                   阈值判定                           │
│          单阈值 + 趋势 + 连续N点防抖                  │
└─────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────┐
│                   状态机                             │
│           正常 → 预警 → 报警 → 断电                   │
└─────────────────────────────────────────────────────┘
    │
    ├──────────────────┬──────────────────┐
    ▼                  ▼                  ▼
┌─────────┐      ┌─────────┐       ┌─────────┐
│ 声光报警 │      │ OLED显示│       │ 联网上报 │
│ GPIO/PWM│      │  SPI    │       │ 以太网  │
└─────────┘      └─────────┘       └─────────┘
    │
    ▼
┌─────────────────────────────────────────────────────┐
│                   本地存证                           │
│              Flash / SD 卡存储                       │
└─────────────────────────────────────────────────────┘
```

## 六、内存管理

### 6.1 静态分配为主

嵌入式系统建议使用静态内存分配，避免动态分配带来的碎片问题：

```c
// 定义全局数据结构
static filter_ctx_t current_filter;
static filter_ctx_t temp_filter;
static state_machine_t state_machine;
static sensor_data_t data_buffer[BUFFER_SIZE];
```

### 6.2 内存估算

| 模块 | 内存占用 | 说明 |
|-----|---------|------|
| 滤波缓冲区 | 200 字节 | 2个滤波器 × 10个float × 4字节 |
| 状态机 | 20 字节 | 状态 + 计数器 |
| 数据缓冲区 | 4KB | 100条记录 × 40字节 |
| 任务栈 | 4KB × 3 | 3个任务各4KB |
| **总计** | **约 16KB** | GD32F470 有 256KB SRAM，足够 |

## 七、错误处理策略

### 7.1 Fail-Fast 原则

```c
// 关键路径使用断言
assert(sensor_value >= 0 && sensor_value <= MAX_VALUE);

// 错误时立即报错退出
if (adc_read == ADC_ERROR) {
    LOG_ERROR("ADC read failed!");
    alert_trigger_alarm(STATE_ALARM);
    return -1;
}
```

### 7.2 看门狗保护

```c
// 初始化看门狗
watchdog_init(WATCHDOG_TIMEOUT_MS);

// 主循环中喂狗
while (1) {
    // ... 主要逻辑 ...
    watchdog_feed();
}
```

## 八、可扩展性设计

### 8.1 传感器扩展

通过配置文件定义传感器类型和参数：

```json
{
    "sensors": {
        "current": {
            "type": "ACS712",
            "channel": 0,
            "scale": 0.185,
            "offset": 2.5
        },
        "temperature": {
            "type": "NTC",
            "channel": 1,
            "beta": 3950,
            "r0": 10000,
            "t0": 25
        }
    }
}
```

### 8.2 算法扩展

支持通过配置调整算法参数：

```c
typedef struct {
    int filter_window_size;
    float filter_alpha;
    float warning_threshold;
    float alarm_threshold;
    int consecutive_count;
} algorithm_config_t;
```

## 九、开发建议

1. **先在 PC 端验证算法**：用 Python 实现滤波、阈值、状态机，验证逻辑正确性
2. **模块化开发**：每个模块独立测试，再集成
3. **使用模拟数据**：在没有传感器时，用模拟数据测试
4. **日志记录**：关键路径添加日志，便于调试
5. **单元测试**：为每个模块编写测试用例

---

*文档版本: 1.0*
*最后更新: 2026-09-03*
