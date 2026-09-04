/****************************************************************************
 * FireEye - Electric Bicycle Charging Safety Monitor
 * State Machine Module Header
 ****************************************************************************/

#ifndef __FIREYEYE_FSM_H
#define __FIREYEYE_FSM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "fireeye_config.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize state machine
 * @param ctx Pointer to state machine context
 * @return 0 on success, negative errno on failure
 */

int fsm_init(fsm_ctx_t *ctx);

/**
 * @brief Reset state machine to initial state
 * @param ctx Pointer to state machine context
 */

void fsm_reset(fsm_ctx_t *ctx);

/**
 * @brief Process event and transition to new state
 * @param ctx Pointer to state machine context
 * @param event Event to process
 * @return New system state after transition
 */

system_state_t fsm_process_event(fsm_ctx_t *ctx, system_event_t event);

/**
 * @brief Get current state
 * @param ctx Pointer to state machine context
 * @return Current system state
 */

system_state_t fsm_get_state(const fsm_ctx_t *ctx);

/**
 * @brief Get duration in current state
 * @param ctx Pointer to state machine context
 * @return Duration in current state (in sample periods)
 */

int fsm_get_state_duration(const fsm_ctx_t *ctx);

/**
 * @brief Convert state to string
 * @param state System state
 * @return String representation of state
 */

const char *fsm_state_to_string(system_state_t state);

/**
 * @brief Convert event to string
 * @param event System event
 * @return String representation of event
 */

const char *fsm_event_to_string(system_event_t event);

/**
 * @brief Convert threshold status to event
 * @param status Threshold status (0=normal, 1=warning, 2=alarm)
 * @return Corresponding system event
 */

system_event_t fsm_status_to_event(int status);

#endif /* __FIREYEYE_FSM_H */
