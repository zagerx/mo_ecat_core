/**
 * @file master_states.c
 * @brief 当前发现阶段的主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

static struct mo_ecat_master *master_from_sm(struct statemachine *sm)
{
	return sm ? (struct mo_ecat_master *)sm->data : NULL;
}

void master_state_init(struct statemachine *sm)
{
	struct mo_ecat_master *master;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);
	switch (sm->phase) {
	case ENTER:
		if (master) {
			sm_transition(sm, master_state_idle);
		}
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_idle(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;
	int result;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
		}
		sm->phase = USER_STATUS;
		break;
	case USER_STATUS:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_DISCOVER) {
			master_clear_cmd(master);
			break;
		}

		master_release_resources(master);
		result = backend_init(&master->backend);
		if (result == 0) {
			result = master_backend_open(master);
		}
		if (result == 0) {
			result = master_prepare_discovery(master);
		}

		master_clear_cmd(master);
		if (result < 0) {
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
		} else {
			sm_transition(sm, master_state_discovered);
		}
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_discovered(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
		}
		sm->phase = USER_STATUS;
		break;
	case USER_STATUS:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_release_resources(master);
			sm_transition(sm, master_state_idle);
		}
		master_clear_cmd(master);
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_fault(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
		}
		sm->phase = USER_STATUS;
		break;
	case USER_STATUS:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_release_resources(master);
			sm_transition(sm, master_state_idle);
		}
		master_clear_cmd(master);
		break;
	case EXIT:
	default:
		break;
	}
}
