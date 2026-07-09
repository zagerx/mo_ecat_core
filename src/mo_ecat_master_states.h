#ifndef MO_ECAT_MASTER_STATES_H
#define MO_ECAT_MASTER_STATES_H

#include "common/statemachine/statemachine.h"
#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void mo_ecat_master_state_init(struct statemachine *sm);
void mo_ecat_master_state_idle(struct statemachine *sm);
void mo_ecat_master_state_ready(struct statemachine *sm);
void mo_ecat_master_state_running(struct statemachine *sm);
void mo_ecat_master_state_degraded(struct statemachine *sm);
void mo_ecat_master_state_fault(struct statemachine *sm);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_STATES_H */
