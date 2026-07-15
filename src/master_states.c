/**
 * @file master_states.c
 * @brief 主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

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
	case SM_PHASE_ENTER:
		if (master) {
			sm_transition(sm, master_state_idle);
		}
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}

void master_state_idle(struct statemachine *sm)
{
	enum {
		MASTER_PHASE_START = SM_PHASE_START,
		MASTER_PHASE_OPEN,
		MASTER_PHASE_SCAN_BUILD,
		MASTER_PHASE_READ_STATE,
		MASTER_PHASE_READ_PDO,
		MASTER_PHASE_DISCOVERED,
		MASTER_PHASE_CONFIGURE_DC,
		MASTER_PHASE_BUILD_PDO_MAPPING,
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
	case SM_PHASE_ENTER:
		if (master) {
			master->pdo_mapping.active = 0;
		}
		sm->phase = MASTER_PHASE_START;
		break;
	case MASTER_PHASE_START:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_release_resources(master);
			sm->phase = MASTER_PHASE_START;
			master_clear_cmd(master);
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_SCAN) {
			master_clear_cmd(master);
			break;
		}

		master_clear_cmd(master);
		sm->phase = MASTER_PHASE_OPEN;
		break;
	case MASTER_PHASE_OPEN:
		master_release_resources(master);

		result = backend_init(&master->backend);
		if (result == 0) {
			result = backend_open(&master->backend, master->config);
		}
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_SCAN_BUILD;
		break;
	case MASTER_PHASE_SCAN_BUILD:
		result = backend_load_slave_info(&master->backend, &slave_count);
		if (result == 0) {
			result = master_build_slave_table(master);
		}
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_READ_STATE;
		break;
	case MASTER_PHASE_READ_STATE:
		result = master_read_all_slave_states(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_READ_PDO;
		break;
	case MASTER_PHASE_READ_PDO:
		result = backend_read_pdo_entries(&master->backend,
						  master->slave_table.slaves,
						  master->slave_table.count);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_READ_DEFAULT_PDO_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_DISCOVERED;
		break;
	case MASTER_PHASE_DISCOVERED:
		cmd = master_read_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_release_resources(master);
			sm->phase = MASTER_PHASE_START;
			master_clear_cmd(master);
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_SCAN) {
			master_clear_cmd(master);
			sm->phase = MASTER_PHASE_OPEN;
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_CONFIGURE) {
			master_clear_cmd(master);
			break;
		}

		master_clear_cmd(master);
		sm->phase = MASTER_PHASE_CONFIGURE_DC;
		break;
	case MASTER_PHASE_CONFIGURE_DC:
		/* DC 配置独立失败，不能继续建立 PDO 映射。 */
		result = backend_configure_dc(&master->backend);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_BUILD_PDO_MAPPING;
		break;
	case MASTER_PHASE_BUILD_PDO_MAPPING:
		/* 后端建立 PDO 数据区域并回填所有 PDO entry 的地址偏移。 */
		result = master_build_pdo_mapping(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_MAPPING_FAILED);
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm_transition(sm, master_state_ready);
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}

enum mo_ecat_master_state master_state_from_sm(const struct mo_ecat_master *master)
{
	sm_state_t current_state;

	if (!master) {
		return MO_ECAT_MASTER_STATE_INIT;
	}

	current_state = master->sm.current_state;
	if (current_state == master_state_idle) {
		return MO_ECAT_MASTER_STATE_IDLE;
	}
	if (current_state == master_state_ready) {
		return MO_ECAT_MASTER_STATE_READY;
	}
	if (current_state == master_state_running) {
		return MO_ECAT_MASTER_STATE_RUNNING;
	}
	if (current_state == master_state_fault) {
		return MO_ECAT_MASTER_STATE_FAULT;
	}

	return MO_ECAT_MASTER_STATE_INIT;
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
	case SM_PHASE_ENTER:
		if (master) {
			master->pdo_mapping.active = 0;
		}
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
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
	case SM_PHASE_EXIT:
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
	case SM_PHASE_ENTER:
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
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
	case SM_PHASE_EXIT:
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
	case SM_PHASE_ENTER:
		if (master) {
			master->pdo_mapping.active = 0;
		}
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
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
	case SM_PHASE_EXIT:
	default:
		break;
	}
}
