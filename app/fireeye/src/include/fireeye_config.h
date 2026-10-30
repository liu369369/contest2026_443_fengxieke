/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Configuration Header
 ****************************************************************************/

#ifndef __FIREYEYE_CONFIG_H
#define __FIREYEYE_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Version Information */
#define FIREYEYE_VERSION_MAJOR    2
#define FIREYEYE_VERSION_MINOR    0
#define FIREYEYE_VERSION_PATCH    0

/* Sample Configuration */
#ifdef CONFIG_FIREYEYE_SAMPLE_RATE
#  undef FIREYEYE_SAMPLE_RATE_HZ
#  define FIREYEYE_SAMPLE_RATE_HZ CONFIG_FIREYEYE_SAMPLE_RATE
#else
#define FIREYEYE_SAMPLE_RATE_HZ   10      /* Sample rate in Hz */
#endif
#define FIREYEYE_SAMPLE_PERIOD_US (1000000 / FIREYEYE_SAMPLE_RATE_HZ)

/* Filter Configuration */
#define FILTER_MOVING_AVG_WINDOW  10      /* Moving average window size */
#define FILTER_LOW_PASS_ALPHA     0.3f    /* Low pass filter coefficient (0-1) */

/* Current Threshold (Amps) */
#ifdef CONFIG_FIREYEYE_CURRENT_WARNING
#  define CURRENT_WARNING_THRESHOLD (CONFIG_FIREYEYE_CURRENT_WARNING / 10.0f)
#else
#define CURRENT_WARNING_THRESHOLD 1.2f    /* Warning: 1.2A (2A fuse safe) */
#endif
#ifdef CONFIG_FIREYEYE_CURRENT_ALARM
#  define CURRENT_ALARM_THRESHOLD (CONFIG_FIREYEYE_CURRENT_ALARM / 10.0f)
#else
#define CURRENT_ALARM_THRESHOLD   1.5f    /* Alarm: 1.5A (2A fuse safe) */
#endif

/* Temperature Threshold (Celsius) */
#ifdef CONFIG_FIREYEYE_TEMP_WARNING
#  define TEMP_WARNING_THRESHOLD (CONFIG_FIREYEYE_TEMP_WARNING / 10.0f)
#else
#define TEMP_WARNING_THRESHOLD    45.0f   /* Warning threshold */
#endif
#ifdef CONFIG_FIREYEYE_TEMP_ALARM
#  define TEMP_ALARM_THRESHOLD (CONFIG_FIREYEYE_TEMP_ALARM / 10.0f)
#else
#define TEMP_ALARM_THRESHOLD      60.0f   /* Alarm threshold */
#endif

/* Debounce Configuration */
#ifdef CONFIG_FIREYEYE_DEBOUNCE_COUNT
#  define DEBOUNCE_COUNT CONFIG_FIREYEYE_DEBOUNCE_COUNT
#else
#define DEBOUNCE_COUNT            3       /* Consecutive count before trigger */
#endif

/* Sensor fail-safe window: N invalid samples within the last M samples
 * forces ALARM.  Unlike the old "N consecutive" rule a single good sample
 * can no longer reset the counter, so intermittent sensor faults cannot
 * hide from the fail-safe path. */

#define SENSOR_FAIL_WINDOW        10      /* Sliding window length */
#define SENSOR_FAIL_THRESHOLD     3       /* Invalid samples in window to fail */
#define INA228_FAIL_REINIT        5       /* Consecutive I2C failures before re-init */

/* Trend Configuration */
#define SLOPE_THRESHOLD           0.5f    /* Slope threshold (A/s or C/s) */
#define TREND_HISTORY_SIZE        10      /* Number of history points for trend */

/* Task Priorities */
#define TASK_PRIORITY_MAIN        200
#define TASK_PRIORITY_DISPLAY     150
#define TASK_PRIORITY_NETWORK     100
#define TASK_PRIORITY_STORAGE     80

/* Task Stack Sizes */
#define TASK_STACK_SIZE_MAIN      4096
#define TASK_STACK_SIZE_DISPLAY   2048
#define TASK_STACK_SIZE_NETWORK   4096
#define TASK_STACK_SIZE_STORAGE   2048

/* Display Configuration */
#define OLED_WIDTH                128
#define OLED_HEIGHT               64
#define OLED_I2C_ADDR             0x3C

/* Storage Configuration */
#define STORAGE_MAX_RECORDS       1000
#define STORAGE_RECORD_SIZE       sizeof(sensor_data_t)

/* 真实传感器配置（新载板：INA228 + NTC + 常开继电器） *********************/

#define FIREYEYE_USE_REAL_SENSORS   1       /* 0 = 强制使用模拟数据（无硬件时调试用） */

/* 电流后端：默认 INA228；旧演示板可通过 menuconfig 选择 ACS712。 */

#if defined(CONFIG_FIREYEYE_CURRENT_SENSOR_ACS712)
#  define FIREYEYE_CURRENT_SENSOR_ACS712 1
#  define FIREYEYE_CURRENT_SENSOR_INA228 0
#else
#  define FIREYEYE_CURRENT_SENSOR_ACS712 0
#  define FIREYEYE_CURRENT_SENSOR_INA228 1
#endif

#define FIREYEYE_ADC_VREF_VOLTS     3.3f    /* ADC 参考电压 = VDDA */
#define FIREYEYE_ADC_FULLSCALE      4096.0f /* 12bit */

/* 新载板电流：INA228 + 2mOhm 分流电阻，I2C1 地址 0x40。 */

#define FIREYEYE_INA228_I2C_ADDR     0x40
#define FIREYEYE_INA228_SHUNT_OHM    0.002f

/* 旧演示板电流：ACS712-30A，输出经 10k+10k 分压后进 PA4。 */

#define FIREYEYE_ADC_CH_CURRENT     4
#define FIREYEYE_ACS712_VOLTS_PER_AMP  0.033f   /* 66mV/A ÷ 2（分压） */

/* 温度：10kΩ 上拉到 3V3 + NTC(10k B3950) 到地，中点进 PA6 */

#define FIREYEYE_ADC_CH_TEMPERATURE 6
#define FIREYEYE_NTC_R_SERIES_OHM   10000.0f
#define FIREYEYE_NTC_R0_OHM         10000.0f
#define FIREYEYE_NTC_B_VALUE        3950.0f
#define FIREYEYE_NTC_T0_KELVIN      298.15f
#define FIREYEYE_TEMP_MIN_C         (-40.0f)   /* 低于此值判为接线异常 */
#define FIREYEYE_TEMP_MAX_C         150.0f     /* 高于此值判为接线异常 */

/* 联网上报（W5500，网线直连 PC，静态 IP） *********************************/

#define FIREYEYE_NET_DEVNAME    "eth0"
#define FIREYEYE_NET_DEVICE_ID  "fireeye-01"

/* 开发板地址（与 PC 网口同网段） */

#define FIREYEYE_NET_IPADDR     "192.168.1.200"
#define FIREYEYE_NET_NETMASK    "255.255.255.0"
#define FIREYEYE_NET_GATEWAY    "192.168.1.100"

/* 上位机地址（tools/fireeye_monitor.py 所在电脑） */

#define FIREYEYE_NET_SERVER     "192.168.1.100"
#define FIREYEYE_NET_PORT       8080
#define FIREYEYE_NET_PATH       "/api/data"
#define FIREYEYE_NET_PERIOD_S   5

/* 固定 MAC（本地管理地址，避免每次重启变化） */

#define FIREYEYE_NET_MAC        {0x02, 0x00, 0x00, 0x46, 0x49, 0x52}

/* SPI Configuration */
#define SPI_BUS_OLED              0   /* SPI0 - OLED Display */
#define SPI_BUS_ETHERNET          1   /* SPI1 - Ethernet (W5500) */

/****************************************************************************
 * Type Definitions
 ****************************************************************************/

/* System States */
typedef enum
{
  STATE_NORMAL = 0,     /* Normal operation */
  STATE_WARNING = 1,    /* Warning condition */
  STATE_ALARM = 2,      /* Alarm condition */
  STATE_SHUTDOWN = 3    /* Emergency shutdown */
} system_state_t;

/* System Events */
typedef enum
{
  EVENT_NORMAL = 0,     /* Return to normal */
  EVENT_WARNING = 1,    /* Warning triggered */
  EVENT_ALARM = 2,      /* Alarm triggered */
  EVENT_SHUTDOWN = 3,   /* Shutdown triggered */
  EVENT_RESET = 4       /* Manual reset */
} system_event_t;

/* Alert Types */
typedef enum
{
  ALERT_TYPE_CURRENT = 0,     /* Current anomaly */
  ALERT_TYPE_TEMPERATURE = 1, /* Temperature anomaly */
  ALERT_TYPE_LEAKAGE = 2      /* Leakage detected */
} alert_type_t;

/* Sensor Data Structure */
typedef struct
{
  float current;              /* Current in Amps */
  float temperature;          /* Temperature in Celsius */
  float bus_voltage;          /* Charging-loop voltage in Volts, NAN if N/A */
  bool leakage;               /* Leakage detected */
  system_state_t state;       /* System state */
  uint32_t timestamp;         /* Timestamp in milliseconds */
} sensor_data_t;

/* Alert Record Structure */
typedef struct
{
  alert_type_t type;          /* Alert type */
  system_state_t level;       /* Alert level */
  float value;                /* Trigger value */
  uint32_t timestamp;         /* Timestamp */
  char message[64];           /* Alert message */
} alert_record_t;

/* Filter Context */
typedef struct
{
  float buffer[FILTER_MOVING_AVG_WINDOW];  /* Moving average buffer */
  int window_size;            /* Active moving-average window size */
  int index;                  /* Current index */
  float sum;                  /* Sum of buffer */
  float alpha;                /* Low pass filter coefficient */
  float last_output;          /* Last output value */
  bool low_pass_initialized;  /* Low pass filter has a valid first sample */
  bool initialized;           /* Initialization flag */
} filter_ctx_t;

/* Threshold State */
typedef struct
{
  int over_count;             /* Consecutive over-limit count */
  float history[TREND_HISTORY_SIZE];  /* History for trend analysis */
  int history_index;          /* History index */
} threshold_state_t;

/* Threshold Configuration */
typedef struct
{
  float warning_threshold;    /* Warning threshold */
  float alarm_threshold;      /* Alarm threshold */
  int consecutive_count;      /* Consecutive count for debounce */
  float slope_threshold;      /* Slope threshold for trend */
} threshold_config_t;

/* State Machine Context */
typedef struct
{
  system_state_t current_state;  /* Current state */
  int state_duration;            /* Duration in current state */
  uint32_t last_state_change;    /* Last state change timestamp */
} fsm_ctx_t;

/****************************************************************************
 * Function Prototypes
 ****************************************************************************/

/* Initialization */
int fireeye_init(void);

/* Main Task Entry Point */
int fireeye_main_task(int argc, char *argv[]);

/* Display Task Entry Point */
int fireeye_display_task(int argc, char *argv[]);

/* Network Task Entry Point */
int fireeye_network_task(int argc, char *argv[]);

/* Storage Task Entry Point */
int fireeye_storage_task(int argc, char *argv[]);

#endif /* __FIREYEYE_CONFIG_H */
