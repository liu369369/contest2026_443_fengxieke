/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * State Machine Module Implementation
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <errno.h>
#include "include/fireeye_fsm.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* State transition table */
static const system_state_t g_state_transitions[4][5] =
{
  /* Current State: NORMAL */
  {
    STATE_NORMAL,    /* Event: NORMAL */
    STATE_WARNING,   /* Event: WARNING */
    STATE_ALARM,     /* Event: ALARM */
    STATE_SHUTDOWN,  /* Event: SHUTDOWN */
    STATE_NORMAL     /* Event: RESET (no effect) */
  },
  /* Current State: WARNING */
  {
    STATE_NORMAL,    /* Event: NORMAL */
    STATE_WARNING,   /* Event: WARNING (stay) */
    STATE_ALARM,     /* Event: ALARM */
    STATE_SHUTDOWN,  /* Event: SHUTDOWN */
    STATE_NORMAL     /* Event: RESET */
  },
  /* Current State: ALARM */
  {
    STATE_NORMAL,    /* Event: NORMAL */
    STATE_ALARM,     /* Event: WARNING (stay in alarm) */
    STATE_ALARM,     /* Event: ALARM (stay) */
    STATE_SHUTDOWN,  /* Event: SHUTDOWN */
    STATE_NORMAL     /* Event: RESET */
  },
  /* Current State: SHUTDOWN */
  {
    STATE_SHUTDOWN,  /* Event: NORMAL (stay in shutdown) */
    STATE_SHUTDOWN,  /* Event: WARNING (stay in shutdown) */
    STATE_SHUTDOWN,  /* Event: ALARM (stay in shutdown) */
    STATE_SHUTDOWN,  /* Event: SHUTDOWN (stay) */
    STATE_NORMAL     /* Event: RESET */
  }
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int fsm_init(fsm_ctx_t *ctx)
{
  if (ctx == NULL)
    {
      return -EINVAL;
    }

  memset(ctx, 0, sizeof(fsm_ctx_t));
  ctx->current_state = STATE_NORMAL;
  ctx->state_duration = 0;
  ctx->last_state_change = 0;

  return 0;
}

void fsm_reset(fsm_ctx_t *ctx)
{
  if (ctx == NULL)
    {
      return;
    }

  ctx->current_state = STATE_NORMAL;
  ctx->state_duration = 0;
  ctx->last_state_change = 0;
}

system_state_t fsm_process_event(fsm_ctx_t *ctx, system_event_t event)
{
  system_state_t old_state;
  system_state_t new_state;

  if (ctx == NULL)
    {
      return STATE_NORMAL;
    }

  if (event < 0 || event > EVENT_RESET)
    {
      return ctx->current_state;
    }

  old_state = ctx->current_state;
  new_state = g_state_transitions[old_state][event];

  /* Update state if changed */
  if (new_state != old_state)
    {
      ctx->current_state = new_state;
      ctx->state_duration = 0;
      ctx->last_state_change = 0;  /* Will be set by caller if needed */
    }
  else
    {
      ctx->state_duration++;
    }

  return ctx->current_state;
}

system_state_t fsm_get_state(const fsm_ctx_t *ctx)
{
  if (ctx == NULL)
    {
      return STATE_NORMAL;
    }

  return ctx->current_state;
}

int fsm_get_state_duration(const fsm_ctx_t *ctx)
{
  if (ctx == NULL)
    {
      return 0;
    }

  return ctx->state_duration;
}

const char *fsm_state_to_string(system_state_t state)
{
  switch (state)
    {
      case STATE_NORMAL:
        return "NORMAL";
      case STATE_WARNING:
        return "WARNING";
      case STATE_ALARM:
        return "ALARM";
      case STATE_SHUTDOWN:
        return "SHUTDOWN";
      default:
        return "UNKNOWN";
    }
}

const char *fsm_event_to_string(system_event_t event)
{
  switch (event)
    {
      case EVENT_NORMAL:
        return "NORMAL";
      case EVENT_WARNING:
        return "WARNING";
      case EVENT_ALARM:
        return "ALARM";
      case EVENT_SHUTDOWN:
        return "SHUTDOWN";
      case EVENT_RESET:
        return "RESET";
      default:
        return "UNKNOWN";
    }
}

system_event_t fsm_status_to_event(int status)
{
  switch (status)
    {
      case 0:
        return EVENT_NORMAL;
      case 1:
        return EVENT_WARNING;
      case 2:
        return EVENT_ALARM;
      default:
        return EVENT_NORMAL;
    }
}
