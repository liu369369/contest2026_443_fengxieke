/****************************************************************************
 * FireEye - INA228 current and bus-voltage monitor
 *
 * The new carrier board uses a 2 mOhm shunt and the INA228 at address 0x40.
 * Current is calculated from the raw VSHUNT register.  This avoids relying
 * on the device CURRENT register and its SHUNT_CAL programming.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

#include <nuttx/i2c/i2c_master.h>
#include <arch/board/board.h>

#include "include/fireeye_config.h"
#include "include/fireeye_ina228.h"

#define FIREEYE_INA228_I2C_PATH    "/dev/i2c1"
#define FIREEYE_INA228_I2C_FREQ    100000

#define FIREEYE_INA228_REG_CONFIG          0x00
#define FIREEYE_INA228_REG_ADC_CONFIG      0x01
#define FIREEYE_INA228_REG_VSHUNT          0x04
#define FIREEYE_INA228_REG_VBUS            0x05
#define FIREEYE_INA228_REG_MANUFACTURER_ID 0x3e
#define FIREEYE_INA228_REG_DEVICE_ID       0x3f

#define FIREEYE_INA228_MANUFACTURER_ID     0x5449
#define FIREEYE_INA228_DEVICE_ID_MASK      0xfff0
#define FIREEYE_INA228_DEVICE_ID_VALUE     0x2280

/* ADCRANGE=1 gives +/-40.96mV range and 78.125nV/LSB. */

#define FIREEYE_INA228_VSHUNT_LSB_V        0.000000078125f
#define FIREEYE_INA228_VBUS_LSB_V          0.0001953125f

static int  g_ina_fd = -1;
static bool g_ina_ready;

static int ina_read(uint8_t reg, uint8_t *buffer, uint32_t length)
{
  /* The GD32 NuttX hardware-I2C receive state machine corrupts multi-byte
   * reads.  Use the board bit-bang transport validated on the same pins.
   */

  return gd32_fireeye_i2c_read(FIREYEYE_INA228_I2C_ADDR, reg, buffer,
                               length);
}

static int ina_write_u16(uint8_t reg, uint16_t value)
{
  uint8_t buffer[3];
  buffer[0] = reg;
  buffer[1] = (uint8_t)(value >> 8);
  buffer[2] = (uint8_t)value;
  return gd32_fireeye_i2c_write(FIREYEYE_INA228_I2C_ADDR, buffer,
                                sizeof(buffer));
}

static int ina_read_u16(uint8_t reg, uint16_t *value)
{
  uint8_t buffer[2];
  int ret;

  ret = ina_read(reg, buffer, sizeof(buffer));
  if (ret < 0)
    {
      return ret;
    }

  *value = ((uint16_t)buffer[0] << 8) | buffer[1];
  return 0;
}

static int ina_read_u24(uint8_t reg, uint32_t *value)
{
  uint8_t buffer[3];
  int ret;

  ret = ina_read(reg, buffer, sizeof(buffer));
  if (ret < 0)
    {
      return ret;
    }

  *value = ((uint32_t)buffer[0] << 16) |
           ((uint32_t)buffer[1] << 8) |
           (uint32_t)buffer[2];
  return 0;
}

static int32_t ina_signed_20bit(uint32_t value)
{
  int32_t sample = (int32_t)(value >> 4);

  if ((sample & 0x00080000) != 0)
    {
      sample |= 0xfff00000;
    }

  return sample;
}

int fireeye_ina228_init(void)
{
  struct fireeye_ina228_info_s info;
  uint16_t config;
  uint16_t adc_config;
  int ret;

  g_ina_ready = false;

  if (g_ina_fd >= 0)
    {
      close(g_ina_fd);
    }

  g_ina_fd = open(FIREEYE_INA228_I2C_PATH, O_RDWR);
  if (g_ina_fd < 0)
    {
      return -ENODEV;
    }

  ret = fireeye_ina228_get_info(&info);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye INA228: ID read failed (%d)\n", ret);
      goto errout;
    }

  if (info.manufacturer_id != FIREEYE_INA228_MANUFACTURER_ID ||
      (info.device_id & FIREEYE_INA228_DEVICE_ID_MASK) !=
      FIREEYE_INA228_DEVICE_ID_VALUE)
    {
      syslog(LOG_ERR,
             "FireEye INA228: unexpected IDs mfg=0x%04x id=0x%04x\n",
             info.manufacturer_id, info.device_id);
      ret = -ENODEV;
      goto errout;
    }

  /* ADCRANGE=1 gives +/-40.96mV, or about +/-20.48A with the 2mOhm shunt. */

  ret = ina_write_u16(FIREEYE_INA228_REG_CONFIG, 0x0010);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye INA228: CONFIG write failed (%d)\n", ret);
      goto errout;
    }

  /* Continuous shunt and bus voltage conversion, 16-sample averaging. */

  ret = ina_write_u16(FIREEYE_INA228_REG_ADC_CONFIG, 0xb142);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye INA228: ADC_CONFIG write failed (%d)\n", ret);
      goto errout;
    }

  ret = ina_read_u16(FIREEYE_INA228_REG_CONFIG, &config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye INA228: CONFIG readback failed (%d)\n", ret);
      goto errout;
    }

  ret = ina_read_u16(FIREEYE_INA228_REG_ADC_CONFIG, &adc_config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "FireEye INA228: ADC_CONFIG readback failed (%d)\n",
             ret);
      goto errout;
    }

  if ((config & 0x0010) == 0 || adc_config != 0xb142)
    {
      syslog(LOG_ERR,
             "FireEye INA228: config mismatch config=0x%04x adc=0x%04x\n",
             config, adc_config);
      ret = -EIO;
      goto errout;
    }

  g_ina_ready = true;
  syslog(LOG_INFO,
         "FireEye INA228: id=0x%04x config=0x%04x adc=0x%04x "
         "shunt=%.4f ohm\n",
         info.device_id, config, adc_config,
         (double)FIREYEYE_INA228_SHUNT_OHM);
  return 0;

errout:
  close(g_ina_fd);
  g_ina_fd = -1;
  return ret;
}

bool fireeye_ina228_ready(void)
{
  return g_ina_ready;
}

int fireeye_ina228_get_info(struct fireeye_ina228_info_s *info)
{
  int ret;

  if (info == NULL)
    {
      return -EINVAL;
    }

  ret = ina_read_u16(FIREEYE_INA228_REG_MANUFACTURER_ID,
                     &info->manufacturer_id);
  if (ret < 0)
    {
      return ret;
    }

  ret = ina_read_u16(FIREEYE_INA228_REG_DEVICE_ID, &info->device_id);
  if (ret < 0)
    {
      return ret;
    }

  ret = ina_read_u16(FIREEYE_INA228_REG_CONFIG, &info->config);
  if (ret < 0)
    {
      return ret;
    }

  return ina_read_u16(FIREEYE_INA228_REG_ADC_CONFIG, &info->adc_config);
}

int fireeye_ina228_read_shunt_raw(int32_t *raw)
{
  uint32_t value;
  int ret;

  if (!g_ina_ready)
    {
      return -ENODEV;
    }

  if (raw == NULL)
    {
      return -EINVAL;
    }

  ret = ina_read_u24(FIREEYE_INA228_REG_VSHUNT, &value);
  if (ret < 0)
    {
      return ret;
    }

  *raw = ina_signed_20bit(value);
  return 0;
}

int fireeye_ina228_read_current(float *current_a)
{
  int32_t shunt_raw;
  int ret;

  if (current_a == NULL)
    {
      return -EINVAL;
    }

  ret = fireeye_ina228_read_shunt_raw(&shunt_raw);
  if (ret < 0)
    {
      return ret;
    }

  *current_a = (float)shunt_raw * FIREEYE_INA228_VSHUNT_LSB_V /
               FIREYEYE_INA228_SHUNT_OHM;
  return 0;
}

int fireeye_ina228_read_bus_voltage(float *bus_v)
{
  uint32_t bus_raw;
  int ret;

  if (bus_v == NULL)
    {
      return -EINVAL;
    }

  if (!g_ina_ready)
    {
      return -ENODEV;
    }

  ret = ina_read_u24(FIREEYE_INA228_REG_VBUS, &bus_raw);
  if (ret < 0)
    {
      return ret;
    }

  *bus_v = (float)(bus_raw >> 4) * FIREEYE_INA228_VBUS_LSB_V;
  return 0;
}

int fireeye_ina228_read_measurement(float *current_a, float *bus_v)
{
  int ret;

  if (current_a == NULL || bus_v == NULL)
    {
      return -EINVAL;
    }

  ret = fireeye_ina228_read_current(current_a);
  if (ret < 0)
    {
      return ret;
    }

  return fireeye_ina228_read_bus_voltage(bus_v);
}
