/**
 * @file master_states.c
 * @brief 主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

static void master_state_configuring(struct statemachine *sm);

static void master_set_fault(struct mo_ecat_master *master,
			     enum mo_ecat_master_error error)
{
	if (master) {
		master->error_code = error;
	}
}

void master_state_init(struct statemachine *sm)
{
	struct mo_ecat_master *master;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
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
	enum {
		SCAN = USER_STATUS,
		CONFIG_PDO,
	};
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;
	size_t slave_count;
	int result;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->process.image.active = 0;
		}
		sm->phase = SCAN;
		break;
	case SCAN:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_SCAN) {
			master_clear_cmd(master);
			break;
		}

		master_release_resources(master);
		result = backend_init(&master->backend);
		if (result == 0) {
			result = master_backend_open(master);
		}
		if (result == 0) {
			result = master_scan(master, &slave_count);
		}
		if (result == 0) {
			result = master_build_topology(master, slave_count);
		}

		master_clear_cmd(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = CONFIG_PDO;
		break;
	case CONFIG_PDO:
		result = master_read_pdo_entries(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm_transition(sm, master_state_discovered);
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

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->process.image.active = 0;
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
		} else if (cmd == MO_ECAT_MASTER_CMD_CONFIGURE) {
			sm_transition(sm, master_state_configuring);
		}
		master_clear_cmd(master);
		break;
	case EXIT:
	default:
		break;
	}
}

static void master_state_configuring(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	int result;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
		result = master_configure(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_IOMAP_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm_transition(sm, master_state_ready);
		break;
	case USER_STATUS:
	case EXIT:
	default:
		break;
	}
}

void master_state_ready(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->process.image.active = 0;
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
		} else if (cmd == MO_ECAT_MASTER_CMD_ACTIVATE) {
			if (master_activate(master) < 0) {
				master_set_fault(master, MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED);
				master_release_resources(master);
				sm_transition(sm, master_state_fault);
			} else {
				sm_transition(sm, master_state_running);
			}
		}
		master_clear_cmd(master);
		break;
	case EXIT:
	default:
		break;
	}
}

void master_state_running(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
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
		} else if (cmd == MO_ECAT_MASTER_CMD_DEACTIVATE) {
			master_deactivate(master);
			sm_transition(sm, master_state_ready);
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

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case ENTER:
		if (master) {
			master->process.image.active = 0;
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
