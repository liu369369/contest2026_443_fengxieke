/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Main Application
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <syslog.h>

#include "include/fireeye_config.h"
#include "include/fireeye_filter.h"
#include "include/fireeye_threshold.h"
#include "include/fireeye_fsm.h"
#include "include/oled_display.h"
#include "include/fireeye_sensors.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Filter contexts */
static filter_ctx_t g_current_filter;
static filter_ctx_t g_temp_filter;

/* Threshold configurations */
static threshold_config_t g_current_threshold_config;
static threshold_config_t g_temp_threshold_config;

/* Threshold states */
static threshold_state_t g_current_threshold_state;
static threshold_state_t g_temp_threshold_state;

/* State machine context */
static fsm_ctx_t g_fsm;

/* Latest sensor data */
static sensor_data_t g_latest_data;

/* 按键（人工复位）上一次的电平，用于边沿检测 */
static bool g_key_prev;

/* 传感器有效性：采样失败时保留上一次有效值，但把标记置为 false */
static bool  g_current_valid = true;
static bool  g_temp_valid    = true;
static float g_last_current;
static float g_last_temp     = 25.0f;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief Read current sensor value (ADC)
 * @return Current in Amps
 */

static float read_current_sensor(void)
{
#if FIREYEYE_USE_REAL_SENSORS
  /* 真实 ADC 采样。失败返回 NAN，由主循环保留上一次有效值并给出提示，
   * 不再退回模拟数据（否则会让人误以为读到了真实电流）。 */

  return fireeye_sensors_read_current();
#else
  /* 模拟数据：仅在 FIREYEYE_USE_REAL_SENSORS = 0 时使用 */
  static float simulated_current = 2.0f;
  static int tick = 0;

  tick++;

  /* Simulate current variations */
  if (tick > 200 && tick < 250)
    {
      /* Simulate over-current condition */
      simulated_current = 8.0f + 2.0f * sinf(tick * 0.1f);
    }
  else if (tick >= 100 && tick < 200)
    {
      /* Simulate gradually increasing current */
      simulated_current = 2.0f + 0.03f * (tick - 100);
    }
  else
    {
      /* Normal current */
      simulated_current = 2.0f + 0.5f * sinf(tick * 0.05f);
    }

  return simulated_current;
#endif
}

/**
 * @brief Read temperature sensor value
 * @return Temperature in Celsius
 */

static float read_temperature_sensor(void)
{
#if FIREYEYE_USE_REAL_SENSORS
  /* 真实 ADC 采样。无效（NTC 开路/短路）时返回 NAN，由主循环处理。 */

  return fireeye_sensors_read_temperature();
#else
  /* 模拟数据：仅在 FIREYEYE_USE_REAL_SENSORS = 0 时使用 */
  static float simulated_temp = 25.0f;
  static int tick = 0;

  tick++;

  /* Simulate temperature variations */
  if (tick > 200 && tick < 250)
    {
      /* Simulate over-temperature condition */
      simulated_temp = 55.0f + 5.0f * sinf(tick * 0.2f);
    }
  else if (tick >= 100 && tick < 200)
    {
      /* Simulate gradually increasing temperature */
      simulated_temp = 25.0f + 0.2f * (tick - 100);
    }
  else
    {
      /* Normal temperature */
      simulated_temp = 25.0f + 2.0f * sinf(tick * 0.02f);
    }

  return simulated_temp;
#endif
}

/**
 * @brief Check leakage sensor
 * @return true if leakage detected, false otherwise
 */

static bool check_leakage_sensor(void)
{
  /* 本板目前没有漏电互感器，恒返回 false。
   * 旧代码会在运行 20 秒后模拟出"漏电"，并把状态机直接推到 ALARM，
   * 与真实电流/温度数据混在一起，容易误判，故去掉模拟。 */

  return false;
}

/**
 * @brief OLED 整行显示（右侧补空格，避免旧字符残留）
 * @param page 显示页（0~7）
 * @param text 文本
 */

static void oled_show_line(int page, const char *text)
{
  char buf[22];

  snprintf(buf, sizeof(buf), "%-21s", text);
  oled_showstr(page, buf);
}

/**
 * @brief Save data to storage
 * @param data Pointer to sensor data
 */

static void save_data(const sensor_data_t *data)
{
  /* TODO: Implement actual storage */
  /* For now, just log */
  syslog(LOG_DEBUG, "Data saved: I=%.2fA T=%.1fC L=%d S=%s\n",
         data->current, data->temperature, data->leakage,
         fsm_state_to_string(data->state));
}

/**
 * @brief Report data via network
 * @param data Pointer to sensor data
 */

static void report_data(const sensor_data_t *data)
{
  /* TODO: Implement actual network reporting */
  /* For now, just log */
  /* 每 10 秒打印一次实时数值（LOG_INFO，串口可见） */

  syslog(LOG_INFO, "FireEye: I=%.2f A, T=%.1f C, state=%s\n",
         (double)data->current, (double)data->temperature,
         fsm_state_to_string(data->state));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int fireeye_init(void)
{
  int ret;

  syslog(LOG_INFO, "FireEye v%d.%d.%d Initializing...\n",
         FIREYEYE_VERSION_MAJOR, FIREYEYE_VERSION_MINOR,
         FIREYEYE_VERSION_PATCH);

  /* Initialize filters */
  ret = filter_init(&g_current_filter,
                    FILTER_MOVING_AVG_WINDOW,
                    FILTER_LOW_PASS_ALPHA);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to init current filter: %d\n", ret);
      return ret;
    }

  ret = filter_init(&g_temp_filter,
                    FILTER_MOVING_AVG_WINDOW,
                    FILTER_LOW_PASS_ALPHA);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to init temp filter: %d\n", ret);
      return ret;
    }

  /* Initialize threshold configurations */
  ret = threshold_init(&g_current_threshold_config,
                       CURRENT_WARNING_THRESHOLD,
                       CURRENT_ALARM_THRESHOLD,
                       DEBOUNCE_COUNT,
                       SLOPE_THRESHOLD);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to init current threshold: %d\n", ret);
      return ret;
    }

  ret = threshold_init(&g_temp_threshold_config,
                       TEMP_WARNING_THRESHOLD,
                       TEMP_ALARM_THRESHOLD,
                       DEBOUNCE_COUNT,
                       SLOPE_THRESHOLD);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to init temp threshold: %d\n", ret);
      return ret;
    }

  /* Reset threshold states */
  threshold_reset(&g_current_threshold_state);
  threshold_reset(&g_temp_threshold_state);

  /* Initialize state machine */
  ret = fsm_init(&g_fsm);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to init FSM: %d\n", ret);
      return ret;
    }

  /* Clear latest data */
  memset(&g_latest_data, 0, sizeof(sensor_data_t));

#if FIREYEYE_USE_REAL_SENSORS
  /* 初始化真实传感器（ADC/GPIO）；失败则退回模拟数据 */

  ret = fireeye_sensors_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye: sensor init failed(%d); readings will be invalid\n",
             ret);
    }
#endif

  syslog(LOG_INFO, "FireEye Initialization Complete\n");
  ret = oled_init();
  if (ret < 0)
    {
      syslog(LOG_ERR,
             "FireEye: OLED init FAILED (%d): -19 = no /dev/i2c1, "
             "-5 = no ACK (wiring/address), check SDA=JP5-25 SCL=JP5-26\n", ret);
    }
  else
    {
      syslog(LOG_INFO, "FireEye: OLED init OK\n");
      oled_show_line(0, "FireEye");
    }

  return 0;
}

/**
 * @brief 硬件自检：蜂鸣器 + 继电器（供 `fireeye test` 使用）
 * @return 0
 */

static int fireeye_hw_test(void)
{
  int i;

  syslog(LOG_INFO, "FireEye test: buzzer=PB1(JP5-16), relay=PB0(JP5-13)\n");

  /* 1) 蜂鸣器：3 短声（每次 0.3 秒，间隔 0.3 秒） */

  syslog(LOG_INFO, "FireEye test: buzzer 3 short beeps\n");

  for (i = 0; i < 3; i++)
    {
      fireeye_sensors_alarm_output(STATE_ALARM);
      usleep(300000);
      fireeye_sensors_alarm_output(STATE_NORMAL);
      usleep(300000);
    }

  /* 2) 继电器：吸合 2 秒后释放（模块会咔哒一声，指示灯亮/灭） */

  syslog(LOG_INFO, "FireEye test: relay ON for 2s (listen for a click)\n");
  fireeye_sensors_alarm_output(STATE_ALARM);
  usleep(2000000);
  fireeye_sensors_alarm_output(STATE_NORMAL);

  syslog(LOG_INFO, "FireEye test: relay OFF\n");

  /* 3) 预警音型：1Hz 间歇响 3 秒 */

  syslog(LOG_INFO, "FireEye test: warning beep pattern (1Hz, 3s)\n");

  for (i = 0; i < 6; i++)
    {
      fireeye_sensors_alarm_output(STATE_WARNING);
      usleep(500000);
    }

  fireeye_sensors_alarm_output(STATE_NORMAL);

  /* 4) 报警灯模块（PD9 = JP5 第 31 脚）：亮 1 秒、灭 1 秒，重复 3 次 */

  syslog(LOG_INFO, "FireEye test: alarm LED on PD9(JP5-31), 1s on / 1s off x3\n");

  for (i = 0; i < 3; i++)
    {
      fireeye_sensors_set_led(true);
      usleep(1000000);
      fireeye_sensors_set_led(false);
      usleep(1000000);
    }

  syslog(LOG_INFO, "FireEye test: done\n");
  return 0;
}

int fireeye_main_task(int argc, char *argv[])
{
  float raw_current;
  float raw_temp;
  bool leakage;
  float filtered_current;
  float filtered_temp;
  int current_status;
  int temp_status;
  int max_status;
  system_event_t event;
  system_state_t new_state;
  int display_counter = 0;
  int sensor_fault_tick = 0;
  sensor_data_t data;
  bool key_now;

  syslog(LOG_INFO, "FireEye Main Task Started\n");

  /* 子命令：fireeye test —— 只做硬件自检（蜂鸣器/继电器），跑完即退出 */

  if (argc > 1 && strcmp(argv[1], "test") == 0)
    {
      if (fireeye_sensors_init() < 0)
        {
          syslog(LOG_ERR, "FireEye test: sensor init failed\n");
          return -1;
        }

      return fireeye_hw_test();
    }

  /* Initialize all modules */
  if (fireeye_init() < 0)
    {
      syslog(LOG_ERR, "FireEye initialization failed!\n");
      return -1;
    }

  /* Main loop */
  while (1)
    {
      /* Read sensors */
      raw_current = read_current_sensor();
      raw_temp = read_temperature_sensor();
      leakage = check_leakage_sensor();

      /* 采样失败：保留上一次有效值驱动状态机，但标记为无效并限频告警 */

      if (isnan(raw_current))
        {
          raw_current = g_last_current;
          g_current_valid = false;
        }
      else
        {
          g_last_current = raw_current;
          g_current_valid = true;
        }

      if (isnan(raw_temp))
        {
          raw_temp = g_last_temp;
          g_temp_valid = false;
        }
      else
        {
          g_last_temp = raw_temp;
          g_temp_valid = true;
        }

      if ((!g_current_valid || !g_temp_valid) && (++sensor_fault_tick >= 100))
        {
          sensor_fault_tick = 0;
          syslog(LOG_WARNING,
                 "FireEye: sensor invalid (current=%s, temperature=%s); "
                 "current->PA4/JP5-7, temperature->PA6/JP5-9\n",
                 g_current_valid ? "ok" : "FAIL",
                 g_temp_valid ? "ok" : "FAIL");
        }

      /* Apply filters */
      filtered_current = filter_combined(&g_current_filter, raw_current);
      filtered_temp = filter_combined(&g_temp_filter, raw_temp);

      /* Check thresholds */
      current_status = threshold_check(&g_current_threshold_config,
                                       &g_current_threshold_state,
                                       filtered_current);
      temp_status = threshold_check(&g_temp_threshold_config,
                                    &g_temp_threshold_state,
                                    filtered_temp);

      /* Determine maximum status */
      if (leakage)
        {
          max_status = 2;  /* Leakage always triggers alarm */
        }
      else
        {
          max_status = (current_status > temp_status) ?
                       current_status : temp_status;
        }

      /* Process state machine */
      event = fsm_status_to_event(max_status);
      new_state = fsm_process_event(&g_fsm, event);

      /* 报警输出：蜂鸣器（预警间歇 / 报警长鸣）+ 继电器联动断电 */

      fireeye_sensors_alarm_output(new_state);

      /* 人工复位：按下板载按键 K2 */

      key_now = fireeye_sensors_key_pressed();
      if (key_now && !g_key_prev)
        {
          syslog(LOG_INFO, "FireEye: key pressed, FSM reset to NORMAL\n");
          fsm_reset(&g_fsm);
          fireeye_sensors_alarm_output(STATE_NORMAL);
        }

      g_key_prev = key_now;

      /* Prepare data record */
      data.current = filtered_current;
      data.temperature = filtered_temp;
      data.leakage = leakage;
      data.state = new_state;

      /* OLED：每秒刷新一次状态与数值 */

      if (++display_counter >= 10)
        {
          char line[24];

          display_counter = 0;
          oled_show_line(1, fsm_state_to_string(new_state));

          if (g_current_valid)
            {
              snprintf(line, sizeof(line), "I=%5.2fA", (double)filtered_current);
            }
          else
            {
              snprintf(line, sizeof(line), "I= --.-A");
            }

          oled_show_line(2, line);

          if (g_temp_valid)
            {
              snprintf(line, sizeof(line), "T=%5.2fC", (double)filtered_temp);
            }
          else
            {
              snprintf(line, sizeof(line), "T= --.-C");
            }

          oled_show_line(3, line);
        }
      data.timestamp = (uint32_t)time(NULL);

      /* Update latest data */
      memcpy(&g_latest_data, &data, sizeof(sensor_data_t));

      /* Save data */
      save_data(&data);

      /* Report via network (every 10 seconds) */
      static int report_counter = 0;
      if (++report_counter >= (10 * FIREYEYE_SAMPLE_RATE_HZ))
        {
          report_data(&data);
          report_counter = 0;
        }

      /* Sleep for sample period */
      usleep(FIREYEYE_SAMPLE_PERIOD_US);
    }

  return 0;
}

/****************************************************************************
 * Application Entry Point
 ****************************************************************************/

int main(int argc, char *argv[])
{
  return fireeye_main_task(argc, argv);
}
