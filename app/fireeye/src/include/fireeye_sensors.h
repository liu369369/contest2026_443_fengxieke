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

#endif /* __FIREYEYE_SENSORS_H */
