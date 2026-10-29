/****************************************************************************
 * oled_display.h - SSD1306 0.96" I2C OLED (128x64, addr 0x3C)
 ****************************************************************************/

#ifndef __FIREYEYE_OLED_DISPLAY_H
#define __FIREYEYE_OLED_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifndef OLED_I2C_ADDR
#  define OLED_I2C_ADDR   0x3C
#endif
#ifndef OLED_WIDTH
#  define OLED_WIDTH      128
#endif
#ifndef OLED_HEIGHT
#  define OLED_HEIGHT     64
#endif

int  oled_init(void);
int  oled_clear(void);
int  oled_fill(uint8_t on);
void oled_shownum(int row, int col, const char *text);
void oled_showstr(int page, const char *str);
int  oled_refresh(void);
int  oled_show_boot(void);
int  oled_show_status(const char *state,
                      float current, bool current_valid,
                      float temperature, bool temperature_valid,
                      float bus_voltage, bool bus_valid,
                      bool load_enabled);
int  oled_test(void);

#endif
