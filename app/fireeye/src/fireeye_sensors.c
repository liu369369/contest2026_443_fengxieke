/****************************************************************************
 * FireEye - 传感器与执行器实现
 *
 * 硬件访问通过板级 API（<arch/board/board.h>）完成：
 *   gd32_fireeye_adc_sample()   ADC 原始码值（12bit）
 *   gd32_fireeye_set_buzzer()   蜂鸣器
 *   gd32_fireeye_set_relay()    继电器（联动断电）
 *   gd32_fireeye_key_pressed()  板载按键
 *
 * 接线与量程（详见 docs/开发板接线.md）：
 *   电流：ACS712-30A 模块（5V 供电，静态 2.5V，66mV/A）
 *         输出经 10kΩ+10kΩ 分压后接 PA4（ADC0_IN4）
 *         分压后零点是 1.25V，灵敏度 33mV/A
 *   温度：10kΩ 上拉到 3V3，与 NTC(10k B3950) 分压，中点接 PA6（ADC0_IN6）
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <math.h>
#include <errno.h>
#include <stdbool.h>
#include <syslog.h>

#include <arch/board/board.h>

#include "include/fireeye_config.h"
#include "include/fireeye_sensors.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ADC_OVERSAMPLE        8       /* 每次采样取平均的样本数 */
#define ADC_ZERO_SAMPLES      64      /* 电流零点校准样本数 */
#define KEY_DEBOUNCE_COUNT    3       /* 按键消抖次数（主循环 10Hz → 300ms） */
#define ALARM_BEEP_TICKS      5       /* 预警间歇响：5 个 tick 翻转一次（1Hz） */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool  g_ready;
static float g_current_zero_volts;    /* 电流通道零点（分压后的电压） */
static int   g_beep_tick;
static bool  g_key_last;
static int   g_key_debounce;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static float adc_code_to_volts(int code)
{
  return (float)code * FIREYEYE_ADC_VREF_VOLTS / FIREYEYE_ADC_FULLSCALE;
}

/* 单通道多次采样取平均，失败返回负错误码 */

static int adc_read_average(int channel)
{
  int i;
  int code;
  long sum = 0;

  for (i = 0; i < ADC_OVERSAMPLE; i++)
    {
      code = gd32_fireeye_adc_sample(channel);
      if (code < 0)
        {
          return code;
        }

      sum += code;
    }

  return (int)(sum / ADC_OVERSAMPLE);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int fireeye_sensors_init(void)
{
  int ret;
  int i;
  long sum = 0;

  ret = gd32_fireeye_hw_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye: board hardware init failed: %d\n", ret);
      return ret;
    }

  /* 电流零点校准：ACS712 静态输出 = Vcc/2，经 1/2 分压后约 1.25V。
   *
   * 关键：校准必须在负载不工作时进行。开机时继电器若已放开，负载（风扇/充电器）
   * 可能正在工作，那样会把负载电流当成零点，之后负载一停读数就变成负值。
   * 因此这里先让继电器吸合（断开负载）0.5 秒，校准完再释放。 */

  gd32_fireeye_set_relay(true);    /* 断开负载 */
  usleep(500000);

  for (i = 0; i < ADC_ZERO_SAMPLES; i++)
    {
      ret = gd32_fireeye_adc_sample(FIREYEYE_ADC_CH_CURRENT);
      if (ret < 0)
        {
          syslog(LOG_ERR, "FireEye: current channel sampling failed: %d\n", ret);
          return ret;
        }

      sum += ret;
    }

  g_current_zero_volts =
    adc_code_to_volts((int)(sum / ADC_ZERO_SAMPLES));

  gd32_fireeye_set_relay(false);   /* 恢复负载供电 */

  syslog(LOG_INFO, "FireEye: current zero offset = %.3f V (load disconnected)\n",
         (double)g_current_zero_volts);

  /* 打印两路 ADC 的原始码值与电压，便于用万用表对照判断接线是否正常 */

  int raw_cur = adc_read_average(FIREYEYE_ADC_CH_CURRENT);
  int raw_tmp = adc_read_average(FIREYEYE_ADC_CH_TEMPERATURE);

  syslog(LOG_INFO,
         "FireEye: ADC PA4(ch%d)=%d (%.3f V), PA6(ch%d)=%d (%.3f V)\n",
         FIREYEYE_ADC_CH_CURRENT, raw_cur,
         raw_cur >= 0 ? (double)adc_code_to_volts(raw_cur) : 0.0,
         FIREYEYE_ADC_CH_TEMPERATURE, raw_tmp,
         raw_tmp >= 0 ? (double)adc_code_to_volts(raw_tmp) : 0.0);

  g_ready = true;
  return 0;
}

bool fireeye_sensors_ready(void)
{
  return g_ready;
}

float fireeye_sensors_read_current(void)
{
  int code;
  float volts;

  if (!g_ready)
    {
      return NAN;
    }

  code = adc_read_average(FIREYEYE_ADC_CH_CURRENT);
  if (code < 0)
    {
      return NAN;
    }

  volts = adc_code_to_volts(code);

  /* 分压后的灵敏度：66mV/A ÷ 2 = 33mV/A */

  return (volts - g_current_zero_volts) / FIREYEYE_ACS712_VOLTS_PER_AMP;
}

float fireeye_sensors_read_temperature(void)
{
  int code;
  float volts;
  float resistance;
  float kelvin;

  if (!g_ready)
    {
      return NAN;
    }

  code = adc_read_average(FIREYEYE_ADC_CH_TEMPERATURE);
  if (code < 0)
    {
      return NAN;
    }

  volts = adc_code_to_volts(code);

  /* 分压中点电压为 0 或接近 VREF，说明 NTC 短路/开路 */

  if (volts <= 0.02f || volts >= (FIREYEYE_ADC_VREF_VOLTS - 0.02f))
    {
      return NAN;
    }

  /* V = VREF * Rntc / (Rseries + Rntc)  →  Rntc = Rseries * V / (VREF - V) */

  resistance = FIREYEYE_NTC_R_SERIES_OHM * volts / (FIREYEYE_ADC_VREF_VOLTS - volts);

  /* B 参数方程：1/T = 1/T0 + ln(R/R0)/B */

  kelvin = 1.0f / (1.0f / FIREYEYE_NTC_T0_KELVIN +
                   logf(resistance / FIREYEYE_NTC_R0_OHM) / FIREYEYE_NTC_B_VALUE);

  float celsius = kelvin - 273.15f;

  /* 合理性检查：超出 -40~150℃ 说明接线异常（接触不良/接错脚），判为无效 */

  if (celsius < FIREYEYE_TEMP_MIN_C || celsius > FIREYEYE_TEMP_MAX_C)
    {
      return NAN;
    }

  return celsius;
}

bool fireeye_sensors_key_pressed(void)
{
  bool now = gd32_fireeye_key_pressed();

  if (now == g_key_last)
    {
      if (g_key_debounce < KEY_DEBOUNCE_COUNT)
        {
          g_key_debounce++;
        }
    }
  else
    {
      g_key_last = now;
      g_key_debounce = 1;
    }

  return now && (g_key_debounce >= KEY_DEBOUNCE_COUNT);
}

void fireeye_sensors_alarm_output(system_state_t state)
{
  g_beep_tick++;

  switch (state)
    {
      case STATE_NORMAL:
        gd32_fireeye_set_buzzer(false);
        gd32_fireeye_set_alarm_led(false);
        gd32_fireeye_set_relay(false);
        break;

      case STATE_WARNING:

        /* 预警：蜂鸣器间歇响（1Hz）+ 报警灯慢闪，继电器不动作 */

        gd32_fireeye_set_buzzer(((g_beep_tick / ALARM_BEEP_TICKS) % 2) == 0);
        gd32_fireeye_set_alarm_led(((g_beep_tick / ALARM_BEEP_TICKS) % 2) == 0);
        gd32_fireeye_set_relay(false);
        break;

      case STATE_ALARM:
      case STATE_SHUTDOWN:

        /* 报警/断电：蜂鸣器长鸣 + 报警灯常亮，继电器动作切断负载 */

        gd32_fireeye_set_buzzer(true);
        gd32_fireeye_set_alarm_led(true);
        gd32_fireeye_set_relay(true);
        break;

      default:
        gd32_fireeye_set_buzzer(false);
        gd32_fireeye_set_alarm_led(false);
        gd32_fireeye_set_relay(false);
        break;
    }
}

void fireeye_sensors_set_led(bool on)
{
  gd32_fireeye_set_alarm_led(on);
}

void fireeye_sensors_pin_raw(int which, bool high)
{
  gd32_fireeye_pin_raw(which, high);
}
