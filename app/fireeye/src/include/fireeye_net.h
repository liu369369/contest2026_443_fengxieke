/****************************************************************************
 * FireEye - 联网上报接口
 ****************************************************************************/

#ifndef __FIREYEYE_NET_H
#define __FIREYEYE_NET_H

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief 配置静态 IP 并启动上报任务（需 CONFIG_FIREYEYE_NET）
 * @return 0 成功，负值为错误码
 */

int fireeye_net_init(void);

/**
 * @brief 更新待上报的最新数据（主循环每次采样后调用）
 * @param current_a 滤波后的电流
 * @param temp_c    滤波后的温度
 * @param state     状态机状态
 * @param alarm     是否处于报警
 * @param uptime_s  运行时间
 */

void fireeye_net_update(float current_a, float temp_c, int state,
                        bool alarm, uint32_t uptime_s);

#endif /* __FIREYEYE_NET_H */
