/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Threshold Module Header
 ****************************************************************************/

#ifndef __FIREYEYE_THRESHOLD_H
#define __FIREYEYE_THRESHOLD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "fireeye_config.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize threshold configuration
 * @param config Pointer to threshold configuration
 * @param warning_threshold Warning threshold value
 * @param alarm_threshold Alarm threshold value
 * @param consecutive_count Consecutive count for debounce
 * @param slope_threshold Slope threshold for trend analysis
 * @return 0 on success, negative errno on failure
 */

int threshold_init(threshold_config_t *config,
                   float warning_threshold,
                   float alarm_threshold,
                   int consecutive_count,
                   float slope_threshold);

/**
 * @brief Reset threshold state to initial
 * @param state Pointer to threshold state
 */

void threshold_reset(threshold_state_t *state);

/**
 * @brief Check value against thresholds with debounce and trend analysis
 * @param config Pointer to threshold configuration
 * @param state Pointer to threshold state (maintains history)
 * @param value Value to check
 * @return 0 = normal, 1 = warning, 2 = alarm
 */

int threshold_check(const threshold_config_t *config,
                    threshold_state_t *state,
                    float value);

/**
 * @brief Calculate slope from history (trend analysis)
 * @param state Pointer to threshold state
 * @return Slope value (change per second)
 */

float threshold_calculate_slope(const threshold_state_t *state);

/**
 * @brief Get current over-limit count
 * @param state Pointer to threshold state
 * @return Current over-limit count
 */

int threshold_get_over_count(const threshold_state_t *state);

#endif /* __FIREYEYE_THRESHOLD_H */
