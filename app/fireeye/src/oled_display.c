/****************************************************************************
 * oled_display.c - SSD1306 0.96" I2C OLED (128x64, addr 0x3C) for FireEye
 *
 * Opens /dev/i2c1 for device readiness and sends transfers through the board
 * PB10/PB11 software-I2C helper. I2C0 shares PB6/PB7 with USART0.
 ****************************************************************************/

#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arch/board/board.h>
#include "include/fireeye_config.h"
#include "include/oled_display.h"

#define I2C_DEV_PATH      "/dev/i2c1"

static int g_i2c_fd = -1;
static uint8_t g_i2c_addr = OLED_I2C_ADDR;   /* 默认 0x3C，失败会重试 0x3D */
static uint8_t g_fb[OLED_WIDTH * OLED_HEIGHT / 8];

/* SSD1306 command prefix */
#define CMD_PREFIX        0x00
#define DATA_PREFIX       0x40

static int oled_write(uint8_t *buf, uint16_t len)
{
  if (g_i2c_fd < 0)
    {
      return -ENODEV;
    }

  return gd32_fireeye_i2c_write(g_i2c_addr, buf, len);
}

static int oled_cmd(uint8_t c)
{
  uint8_t b[2];

  b[0] = CMD_PREFIX;
  b[1] = c;
  return oled_write(b, 2);
}

/* 发送一串命令：每次最多打包 15 条（缓冲区 1+15），循环发完为止。
 * 注意：旧实现只发一次且按 n+1 长度发送，会越界并丢掉末尾的电荷泵命令。 */

static int oled_cmds(const uint8_t *cmds, size_t n)
{
  uint8_t b[1 + 15];
  size_t sent = 0;

  while (sent < n)
    {
      size_t chunk = n - sent;
      size_t i;
      int ret;

      if (chunk > 15)
        {
          chunk = 15;
        }

      b[0] = CMD_PREFIX;
      for (i = 0; i < chunk; i++)
        {
          b[1 + i] = cmds[sent + i];
        }

      ret = oled_write(b, (uint16_t)(chunk + 1));
      if (ret < 0)
        {
          return ret;
        }

      sent += chunk;
    }

  return 0;
}

int oled_init(void)
{
  static const uint8_t init_cmds[] = {
    0xAE,             /* display off */
    0x20, 0x00,       /* memory mode: horizontal */
    0xB0,             /* page0 */
    0xC8,             /* COM scan dir */
    0x00, 0x10,       /* low/high column */
    0x40,             /* start line 0 */
    0x81, 0x7F,       /* contrast */
    0xA1,             /* segment remap */
    0xA6,             /* normal display */
    0xA8, 0x3F,       /* multiplex 64 */
    0xA4,             /* resume from RAM */
    0xD3, 0x00,       /* display offset */
    0xD5, 0xF0,       /* clock div */
    0xD9, 0x22,       /* pre-charge */
    0xDA, 0x12,       /* com pins */
    0xDB, 0x20,       /* vcomh */
    0x8D, 0x14,       /* charge pump */
  };
  int ret;

  if (g_i2c_fd >= 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
    }

  g_i2c_fd = open(I2C_DEV_PATH, O_RDWR);
  if (g_i2c_fd < 0)
    {
      return -ENODEV;
    }

  /* SSD1306 上电后需要稳定时间 */

  usleep(50000);

  /* 先按 0x3C 初始化，失败再试 0x3D（两种模块都很常见） */

  g_i2c_addr = OLED_I2C_ADDR;
  ret = oled_cmds(init_cmds, sizeof(init_cmds));
  if (ret < 0)
    {
      g_i2c_addr = 0x3D;
      ret = oled_cmds(init_cmds, sizeof(init_cmds));
    }

  if (ret < 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
      return -EIO;
    }

  memset(g_fb, 0, sizeof(g_fb));
  ret = oled_refresh();
  if (ret < 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
      return ret;
    }

  ret = oled_cmd(0xAF);       /* display on */
  if (ret < 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
      return ret;
    }

  return 0;
}

int oled_clear(void)
{
  memset(g_fb, 0, sizeof(g_fb));
  return oled_refresh();
}

int oled_fill(uint8_t on)
{
  memset(g_fb, on ? 0xFF : 0x00, sizeof(g_fb));
  return oled_refresh();
}

int oled_refresh(void)
{
  static const uint8_t window_cmds[] = {
    0x21, 0x00, 0x7F,   /* 列地址 0..127 */
    0x22, 0x00, 0x07,   /* 页地址 0..7   */
  };
  uint8_t page_block[1 + 1 + OLED_WIDTH];
  int page, col;
  int ret;

  if (g_i2c_fd < 0)
    {
      return -ENODEV;
    }

  ret = oled_cmds(window_cmds, sizeof(window_cmds));
  if (ret < 0)
    {
      return ret;
    }

  for (page = 0; page < 8; page++)
    {
      page_block[0] = DATA_PREFIX;
      for (col = 0; col < OLED_WIDTH; col++)
        {
          page_block[1 + col] = g_fb[page * OLED_WIDTH + col];
        }
      ret = oled_write(page_block, 1 + OLED_WIDTH);
      if (ret < 0)
        {
          return ret;
        }
    }

  return 0;
}

/* 6x8 ASCII font, codepoints 0x20..0x7F */
static const uint8_t font6x8[96][6] = {
  {0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x20 space */
  {0x00,0x00,0x5f,0x00,0x00,0x00},
  {0x00,0x07,0x00,0x07,0x00,0x00},
  {0x14,0x7f,0x14,0x7f,0x14,0x00},
  {0x24,0x2a,0x7f,0x2a,0x12,0x00},
  {0x23,0x13,0x08,0x64,0x62,0x00},
  {0x36,0x49,0x55,0x22,0x50,0x00},
  {0x00,0x05,0x03,0x00,0x00,0x00},
  {0x00,0x1c,0x22,0x41,0x00,0x00},
  {0x00,0x41,0x22,0x1c,0x00,0x00},
  {0x14,0x08,0x3e,0x08,0x14,0x00},
  {0x08,0x08,0x3e,0x08,0x08,0x00},
  {0x00,0x50,0x30,0x00,0x00,0x00},
  {0x08,0x08,0x08,0x08,0x08,0x00},
  {0x00,0x60,0x60,0x00,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02,0x00},
  {0x3e,0x51,0x49,0x45,0x3e,0x00}, /* 0 */
  {0x00,0x42,0x7f,0x40,0x00,0x00},
  {0x42,0x61,0x51,0x49,0x46,0x00},
  {0x21,0x41,0x45,0x4b,0x31,0x00},
  {0x18,0x14,0x12,0x7f,0x10,0x00},
  {0x27,0x45,0x45,0x45,0x39,0x00},
  {0x3c,0x4a,0x49,0x49,0x30,0x00},
  {0x01,0x71,0x09,0x05,0x03,0x00},
  {0x36,0x49,0x49,0x49,0x36,0x00},
  {0x06,0x49,0x49,0x29,0x1e,0x00},
  {0x00,0x36,0x36,0x00,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00,0x00},
  {0x14,0x14,0x14,0x14,0x14,0x00},
  {0x00,0x41,0x22,0x14,0x08,0x00},
  {0x02,0x01,0x51,0x09,0x06,0x00},
  {0x32,0x49,0x79,0x41,0x3e,0x00},
  {0x7e,0x11,0x11,0x11,0x7e,0x00},
  {0x7f,0x49,0x49,0x49,0x36,0x00},
  {0x3e,0x41,0x41,0x41,0x22,0x00},
  {0x7f,0x41,0x41,0x22,0x1c,0x00},
  {0x7f,0x49,0x49,0x49,0x41,0x00},
  {0x7f,0x09,0x09,0x09,0x01,0x00},
  {0x3e,0x41,0x49,0x49,0x7a,0x00},
  {0x7f,0x08,0x08,0x08,0x7f,0x00},
  {0x00,0x41,0x7f,0x41,0x00,0x00},
  {0x20,0x40,0x41,0x3f,0x01,0x00},
  {0x7f,0x08,0x14,0x22,0x41,0x00},
  {0x7f,0x40,0x40,0x40,0x40,0x00},
  {0x7f,0x02,0x0c,0x02,0x7f,0x00},
  {0x7f,0x04,0x08,0x10,0x7f,0x00},
  {0x3e,0x41,0x41,0x41,0x3e,0x00},
  {0x7f,0x09,0x09,0x09,0x06,0x00},
  {0x3e,0x41,0x51,0x21,0x5e,0x00},
  {0x7f,0x09,0x19,0x29,0x46,0x00},
  {0x46,0x49,0x49,0x49,0x31,0x00},
  {0x01,0x01,0x7f,0x01,0x01,0x00},
  {0x3f,0x40,0x40,0x40,0x3f,0x00},
  {0x1f,0x20,0x40,0x20,0x1f,0x00},
  {0x3f,0x40,0x38,0x40,0x3f,0x00},
  {0x63,0x14,0x08,0x14,0x63,0x00},
  {0x07,0x08,0x70,0x08,0x07,0x00},
  {0x61,0x51,0x49,0x45,0x43,0x00},
  {0x00,0x7f,0x41,0x41,0x00,0x00},
  {0x02,0x04,0x08,0x10,0x20,0x00},
  {0x00,0x41,0x41,0x7f,0x00,0x00},
  {0x04,0x02,0x01,0x02,0x04,0x00},
  {0x40,0x40,0x40,0x40,0x40,0x00},
  {0x00,0x01,0x02,0x04,0x00,0x00},
  {0x20,0x54,0x54,0x54,0x78,0x00},
  {0x7f,0x48,0x44,0x44,0x38,0x00},
  {0x38,0x44,0x44,0x44,0x20,0x00},
  {0x38,0x44,0x44,0x48,0x7f,0x00},
  {0x38,0x54,0x54,0x54,0x18,0x00},
  {0x08,0x7e,0x09,0x01,0x02,0x00},
  {0x0c,0x52,0x52,0x52,0x3e,0x00},
  {0x7f,0x08,0x04,0x04,0x78,0x00},
  {0x00,0x44,0x7d,0x40,0x00,0x00},
  {0x20,0x40,0x44,0x3d,0x00,0x00},
  {0x7f,0x10,0x28,0x44,0x00,0x00},
  {0x00,0x41,0x7f,0x40,0x00,0x00},
  {0x7c,0x04,0x18,0x04,0x78,0x00},
  {0x7c,0x08,0x04,0x04,0x78,0x00},
  {0x38,0x44,0x44,0x44,0x38,0x00},
  {0x7c,0x14,0x14,0x14,0x08,0x00},
  {0x08,0x14,0x14,0x18,0x7c,0x00},
  {0x7c,0x08,0x04,0x04,0x08,0x00},
  {0x48,0x54,0x54,0x54,0x20,0x00},
  {0x04,0x3f,0x44,0x40,0x20,0x00},
  {0x3c,0x40,0x40,0x20,0x7c,0x00},
  {0x1c,0x20,0x40,0x20,0x1c,0x00},
  {0x3c,0x40,0x30,0x40,0x3c,0x00},
  {0x44,0x28,0x10,0x28,0x44,0x00},
  {0x0c,0x50,0x50,0x50,0x3c,0x00},
  {0x44,0x64,0x54,0x4c,0x44,0x00},
  {0x00,0x08,0x36,0x41,0x00,0x00},
  {0x00,0x00,0x7f,0x00,0x00,0x00},
  {0x00,0x41,0x36,0x08,0x00,0x00},
  {0x08,0x08,0x2a,0x1c,0x08,0x00},
  {0x08,0x1c,0x2a,0x08,0x08,0x00},
};

static void oled_clear_buffer(void)
{
  memset(g_fb, 0, sizeof(g_fb));
}

static void oled_put_pixel(int x, int y, bool on)
{
  uint8_t mask;

  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    {
      return;
    }

  mask = (uint8_t)(1u << (y & 7));
  if (on)
    {
      g_fb[(y >> 3) * OLED_WIDTH + x] |= mask;
    }
  else
    {
      g_fb[(y >> 3) * OLED_WIDTH + x] &= (uint8_t)~mask;
    }
}

static void oled_fill_rect(int x, int y, int width, int height, bool on)
{
  int px;
  int py;

  for (py = y; py < y + height; py++)
    {
      for (px = x; px < x + width; px++)
        {
          oled_put_pixel(px, py, on);
        }
    }
}

static void oled_hline(int x, int y, int width, bool on)
{
  oled_fill_rect(x, y, width, 1, on);
}

static int oled_text_width(const char *text, int scale)
{
  return text ? (int)strlen(text) * 6 * scale : 0;
}

static void oled_draw_text(int x, int y, const char *text, int scale, bool on)
{
  const unsigned char *s = (const unsigned char *)text;
  int cursor = x;

  if (scale < 1)
    {
      scale = 1;
    }

  while (s != NULL && *s != '\0')
    {
      int ch = (int)*s - 0x20;
      int col;

      if (ch < 0 || ch > 95)
        {
          ch = 0;
        }

      for (col = 0; col < 6; col++)
        {
          uint8_t bits = font6x8[ch][col];
          int row;

          for (row = 0; row < 8; row++)
            {
              if ((bits & (1u << row)) != 0)
                {
                  oled_fill_rect(cursor + col * scale, y + row * scale,
                                 scale, scale, on);
                }
            }
        }

      cursor += 6 * scale;
      s++;
    }
}

static void oled_draw_text_right(int right, int y, const char *text,
                                 int scale, bool on)
{
  oled_draw_text(right - oled_text_width(text, scale), y, text, scale, on);
}

void oled_showstr(int page, const char *str)
{
  if (page < 0 || page >= (OLED_HEIGHT / 8))
    {
      return;
    }

  memset(&g_fb[page * OLED_WIDTH], 0, OLED_WIDTH);
  oled_draw_text(0, page * 8, str, 1, true);
  (void)oled_refresh();
}

int oled_show_boot(void)
{
  if (g_i2c_fd < 0)
    {
      return -ENODEV;
    }

  oled_clear_buffer();
  oled_hline(0, 0, OLED_WIDTH, true);
  oled_hline(0, OLED_HEIGHT - 1, OLED_WIDTH, true);
  oled_fill_rect(1, 1, OLED_WIDTH - 2, 8, true);
  oled_draw_text(3, 2, "FIRE EYE", 1, false);
  oled_draw_text_right(OLED_WIDTH - 3, 2, "BOOT", 1, false);
  oled_draw_text(22, 19, "SAFETY MONITOR", 1, true);
  oled_draw_text(25, 33, "SSD1306 128X64", 1, true);
  oled_draw_text(34, 48, "STARTING...", 1, true);
  return oled_refresh();
}

int oled_show_status(const char *state,
                     float current, bool current_valid,
                     float temperature, bool temperature_valid,
                     float bus_voltage, bool bus_valid,
                     bool load_enabled)
{
  char current_text[12];
  char voltage_text[16];
  char temperature_text[16];
  char limit_text[24];

  if (g_i2c_fd < 0)
    {
      return -ENODEV;
    }

  if (state == NULL)
    {
      state = "UNKNOWN";
    }

  oled_clear_buffer();

  oled_hline(0, 0, OLED_WIDTH, true);
  oled_hline(0, OLED_HEIGHT - 1, OLED_WIDTH, true);
  oled_fill_rect(1, 1, OLED_WIDTH - 2, 8, true);
  oled_draw_text(3, 2, "FIRE EYE", 1, false);
  oled_draw_text_right(OLED_WIDTH - 3, 2, state, 1, false);

  oled_draw_text(3, 12, load_enabled ? "LOAD ON" : "LOAD OFF", 1, true);

  if (current_valid)
    {
      snprintf(current_text, sizeof(current_text), "%.3fA",
               (double)current);
    }
  else
    {
      snprintf(current_text, sizeof(current_text), "--.--A");
    }

  oled_draw_text(3, 22, "I", 2, true);
  oled_draw_text(18, 20, current_text, 2, true);
  oled_hline(0, 39, OLED_WIDTH, true);

  if (bus_valid)
    {
      snprintf(voltage_text, sizeof(voltage_text), "VBUS %.2fV",
               (double)bus_voltage);
    }
  else
    {
      snprintf(voltage_text, sizeof(voltage_text), "VBUS --.--V");
    }

  if (temperature_valid)
    {
      snprintf(temperature_text, sizeof(temperature_text), "T %.1fC",
               (double)temperature);
    }
  else
    {
      snprintf(temperature_text, sizeof(temperature_text), "T --.-C");
    }

  oled_draw_text(3, 42, voltage_text, 1, true);
  oled_draw_text_right(OLED_WIDTH - 3, 42, temperature_text, 1, true);
  oled_hline(0, 51, OLED_WIDTH, true);

  snprintf(limit_text, sizeof(limit_text), "TRIP %.1fA  %.0fC",
           (double)CURRENT_ALARM_THRESHOLD, (double)TEMP_ALARM_THRESHOLD);
  oled_draw_text(3, 54, limit_text, 1, true);

  return oled_refresh();
}

int oled_test(void)
{
  int ret;

  ret = oled_init();
  if (ret < 0)
    {
      return ret;
    }

  ret = oled_fill(1);
  if (ret < 0)
    {
      return ret;
    }

  usleep(300000);
  ret = oled_clear();
  if (ret < 0)
    {
      return ret;
    }

  oled_hline(0, 0, OLED_WIDTH, true);
  oled_hline(0, OLED_HEIGHT - 1, OLED_WIDTH, true);
  oled_fill_rect(1, 1, OLED_WIDTH - 2, 8, true);
  oled_draw_text(3, 2, "FIRE EYE", 1, false);
  oled_draw_text_right(OLED_WIDTH - 3, 2, "TEST", 1, false);
  oled_draw_text(22, 20, "OLED OK", 2, true);
  oled_hline(0, 40, OLED_WIDTH, true);
  oled_draw_text(19, 48, "I2C 0X3C 128X64", 1, true);
  return oled_refresh();
}

void oled_shownum(int row, int col, const char *text)
{
  oled_draw_text(col * 6, (row & 7) * 8, text, 1, true);
  (void)oled_refresh();
}
