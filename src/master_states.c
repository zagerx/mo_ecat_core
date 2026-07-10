/**
 * @file master_states.c
 * @brief 主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

#define MO_ECAT_MASTER_FAULT_THRESHOLD 3U

static struct mo_ecat_master *master_from_sm(struct statemachine *sm)
{
	return sm ? (struct mo_ecat_master *)sm->data : NULL;
}

static int master_command_is(struct mo_ecat_master *master,
			     enum mo_ecat_master_command command)
{
	return master && master->command == command;
}

static void master_reject_command(struct mo_ecat_master *master)
{
	if (master && master->command != MO_ECAT_MASTER_CMD_NONE) {
		master_clear_command(master);
	}
}

void master_state_init(struct statemachine *sm)
{
	enum {
		CHECK_INITIALIZED = USER_STATUS,
	};
	struct mo_ecat_master *master;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->cycle.result_pending = 0;
			master->cycle.abnormal = 0;
		}
		sm->count = 0;
		sm->phase = CHECK_INITIALIZED;
		break;
	case CHECK_INITIALIZED:
		sm->count++;
		/*
		 * 主站对象创建完成后，INIT 自动进入 IDLE。
		 * 这里和 MoDriver 的 Not Ready 状态一样：状态函数自己检查条件，
		 * 条件满足后调用 sm_transition()，真正 EXIT/ENTER 由下一次 dispatch 完成。
		 */
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
	enum {
		WAIT_COMMAND = USER_STATUS,
		EXEC_DISCOVER,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;
	int rc;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
			master->cycle.consecutive_errors = 0;
		}
		sm->count = 0;
		sm->phase = WAIT_COMMAND;
		break;
	case WAIT_COMMAND:
		sm->count++;
		if (!master || master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_DISCOVER)) {
			sm->phase = EXEC_DISCOVER;
		} else if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			master_clear_command(master);
		} else {
			master_reject_command(master);
		}
		break;
	case EXEC_DISCOVER:
		master_release_resources(master);
		rc = backend_init(&master->backend);
		if (rc == 0) {
			rc = master_backend_open(master);
		}
		if (rc == 0) {
			rc = master_prepare_discovery(master);
		}
		if (rc < 0) {
			master_release_resources(master);
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_clear_command(master);
			sm_transition(sm, master_state_discovered);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_discovered(struct statemachine *sm)
{
	enum {
		WAIT_COMMAND = USER_STATUS,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
		}
		sm->count = 0;
		sm->phase = WAIT_COMMAND;
		break;
	case WAIT_COMMAND:
		sm->count++;
		if (!master || master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			master_release_resources(master);
			master_clear_command(master);
			sm_transition(sm, master_state_idle);
			sm->phase = WAIT_TRANSITION;
		} else {
			master_reject_command(master);
		}
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_ready(struct statemachine *sm)
{
	enum {
		WAIT_COMMAND = USER_STATUS,
		EXEC_ACTIVATE,
		EXEC_RESET,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;
	int rc;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
			master->cycle.consecutive_errors = 0;
		}
		sm->count = 0;
		sm->phase = WAIT_COMMAND;
		break;
	case WAIT_COMMAND:
		sm->count++;
		if (!master || master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_ACTIVATE)) {
			sm->phase = EXEC_ACTIVATE;
		} else if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			sm->phase = EXEC_RESET;
		} else {
			master_reject_command(master);
		}
		break;
	case EXEC_ACTIVATE:
		rc = master_backend_activate(master);
		if (rc < 0) {
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_clear_command(master);
			sm_transition(sm, master_state_running);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		master_release_resources(master);
		master_clear_command(master);
		sm_transition(sm, master_state_idle);
		sm->phase = WAIT_TRANSITION;
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_running(struct statemachine *sm)
{
	enum {
		RUNNING = USER_STATUS,
		EXEC_DEACTIVATE,
		EXEC_RESET,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;
	int abnormal = 0;
	int rc;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 1;
			master->cycle.consecutive_errors = 0;
		}
		sm->count = 0;
		sm->phase = RUNNING;
		break;
	case RUNNING:
		sm->count++;
		if (!master) {
			break;
		}
		if (master_take_cycle_result(master, &abnormal)) {
			if (abnormal) {
				master->cycle.consecutive_errors++;
				if (master->cycle.consecutive_errors >=
				    MO_ECAT_MASTER_FAULT_THRESHOLD) {
					sm_transition(sm, master_state_fault);
				} else {
					sm_transition(sm, master_state_degraded);
				}
				break;
			}
			master->cycle.consecutive_errors = 0;
		}
		if (master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_DEACTIVATE)) {
			sm->phase = EXEC_DEACTIVATE;
		} else if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			sm->phase = EXEC_RESET;
		} else {
			master_reject_command(master);
		}
		break;
	case EXEC_DEACTIVATE:
		rc = master_backend_deactivate(master);
		if (rc < 0) {
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_clear_command(master);
			sm_transition(sm, master_state_ready);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		rc = master_backend_deactivate(master);
		if (rc < 0) {
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_release_resources(master);
			master_clear_command(master);
			sm_transition(sm, master_state_idle);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_degraded(struct statemachine *sm)
{
	enum {
		RUNNING = USER_STATUS,
		EXEC_DEACTIVATE,
		EXEC_RESET,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;
	int abnormal = 0;
	int rc;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 1;
		}
		sm->count = 0;
		sm->phase = RUNNING;
		break;
	case RUNNING:
		sm->count++;
		if (!master) {
			break;
		}
		if (master_take_cycle_result(master, &abnormal)) {
			if (abnormal) {
				master->cycle.consecutive_errors++;
				if (master->cycle.consecutive_errors >=
				    MO_ECAT_MASTER_FAULT_THRESHOLD) {
					sm_transition(sm, master_state_fault);
				}
				break;
			}
			master->cycle.consecutive_errors = 0;
			sm_transition(sm, master_state_running);
			break;
		}
		if (master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_DEACTIVATE)) {
			sm->phase = EXEC_DEACTIVATE;
		} else if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			sm->phase = EXEC_RESET;
		} else {
			master_reject_command(master);
		}
		break;
	case EXEC_DEACTIVATE:
		rc = master_backend_deactivate(master);
		if (rc < 0) {
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_clear_command(master);
			sm_transition(sm, master_state_ready);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		rc = master_backend_deactivate(master);
		if (rc < 0) {
			master_clear_command(master);
			sm_transition(sm, master_state_fault);
		} else {
			master_release_resources(master);
			master_clear_command(master);
			sm_transition(sm, master_state_idle);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_fault(struct statemachine *sm)
{
	enum {
		WAIT_RESET = USER_STATUS,
		WAIT_TRANSITION
	};
	struct mo_ecat_master *master;

	if (!sm) {
		return;
	}

	master = master_from_sm(sm);

	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->image.active = 0;
		}
		sm->count = 0;
		sm->phase = WAIT_RESET;
		break;
	case WAIT_RESET:
		sm->count++;
		if (!master || master->command == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			master_release_resources(master);
			master_clear_command(master);
			sm_transition(sm, master_state_idle);
			sm->phase = WAIT_TRANSITION;
		} else {
			master_reject_command(master);
		}
		break;
	case WAIT_TRANSITION:
		sm->count++;
		break;
	case EXIT:
	default:
		break;
	}
}
