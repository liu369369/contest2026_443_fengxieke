/****************************************************************************
 * oled_display.h - SSD1306 0.96" I2C OLED (128x64, addr 0x3C)
 ****************************************************************************/

#ifndef __FIREYEYE_OLED_DISPLAY_H
#define __FIREYEYE_OLED_DISPLAY_H

#include <stdint.h>

#define OLED_I2C_ADDR     0x3C
#define OLED_WIDTH        128
#define OLED_HEIGHT       64

int  oled_init(void);
void oled_clear(void);
void oled_fill(uint8_t on);
void oled_shownum(int row, int col, const char *text);
void oled_showstr(int page, const char *str);
void oled_refresh(void);

#endif
