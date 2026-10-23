/****************************************************************************
 * FireEye - INA228 current and bus-voltage monitor
 ****************************************************************************/

#ifndef __FIREYEYE_INA228_H
#define __FIREYEYE_INA228_H

#include <stdbool.h>
#include <stdint.h>

struct fireeye_ina228_info_s
{
  uint16_t manufacturer_id;
  uint16_t device_id;
  uint16_t config;
  uint16_t adc_config;
};

int fireeye_ina228_init(void);
bool fireeye_ina228_ready(void);
int fireeye_ina228_get_info(struct fireeye_ina228_info_s *info);
int fireeye_ina228_read_shunt_raw(int32_t *raw);
int fireeye_ina228_read_current(float *current_a);
int fireeye_ina228_read_bus_voltage(float *bus_v);
int fireeye_ina228_read_measurement(float *current_a, float *bus_v);

#endif /* __FIREYEYE_INA228_H */
