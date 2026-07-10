/**
 * @file master_states.c
 * @brief 当前发现阶段的主站生命周期状态函数
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

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
			master->image.active = 0;
		}
		sm->phase = SCAN;
		break;
	case SCAN:
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
			result = master_scan(master, &slave_count);
		}
		if (result == 0) {
			result = master_build_topology(master, slave_count);
		}

		master_clear_cmd(master);
		if (result < 0) {
			master_release_resources(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = CONFIG_PDO;
		break;
	case CONFIG_PDO:
		/*
		 * 扫描阶段只保存从站身份与基础通信能力；这里读取从站默认
		 * PDO 映射描述。此时不创建过程数据映像，也不请求 OP 状态。
		 */
		result = master_read_pdo_entries(master);
		if (result < 0) {
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

	master = (struct mo_ecat_master *)sm->data;
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
