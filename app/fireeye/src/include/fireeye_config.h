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
#define FIREYEYE_VERSION_MAJOR    1
#define FIREYEYE_VERSION_MINOR    0
#define FIREYEYE_VERSION_PATCH    0

/* Sample Configuration */
#define FIREYEYE_SAMPLE_RATE_HZ   10      /* Sample rate in Hz */
#define FIREYEYE_SAMPLE_PERIOD_US (1000000 / FIREYEYE_SAMPLE_RATE_HZ)

/* Filter Configuration */
#define FILTER_MOVING_AVG_WINDOW  10      /* Moving average window size */
#define FILTER_LOW_PASS_ALPHA     0.3f    /* Low pass filter coefficient (0-1) */

/* Current Threshold (Amps) */
#define CURRENT_WARNING_THRESHOLD 5.0f    /* Warning threshold */
#define CURRENT_ALARM_THRESHOLD   8.0f    /* Alarm threshold */

/* Temperature Threshold (Celsius) */
#define TEMP_WARNING_THRESHOLD    45.0f   /* Warning threshold */
#define TEMP_ALARM_THRESHOLD      60.0f   /* Alarm threshold */

/* Debounce Configuration */
#define DEBOUNCE_COUNT            3       /* Consecutive count before trigger */

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

/* Network Configuration */
#define NETWORK_MQTT_BROKER       "mqtt.example.com"
#define NETWORK_MQTT_PORT         1883
#define NETWORK_MQTT_TOPIC_ALERT  "fireeye/alert"
#define NETWORK_MQTT_TOPIC_STATUS "fireeye/status"

/* Storage Configuration */
#define STORAGE_MAX_RECORDS       1000
#define STORAGE_RECORD_SIZE       sizeof(sensor_data_t)

/* GPIO Pin Definitions (GD32F470V-START) */
#define PIN_BUZZER                GPIO_PIN_0   /* PA0 - Buzzer */
#define PIN_LED_WARNING           GPIO_PIN_1   /* PA1 - Warning LED */
#define PIN_LED_ALARM             GPIO_PIN_2   /* PA2 - Alarm LED */
#define PIN_RELAY                 GPIO_PIN_3   /* PA3 - Shutdown Relay */

/* ADC Channel Definitions */

/* 真实传感器配置（火眼 P1：电流 + 温度 + 报警输出 + 继电器联动） *********/

#define FIREYEYE_USE_REAL_SENSORS   1       /* 0 = 强制使用模拟数据（无硬件时调试用） */

#define FIREYEYE_ADC_VREF_VOLTS     3.3f    /* ADC 参考电压 = VDDA */
#define FIREYEYE_ADC_FULLSCALE      4096.0f /* 12bit */

/* 电流：ACS712-30A（5V 供电，静态 2.5V，66mV/A），输出经 10k+10k 分压后进 PA4 */

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

#define ADC_CHANNEL_CURRENT       0   /* ADC Channel 0 - Current sensor */
#define ADC_CHANNEL_TEMPERATURE   1   /* ADC Channel 1 - Temperature sensor */
#define ADC_CHANNEL_LEAKAGE       2   /* ADC Channel 2 - Leakage sensor */

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
  int index;                  /* Current index */
  float sum;                  /* Sum of buffer */
  float alpha;                /* Low pass filter coefficient */
  float last_output;          /* Last output value */
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
