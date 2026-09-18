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
#include "include/fireeye_net.h"
#include <nuttx/progmem.h>

#include "include/fireeye_storage.h"

/* 板端存证（片上 Flash）在本板实测不可写：这颗 GD32F407VK 上只有极少数地址
 * 能被编程，同一扇区同一页内也会失败（实测记录见 drafts/说明_20260918m.txt）。
 * 演示固件里整块关闭，避免每次状态变化都往串口刷一行 write failed。 */

#if defined(CONFIG_FIREYEYE_STORAGE) && defined(CONFIG_ARCH_HAVE_PROGMEM)
#  define FIREEYE_STORAGE_ON 1
#else
#  define FIREEYE_STORAGE_ON 0
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 串口输出分级：0 = 只留告警与错误（默认），1 = 加上一般信息，2 = 全部调试信息。
 *
 * 默认取 0 的原因：板级 W5500 驱动每 5~10 秒会打印诊断信息，
 * 加上本应用自己的周期数据行，会把 115200 的控制台刷满，
 * 既干扰 nsh 输入，也让"看关键事件"变得困难。 */

static int g_verbosity;

static void fireeye_set_verbosity(int level)
{
  if (level < 0)
    {
      level = 0;
    }
  else if (level > 2)
    {
      level = 2;
    }

  g_verbosity = level;

  /* Flat build 下 syslog 掩码是一个全局掩码，所以这一处设置会同时
   * 收敛应用、内核与驱动侧的 INFO/DEBUG 输出。 */

  if (level >= 2)
    {
      setlogmask(LOG_UPTO(LOG_DEBUG));
    }
  else if (level == 1)
    {
      setlogmask(LOG_UPTO(LOG_INFO));
    }
  else
    {
      setlogmask(LOG_UPTO(LOG_WARNING));
    }
}

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

/* 应用启动时刻，用于上报运行时长 */
static time_t g_start_time;

/* 上一次状态，用于只在状态变化时打印 */
static system_state_t g_last_state = STATE_NORMAL;

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
  /* 这里不做逐样本落盘：10Hz 写 Flash 会把擦除块很快写坏。
   * 历史记录统一走 fireeye_storage_append()，只在"状态变化"与
   * "60 秒心跳"时写入片上 Flash，断电重启后可用 `fireeye -e` 回放。 */

  UNUSED(data);
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

  /* 恢复片上 Flash 里的历史记录（断电后仍可回看报警过程） */

#if FIREEYE_STORAGE_ON
  ret = fireeye_storage_init();
  if (ret < 0)
    {
      syslog(LOG_ERR,
             "FireEye: storage init failed(%d), history disabled\n", ret);
    }
  else
    {
      syslog(LOG_INFO, "FireEye: storage ready, %d records restored\n", ret);

      if (ret > 0)
        {
          fireeye_storage_show(5);
        }
    }
#else
  syslog(LOG_INFO, "FireEye: local storage disabled in this build\n");
#endif

  syslog(LOG_INFO, "FireEye Initialization Complete\n");

  /* 安静档下至少留一行"我起来了"，否则控制台一片空白会让人怀疑没跑 */

  if (g_verbosity > 0)
    {
      syslog(LOG_INFO,
             "FireEye running (periodic data row every 10 s)\n");
    }
  else
    {
      syslog(LOG_WARNING,
             "FireEye running (quiet: only warnings; "
             "'fireeye -v' for details)\n");
    }

#ifdef CONFIG_FIREYEYE_NET
  /* 启动联网上报（W5500）；失败不影响本地采样与报警 */

  ret = fireeye_net_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye: net init failed(%d), local mode only\n", ret);
    }
#endif
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

/**
 * @brief 引脚电平排查：每个输出脚先低 2 秒、再高 2 秒
 * @return 0
 */

static int fireeye_level_test(void)
{
  static const char *names[3] = { "buzzer(PB1)", "relay(PB0)", "led(PD9)" };
  int i;

  syslog(LOG_INFO, "FireEye level test: each output pin LOW 2s then HIGH 2s (watch the module)\n");

  for (i = 0; i < 3; i++)
    {
      syslog(LOG_INFO, "FireEye level test: %s -> LOW\n", names[i]);
      fireeye_sensors_pin_raw(i, false);
      usleep(2000000);

      syslog(LOG_INFO, "FireEye level test: %s -> HIGH\n", names[i]);
      fireeye_sensors_pin_raw(i, true);
      usleep(2000000);
    }

  syslog(LOG_INFO, "FireEye level test: done\n");
  return 0;
}

/**
 * @brief 存证自检：写一条测试记录并重新扫描整片记录区回读
 * @return 0 成功，负值为错误码
 */

static int fireeye_store_test(void)
{
  int before;
  int after;
  int ret;

  before = fireeye_storage_init();
  if (before < 0)
    {
      syslog(LOG_ERR, "FireEye store: init failed (%d)\n", before);
      return before;
    }

  {
    const uint8_t *head = (const uint8_t *)up_progmem_getaddress(0);

    syslog(LOG_INFO, "FireEye store: %d records before, region head "
           "%02x %02x %02x %02x %02x %02x %02x %02x\n", before,
           head[0], head[1], head[2], head[3],
           head[4], head[5], head[6], head[7]);
  }

  /* 特征值：I=1.23A / T=45.6C / uptime=43981s，回放时一眼能认出来 */

  ret = fireeye_storage_append((uint8_t)STATE_NORMAL, false, 1.23f,
                               45.6f, 0xABCDu);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye store: append failed (%d)\n", ret);
      return ret;
    }

  /* 重新从 Flash 扫描一遍：能读回来才算真的写进去 */

  after = fireeye_storage_init();
  if (after < 0)
    {
      syslog(LOG_ERR, "FireEye store: rescan failed (%d)\n", after);
      return after;
    }

  if (after != before + 1)
    {
      syslog(LOG_ERR,
             "FireEye store: FAILED, %d -> %d records after append\n",
             before, after);
      return -1;
    }

  syslog(LOG_INFO, "FireEye store: OK, %d -> %d records\n",
         before, after);

  return fireeye_storage_show(3);
}

/* 临时诊断：直接读 FMC 寄存器，并在未使用的 bank0 区域做字节/字编程测试。
 * 用途：区分"存证区地址不存在"和"FMC 编程通路本身有问题"。*/

#define FIREEYE_FMC_WS     (0x40023c00u + 0x00u)
#define FIREEYE_FMC_KEY    (0x40023c00u + 0x04u)
#define FIREEYE_FMC_STAT   (0x40023c00u + 0x0cu)
#define FIREEYE_FMC_CTL    (0x40023c00u + 0x10u)
#define FIREEYE_FMC_OBCTL0 (0x40023c00u + 0x14u)
#define FIREEYE_FMC_PID    (0x40023c00u + 0x100u)

static uint32_t fireeye_fmc_rd(uint32_t addr)
{
  return *(volatile uint32_t *)addr;
}

static void fireeye_fmc_wr(uint32_t addr, uint32_t value)
{
  *(volatile uint32_t *)addr = value;
}

static void fireeye_fmc_wait(void)
{
  int guard = 2000000;

  while ((fireeye_fmc_rd(FIREEYE_FMC_STAT) & (1u << 16)) != 0 &&
         guard-- > 0)
    {
    }
}

static int fireeye_flash_probe(void)
{
  volatile uint8_t *baddr = (volatile uint8_t *)0x08080000u;
  volatile uint32_t *waddr = (volatile uint32_t *)0x08080004u;
  uint16_t size_kb = *(volatile uint16_t *)0x1fff7a22u;
  uint32_t ctl;

  syslog(LOG_INFO, "FireEye flash: FMC_SIZE=%u KB, PID=0x%08lx\n",
         (unsigned)size_kb, (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_PID));
  {
    static const uint32_t probe[4] =
      { 0x08080000u, 0x080a0000u, 0x080c0000u, 0x08100000u };
    int k;

    for (k = 0; k < 4; k++)
      {
        const uint8_t *ptr = (const uint8_t *)probe[k];

        syslog(LOG_INFO,
               "FireEye flash: %08lx = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               (unsigned long)probe[k], ptr[0], ptr[1], ptr[2], ptr[3],
               ptr[4], ptr[5], ptr[6], ptr[7]);
      }
  }

  syslog(LOG_INFO, "FireEye flash: CTL=0x%08lx STAT=0x%08lx OBCTL0=0x%08lx\n",
         (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_CTL),
         (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_STAT),
         (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_OBCTL0));
  syslog(LOG_INFO, "FireEye flash: WS(ACR)=0x%08lx\n",
         (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_WS));

  {
    const uint8_t *ptr = (const uint8_t *)0x08080000u;
    int k;

    for (k = 0; k < 64; k += 8)
      {
        syslog(LOG_INFO,
               "FireEye flash: 080800%02x = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               k, ptr[k], ptr[k + 1], ptr[k + 2], ptr[k + 3],
               ptr[k + 4], ptr[k + 5], ptr[k + 6], ptr[k + 7]);
      }
  }

  ctl = fireeye_fmc_rd(FIREEYE_FMC_CTL);
  if ((ctl & 0x80000000u) != 0)
    {
      fireeye_fmc_wr(FIREEYE_FMC_KEY, 0x45670123u);
      fireeye_fmc_wr(FIREEYE_FMC_KEY, 0xcdef89abu);
      syslog(LOG_INFO, "FireEye flash: unlocked, CTL=0x%08lx\n",
             (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_CTL));
    }

  fireeye_fmc_wr(FIREEYE_FMC_STAT, (1u << 4));

  /* 字节编程：0x08080000（bank0，固件 320KB 之后、存证区之前） */

  ctl = fireeye_fmc_rd(FIREEYE_FMC_CTL);
  fireeye_fmc_wr(FIREEYE_FMC_CTL, (ctl & ~(3u << 8)) | (0u << 8));
  fireeye_fmc_wr(FIREEYE_FMC_CTL, fireeye_fmc_rd(FIREEYE_FMC_CTL) | 1u);
  *baddr = 0x5au;
  fireeye_fmc_wait();
  fireeye_fmc_wr(FIREEYE_FMC_CTL, fireeye_fmc_rd(FIREEYE_FMC_CTL) & ~1u);
  syslog(LOG_INFO,
         "FireEye flash: byte@0x08080000 wrote 0x5a read 0x%02x STAT=0x%08lx\n",
         (unsigned)*baddr, (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_STAT));

  /* 字编程：0x08080004 */

  fireeye_fmc_wr(FIREEYE_FMC_STAT, (1u << 4));
  ctl = fireeye_fmc_rd(FIREEYE_FMC_CTL);
  fireeye_fmc_wr(FIREEYE_FMC_CTL, (ctl & ~(3u << 8)) | (2u << 8));
  fireeye_fmc_wr(FIREEYE_FMC_CTL, fireeye_fmc_rd(FIREEYE_FMC_CTL) | 1u);
  *waddr = 0x11223344u;
  fireeye_fmc_wait();
  fireeye_fmc_wr(FIREEYE_FMC_CTL, fireeye_fmc_rd(FIREEYE_FMC_CTL) & ~1u);
  syslog(LOG_INFO,
         "FireEye flash: word@0x08080004 wrote 0x11223344 read 0x%08lx STAT=0x%08lx\n",
         (unsigned long)*waddr, (unsigned long)fireeye_fmc_rd(FIREEYE_FMC_STAT));

  /* 重新上锁 */

  fireeye_fmc_wr(FIREEYE_FMC_CTL, fireeye_fmc_rd(FIREEYE_FMC_CTL) | 0x80000000u);
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

  g_start_time = time(NULL);

  /* 子命令：fireeye test —— 只做硬件自检（蜂鸣器/继电器），跑完即退出 */

  /* 子命令：fireeye level —— 逐脚输出 LOW/HIGH，用于确认模块触发极性 */

  if (argc > 1 && strcmp(argv[1], "level") == 0)
    {
      if (fireeye_sensors_init() < 0)
        {
          syslog(LOG_ERR, "FireEye level test: sensor init failed\n");
          return -1;
        }

      return fireeye_level_test();
    }

  /* 子命令：fireeye store —— 立刻写一条记录并回读，验证存证通路 */

  if (argc > 1 && strcmp(argv[1], "store") == 0)
    {
      return (fireeye_store_test() < 0) ? 1 : 0;
    }

  /* 子命令：fireeye flash —— 临时诊断，打印 FMC 状态并做编程测试 */

  if (argc > 1 && strcmp(argv[1], "flash") == 0)
    {
      return fireeye_flash_probe();
    }

  /* 子命令：fireeye adc —— 打印继电器两种状态下的原始 ADC 码值 */

  if (argc > 1 && strcmp(argv[1], "adc") == 0)
    {
      if (fireeye_sensors_init() < 0)
        {
          syslog(LOG_ERR, "FireEye ref test: sensor init failed\n");
          return -1;
        }

      return (fireeye_sensors_offset_test() < 0) ? 1 : 0;
    }

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

      /* 传感器无效提示：每 60 秒一次即可，10 秒一次太吵 */

      if ((!g_current_valid || !g_temp_valid) && (++sensor_fault_tick >= 600))
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

      /* 把最新数据交给联网上报任务 */

      fireeye_net_update(filtered_current, filtered_temp, (int)new_state,
                         new_state >= STATE_ALARM,
                         (uint32_t)(time(NULL) - g_start_time));

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
      static int storage_counter = 0;
      /* 状态变化时打印一条（关键事件） */

      if (new_state != g_last_state)
        {
          /* 进入预警/报警属于需要留意的状态，用 WARNING 级别，
           * 这样安静档下也照样能看到；恢复正常用 INFO。 */

          syslog(new_state >= STATE_WARNING ? LOG_WARNING : LOG_INFO,
                 "FireEye: state %s -> %s (I=%.2f A, T=%.1f C)\n",
                 fsm_state_to_string(g_last_state),
                 fsm_state_to_string(new_state),
                 (double)filtered_current, (double)filtered_temp);

          /* 状态变化落盘：断电重启后仍能回看这次报警 */

          fireeye_storage_append((uint8_t)new_state,
                                 new_state >= STATE_ALARM,
                                 filtered_current, filtered_temp,
                                 (uint32_t)(time(NULL) - g_start_time));
          g_last_state = new_state;
        }

      /* 每 10 秒打印一次数据行 */

      if (++report_counter >= (10 * FIREYEYE_SAMPLE_RATE_HZ))
        {
          report_data(&data);
          report_counter = 0;
        }

      /* 每 60 秒写一条心跳记录，便于事后回看趋势 */

      if (++storage_counter >= (60 * FIREYEYE_SAMPLE_RATE_HZ))
        {
          storage_counter = 0;
          fireeye_storage_append((uint8_t)new_state,
                                 new_state >= STATE_ALARM,
                                 filtered_current, filtered_temp,
                                 (uint32_t)(time(NULL) - g_start_time));
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
  /* 参数：
   *   -q        安静档（默认）：只输出告警与错误
   *   -v        一般信息：再加周期数据行、启动信息等
   *   -vv       调试信息：全开（含驱动诊断）
   *   -e [n]    只回放片上 Flash 里的历史记录，不启动监测
   *   test/level  硬件自检子命令（由 fireeye_main_task 处理）
   */

  bool events = false;
  int  max = 20;
  int  level = 0;
  int  i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "-q") == 0)
        {
          level = 0;
        }
      else if (strcmp(argv[i], "-vv") == 0)
        {
          level = 2;
        }
      else if (strcmp(argv[i], "-v") == 0)
        {
          if (level < 1)
            {
              level = 1;
            }
        }
      else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--events") == 0)
        {
          events = true;

          if (i + 1 < argc)
            {
              max = atoi(argv[i + 1]);
            }
        }
    }

  /* 回放历史与硬件自检本身就是"打印给人看"的命令，安静档下也放行 INFO */

  if (level < 1 &&
      (events || (argc > 1 &&
                  (strcmp(argv[1], "test") == 0 ||
                   strcmp(argv[1], "level") == 0 ||
                   strcmp(argv[1], "store") == 0 ||
                   strcmp(argv[1], "adc") == 0 ||
                   strcmp(argv[1], "flash") == 0))))
    {
      level = 1;
    }

  fireeye_set_verbosity(level);

  if (events)
    {
#if FIREEYE_STORAGE_ON
      if (fireeye_storage_init() < 0)
        {
          syslog(LOG_ERR, "FireEye: local storage unavailable\n");
          return 1;
        }

      return (fireeye_storage_show(max) < 0) ? 1 : 0;
#else
      syslog(LOG_ERR, "FireEye: local storage disabled in this build\n");
      (void)max;
      return 1;
#endif
    }

  return fireeye_main_task(argc, argv);
}
