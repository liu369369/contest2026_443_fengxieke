/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * Filter Module Implementation
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <math.h>
#include <errno.h>
#include "include/fireeye_filter.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int filter_init(filter_ctx_t *ctx, int window_size, float alpha)
{
  if (ctx == NULL || window_size <= 0 || alpha <= 0.0f || alpha > 1.0f)
    {
      return -EINVAL;
    }

  memset(ctx, 0, sizeof(filter_ctx_t));
  ctx->alpha = alpha;
  ctx->last_output = 0.0f;
  ctx->initialized = true;

  return 0;
}

void filter_reset(filter_ctx_t *ctx)
{
  if (ctx == NULL)
    {
      return;
    }

  ctx->index = 0;
  ctx->sum = 0.0f;
  ctx->last_output = 0.0f;
  memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

float filter_moving_average(filter_ctx_t *ctx, float input)
{
  if (ctx == NULL || !ctx->initialized)
    {
      return input;
    }

  /* If buffer is full, subtract the oldest value */
  if (ctx->index >= FILTER_MOVING_AVG_WINDOW)
    {
      ctx->sum -= ctx->buffer[ctx->index % FILTER_MOVING_AVG_WINDOW];
    }

  /* Add new value to buffer */
  ctx->buffer[ctx->index % FILTER_MOVING_AVG_WINDOW] = input;
  ctx->sum += input;
  ctx->index++;

  /* Calculate average */
  int count = (ctx->index < FILTER_MOVING_AVG_WINDOW) ?
              ctx->index : FILTER_MOVING_AVG_WINDOW;

  return ctx->sum / (float)count;
}

float filter_low_pass(filter_ctx_t *ctx, float input)
{
  if (ctx == NULL || !ctx->initialized)
    {
      return input;
    }

  /* First order low pass filter: y[n] = alpha * x[n] + (1 - alpha) * y[n-1] */
  ctx->last_output = ctx->alpha * input + (1.0f - ctx->alpha) * ctx->last_output;

  return ctx->last_output;
}

float filter_combined(filter_ctx_t *ctx, float input)
{
  float avg_value;

  if (ctx == NULL || !ctx->initialized)
    {
      return input;
    }

  /* Step 1: Moving average to remove noise */
  avg_value = filter_moving_average(ctx, input);

  /* Step 2: Low pass filter to smooth the result */
  return filter_low_pass(ctx, avg_value);
}
