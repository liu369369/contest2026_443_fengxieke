/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Filter Module Header
 ****************************************************************************/

#ifndef __FIREYEYE_FILTER_H
#define __FIREYEYE_FILTER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "fireeye_config.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize a filter context
 * @param ctx Pointer to filter context
 * @param window_size Moving average window size
 * @param alpha Low pass filter coefficient (0-1)
 * @return 0 on success, negative errno on failure
 */

int filter_init(filter_ctx_t *ctx, int window_size, float alpha);

/**
 * @brief Reset filter context to initial state
 * @param ctx Pointer to filter context
 */

void filter_reset(filter_ctx_t *ctx);

/**
 * @brief Process a single data point through moving average filter
 * @param ctx Pointer to filter context
 * @param input Input value
 * @return Filtered output value
 */

float filter_moving_average(filter_ctx_t *ctx, float input);

/**
 * @brief Process a single data point through low pass filter
 * @param ctx Pointer to filter context
 * @param input Input value
 * @return Filtered output value
 */

float filter_low_pass(filter_ctx_t *ctx, float input);

/**
 * @brief Process a single data point through combined filter
 *        (moving average + low pass)
 * @param ctx Pointer to filter context
 * @param input Input value
 * @return Filtered output value
 */

float filter_combined(filter_ctx_t *ctx, float input);

#endif /* __FIREYEYE_FILTER_H */
