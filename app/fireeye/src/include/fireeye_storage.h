/****************************************************************************
 * FireEye - Local event history in on-chip FLASH
 ****************************************************************************/

#ifndef __FIREEYE_STORAGE_H
#define __FIREEYE_STORAGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#if defined(CONFIG_FIREYEYE_STORAGE) && defined(CONFIG_ARCH_HAVE_PROGMEM)

/**
 * @brief 扫描片上 Flash 里的历史记录区，恢复计数与写入位置
 * @return 恢复到的历史记录条数，失败返回负的错误码

 */

int fireeye_storage_init(void);

/**
 * @brief 追加一条记录（状态变化或心跳时调用）
 * @param state     状态机状态
 * @param alarm     是否处于报警态
 * @param current_a 滤波后的电流
 * @param temp_c    滤波后的温度
 * @param uptime_s  系统运行秒数
 * @return 0 成功，负值为错误码

 */

int fireeye_storage_append(uint8_t state, bool alarm, float current_a,
                           float temp_c, uint32_t uptime_s);

/**
 * @brief 读取当前已保存的记录条数

 */

int fireeye_storage_count(void);

/**
 * @brief 把最近 max 条记录打印到控制台（供上电回放与 `fireeye -e` 使用）
 * @return 实际打印的条数，负值为错误码

 */

int fireeye_storage_show(int max);

#else /* 未启用存储或该芯片没有 progmem 支持 */

static inline int fireeye_storage_init(void)
{
  return -ENOSYS;
}

static inline int fireeye_storage_append(uint8_t state, bool alarm,
                                         float current_a, float temp_c,
                                         uint32_t uptime_s)
{
  (void)state;
  (void)alarm;
  (void)current_a;
  (void)temp_c;
  (void)uptime_s;
  return -ENOSYS;
}

static inline int fireeye_storage_count(void)
{
  return 0;
}

static inline int fireeye_storage_show(int max)
{
  (void)max;
  return -ENOSYS;
}

#endif

#endif /* __FIREEYE_STORAGE_H */
