/**
 * @file mo_ecat_master_states.c
 * @brief 主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "mo_ecat_master_states.h"
#include "mo_ecat_master_priv.h"

#define MO_ECAT_MASTER_FAULT_THRESHOLD 3U

static struct mo_ecat_master *master_from_sm(struct statemachine *sm)
{
	return sm ? (struct mo_ecat_master *)sm->data : NULL;
}

static int master_command_is(struct mo_ecat_master *master,
			     enum mo_ecat_master_command command)
{
	return master && master->cmd.pending && master->cmd.id == command;
}

static void master_reject_command(struct mo_ecat_master *master)
{
	if (master && master->cmd.pending) {
		mo_ecat_master_clear_command(master, -1);
	}
}

void mo_ecat_master_state_init(struct statemachine *sm)
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
			sm_transition(sm, mo_ecat_master_state_idle);
		}
		break;
	case EXIT:
	default:
		break;
	}
}

void mo_ecat_master_state_idle(struct statemachine *sm)
{
	enum {
		WAIT_COMMAND = USER_STATUS,
		EXEC_CONFIGURE,
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
		if (!master || !master->cmd.pending) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_CONFIGURE)) {
			sm->phase = EXEC_CONFIGURE;
		} else if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			mo_ecat_master_clear_command(master, 0);
		} else {
			master_reject_command(master);
		}
		break;
	case EXEC_CONFIGURE:
		rc = mo_ecat_master_prepare_config(master, master->cmd.pending_config,
						   &master->cmd.pending_backend_value);
		if (rc == 0) {
			rc = mo_ecat_master_backend_configure(master);
		}
		if (rc < 0) {
			mo_ecat_master_release_resources(master);
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_ready);
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

void mo_ecat_master_state_ready(struct statemachine *sm)
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
		if (!master || !master->cmd.pending) {
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
		rc = mo_ecat_master_backend_activate(master);
		if (rc < 0) {
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_running);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		mo_ecat_master_release_resources(master);
		mo_ecat_master_clear_command(master, 0);
		sm_transition(sm, mo_ecat_master_state_idle);
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

void mo_ecat_master_state_running(struct statemachine *sm)
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
		if (mo_ecat_master_take_cycle_result(master, &abnormal)) {
			if (abnormal) {
				master->cycle.consecutive_errors++;
				if (master->cycle.consecutive_errors >=
				    MO_ECAT_MASTER_FAULT_THRESHOLD) {
					sm_transition(sm, mo_ecat_master_state_fault);
				} else {
					sm_transition(sm, mo_ecat_master_state_degraded);
				}
				break;
			}
			master->cycle.consecutive_errors = 0;
		}
		if (!master->cmd.pending) {
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
		rc = mo_ecat_master_backend_deactivate(master);
		if (rc < 0) {
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_ready);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		rc = mo_ecat_master_backend_deactivate(master);
		if (rc < 0) {
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_release_resources(master);
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_idle);
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

void mo_ecat_master_state_degraded(struct statemachine *sm)
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
		if (mo_ecat_master_take_cycle_result(master, &abnormal)) {
			if (abnormal) {
				master->cycle.consecutive_errors++;
				if (master->cycle.consecutive_errors >=
				    MO_ECAT_MASTER_FAULT_THRESHOLD) {
					sm_transition(sm, mo_ecat_master_state_fault);
				}
				break;
			}
			master->cycle.consecutive_errors = 0;
			sm_transition(sm, mo_ecat_master_state_running);
			break;
		}
		if (!master->cmd.pending) {
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
		rc = mo_ecat_master_backend_deactivate(master);
		if (rc < 0) {
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_ready);
		}
		sm->phase = WAIT_TRANSITION;
		break;
	case EXEC_RESET:
		rc = mo_ecat_master_backend_deactivate(master);
		if (rc < 0) {
			mo_ecat_master_clear_command(master, rc);
			sm_transition(sm, mo_ecat_master_state_fault);
		} else {
			mo_ecat_master_release_resources(master);
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_idle);
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

void mo_ecat_master_state_fault(struct statemachine *sm)
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
		if (!master || !master->cmd.pending) {
			break;
		}
		if (master_command_is(master, MO_ECAT_MASTER_CMD_RESET)) {
			mo_ecat_master_release_resources(master);
			mo_ecat_master_clear_command(master, 0);
			sm_transition(sm, mo_ecat_master_state_idle);
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
