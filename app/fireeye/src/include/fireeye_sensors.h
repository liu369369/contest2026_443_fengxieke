/****************************************************************************
 * FireEye - 传感器与执行器接口
 *
 * 板级硬件（ADC/GPIO）由 vendor 板级文件 gd32f4xx_fireeye_hw.c 提供，
 * 本模块负责物理量换算、按键消抖和报警输出逻辑。
 ****************************************************************************/

#ifndef __FIREYEYE_SENSORS_H
#define __FIREYEYE_SENSORS_H

#include <stdbool.h>

#include "fireeye_config.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 初始化传感器（ADC/GPIO）并做电流零点校准
 * @return 0 成功，负值为错误码
 */

int fireeye_sensors_init(void);

/**
 * @brief 传感器是否可用
 */

bool fireeye_sensors_ready(void);

/**
 * @brief 读取电流（A），失败返回 NAN
 */

float fireeye_sensors_read_current(void);

/**
 * @brief 读取温度（摄氏度），失败返回 NAN
 */

float fireeye_sensors_read_temperature(void);

/**
 * @brief 板载按键是否按下（已消抖，返回电平而非边沿）
 */

bool fireeye_sensors_key_pressed(void);

/**
 * @brief 按系统状态驱动蜂鸣器与继电器
 * @param state 当前系统状态
 */

void fireeye_sensors_alarm_output(system_state_t state);

/**
 * @brief 直接控制报警灯（供电自检用）
 * @param on true 点亮
 */

void fireeye_sensors_set_led(bool on);

/**
 * @brief 直接设置输出引脚电平（不做极性换算），用于极性排查
 * @param which 0=蜂鸣器 1=继电器 2=报警灯
 * @param high true 高电平
 */

void fireeye_sensors_pin_raw(int which, bool high);

/**
 * @brief 电流参考自检：分别打印继电器吸合/释放时的电流通道原始 ADC 码值
 *        负载未接时，两者的差值就是继电器线圈造成的采样偏移
 * @return 0 成功，负值为错误码
 */

int fireeye_sensors_offset_test(void);

#endif /* __FIREYEYE_SENSORS_H */
