#ifndef MASTER_STATES_H
#define MASTER_STATES_H

#include "common/statemachine/statemachine.h"
#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void master_state_init(struct statemachine *sm);
void master_state_idle(struct statemachine *sm);
void master_state_discovered(struct statemachine *sm);
void master_state_fault(struct statemachine *sm);

#ifdef __cplusplus
}
#endif

#endif /* MASTER_STATES_H */
