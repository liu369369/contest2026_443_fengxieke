/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Threshold Module Implementation
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <math.h>
#include <errno.h>
#include "include/fireeye_threshold.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief Calculate linear regression slope from history buffer
 * @param values Array of values
 * @param count Number of values
 * @return Slope value
 */

static float calculate_linear_slope(const float *values, int count)
{
  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  float sum_x2 = 0.0f;
  float slope;
  int i;

  if (count < 2)
    {
      return 0.0f;
    }

  for (i = 0; i < count; i++)
    {
      float x = (float)i;
      float y = values[i];

      sum_x += x;
      sum_y += y;
      sum_xy += x * y;
      sum_x2 += x * x;
    }

  /* Linear regression: slope = (n*sum_xy - sum_x*sum_y) / (n*sum_x2 - sum_x^2) */
  float n = (float)count;
  float denominator = n * sum_x2 - sum_x * sum_x;

  if (fabsf(denominator) < 1e-6f)
    {
      return 0.0f;
    }

  slope = (n * sum_xy - sum_x * sum_y) / denominator;

  return slope;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int threshold_init(threshold_config_t *config,
                   float warning_threshold,
                   float alarm_threshold,
                   int consecutive_count,
                   float slope_threshold)
{
  if (config == NULL)
    {
      return -EINVAL;
    }

  if (warning_threshold <= 0 || alarm_threshold <= warning_threshold)
    {
      return -EINVAL;
    }

  if (consecutive_count <= 0 || slope_threshold <= 0)
    {
      return -EINVAL;
    }

  config->warning_threshold = warning_threshold;
  config->alarm_threshold = alarm_threshold;
  config->consecutive_count = consecutive_count;
  config->slope_threshold = slope_threshold;

  return 0;
}

void threshold_reset(threshold_state_t *state)
{
  if (state == NULL)
    {
      return;
    }

  state->over_count = 0;
  state->history_index = 0;
  memset(state->history, 0, sizeof(state->history));
}

int threshold_check(const threshold_config_t *config,
                    threshold_state_t *state,
                    float value)
{
  bool is_warning;
  bool is_alarm;
  float slope;
  bool is_rising_fast;
  int result;

  if (config == NULL || state == NULL)
    {
      return 0;
    }

  /* Update history buffer */
  state->history[state->history_index % TREND_HISTORY_SIZE] = value;
  state->history_index++;

  /* Single threshold check */
  is_alarm = (value >= config->alarm_threshold);
  is_warning = (value >= config->warning_threshold);

  /* Trend analysis - calculate slope */
  slope = threshold_calculate_slope(state);
  is_rising_fast = (fabsf(slope) > config->slope_threshold);

  /* Combined check: alarm OR (warning AND rising fast) */
  if (is_alarm || (is_warning && is_rising_fast))
    {
      state->over_count++;
    }
  else
    {
      /* Decrease count (with minimum of 0) */
      if (state->over_count > 0)
        {
          state->over_count--;
        }
    }

  /* Debounce: need consecutive count before triggering */
  if (state->over_count >= config->consecutive_count)
    {
      if (is_alarm)
        {
          result = 2;  /* Alarm */
        }
      else if (is_warning)
        {
          result = 1;  /* Warning */
        }
      else
        {
          result = 0;  /* Normal */
        }
    }
  else
    {
      result = 0;  /* Normal (not enough consecutive triggers) */
    }

  return result;
}

float threshold_calculate_slope(const threshold_state_t *state)
{
  float values[TREND_HISTORY_SIZE];
  int count;
  int i;

  if (state == NULL)
    {
      return 0.0f;
    }

  /* Determine how many values we have */
  count = (state->history_index < TREND_HISTORY_SIZE) ?
          state->history_index : TREND_HISTORY_SIZE;

  if (count < 2)
    {
      return 0.0f;
    }

  /* Copy history to temporary buffer in order */
  for (i = 0; i < count; i++)
    {
      int idx = (state->history_index - count + i) % TREND_HISTORY_SIZE;
      values[i] = state->history[idx];
    }

  /* Calculate slope and convert to per-second rate */
  float slope = calculate_linear_slope(values, count);
  return slope * FIREYEYE_SAMPLE_RATE_HZ;
}

int threshold_get_over_count(const threshold_state_t *state)
{
  if (state == NULL)
    {
      return 0;
    }

  return state->over_count;
}
