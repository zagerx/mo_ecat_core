/*
 * master_states.c - 主站生命周期状态函数
 *
 * 实现主站状态机各状态：INIT、IDLE、READY、RUNNING、FAULT。
 * 负责处理外部命令、总线扫描、DC 配置、PDO 映射建立以及周期数据交换。
 */

#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "master_priv.h"

/**
 * master_set_fault - 设置主站故障码
 * @master: 主站对象指针
 * @error: 故障码
 */
static void master_set_fault(struct mo_ecat_master *master,
			     enum mo_ecat_master_error error)
{
	if (master) {
		atomic_store(&master->error_code, error);
	}
}

/**
 * master_state_init - 主站 INIT 状态
 * @sm: 状态机实例指针
 *
 * 进入 INIT 状态后自动切换到 IDLE 状态。
 */
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
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_INIT);
			sm_transition(sm, master_state_idle);
		}
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}

/**
 * master_state_idle - 主站 IDLE 状态
 * @sm: 状态机实例指针
 *
 * 等待 SCAN / CONFIGURE / RESET 命令，依次完成后端打开、从站扫描、
 * 状态刷新、PDO 描述读取、DC 配置以及 PDO 映射建立。
 */
void master_state_idle(struct statemachine *sm)
{
	enum {
		MASTER_PHASE_START = SM_PHASE_START,
		MASTER_PHASE_OPEN,
		MASTER_PHASE_SCAN_BUILD,
		MASTER_PHASE_READ_STATE,
		MASTER_PHASE_READ_PDO,
		MASTER_PHASE_WAIT_CONFIGURE,
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
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_IDLE);
			master->pdo_mapping.is_active = 0;
		}
		sm->phase = MASTER_PHASE_START;
		break;
	case MASTER_PHASE_START:
		cmd = master_take_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_resources_release(master);
			sm->phase = MASTER_PHASE_START;
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_SCAN) {
			break;
		}

		sm->phase = MASTER_PHASE_OPEN;
		break;
	case MASTER_PHASE_OPEN:
		master_resources_release(master);

		result = backend_init(&master->backend);
		if (result == 0) {
			result = backend_open(&master->backend, master->config);
		}
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_SCAN_BUILD;
		break;
	case MASTER_PHASE_SCAN_BUILD:
		result = backend_load_slave_info(&master->backend, &slave_count);
		if (result == 0) {
			result = master_topology_build(master);
		}
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_READ_STATE;
		break;
	case MASTER_PHASE_READ_STATE:
		result = master_topology_refresh_states(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_READ_PDO;
		break;
	case MASTER_PHASE_READ_PDO:
		result = backend_read_pdo_entries(&master->backend,
					  master->topology.slaves,
					  master->topology.slave_count);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_READ_CYCLIC_DESCRIPTION_FAILED);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_WAIT_CONFIGURE;
		break;
	case MASTER_PHASE_WAIT_CONFIGURE:
		cmd = master_take_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_resources_release(master);
			sm->phase = MASTER_PHASE_START;
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_SCAN) {
			sm->phase = MASTER_PHASE_OPEN;
			break;
		}
		if (cmd != MO_ECAT_MASTER_CMD_CONFIGURE) {
			break;
		}

		sm->phase = MASTER_PHASE_CONFIGURE_DC;
		break;
	case MASTER_PHASE_CONFIGURE_DC:
		/* DC 配置独立失败，不能继续建立 PDO 映射。 */
		result = backend_configure_dc(&master->backend);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}
		sm->phase = MASTER_PHASE_BUILD_PDO_MAPPING;
		break;
	case MASTER_PHASE_BUILD_PDO_MAPPING:
		/* 后端建立 PDO 数据区域并回填所有 PDO entry 的地址偏移。 */
		result = master_pdo_mapping_build(master);
		if (result < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_CONFIGURE_CYCLIC_MAPPING_FAILED);
			master_resources_release(master);
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

/**
 * master_state_ready - 主站 READY 状态
 * @sm: 状态机实例指针
 *
 * 等待 ACTIVATE / RESET 命令，激活 PDO 映射后进入 RUNNING 状态。
 */
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
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_READY);
			master->pdo_mapping.is_active = 0;
		}
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
		cmd = master_take_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_resources_release(master);
			sm_transition(sm, master_state_idle);
		} else if (cmd == MO_ECAT_MASTER_CMD_ACTIVATE) {
			if (master_pdo_mapping_activate(master) < 0) {
				master_set_fault(master, MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED);
				master_resources_release(master);
				sm_transition(sm, master_state_fault);
			} else {
				sm_transition(sm, master_state_running);
			}
		}
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}

/**
 * master_state_running - 主站 RUNNING 状态
 * @sm: 状态机实例指针
 *
 * 执行周期数据接收、用户回调、周期数据发送，并处理 RESET / DEACTIVATE 命令。
 */
void master_state_running(struct statemachine *sm)
{
	struct mo_ecat_master *master;
	enum mo_ecat_master_cmd cmd;
	struct mo_ecat_cyclic_result result;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case SM_PHASE_ENTER:
		if (master) {
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_RUNNING);
		}
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
		cmd = master_take_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_resources_release(master);
			sm_transition(sm, master_state_idle);
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_DEACTIVATE) {
			master_pdo_mapping_deactivate(master);
			sm_transition(sm, master_state_ready);
			break;
		}

		if (master_cyclic_receive(master, &result) < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}

		if (master->cyclic_callback) {
			master->cyclic_callback(master, &result, master->user_data);
		}

		if (master_cyclic_send(master, &result) < 0) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT);
			master_resources_release(master);
			sm_transition(sm, master_state_fault);
		}
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}

/**
 * master_state_fault - 主站 FAULT 状态
 * @sm: 状态机实例指针
 *
 * 进入故障状态后仅响应 RESET 命令，复位后回到 IDLE 状态。
 */
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
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_FAULT);
			master->pdo_mapping.is_active = 0;
		}
		sm->phase = SM_PHASE_START;
		break;
	case SM_PHASE_START:
		cmd = master_take_cmd(master);
		if (cmd == MO_ECAT_MASTER_CMD_NONE) {
			break;
		}
		if (cmd == MO_ECAT_MASTER_CMD_RESET) {
			master_resources_release(master);
			sm_transition(sm, master_state_idle);
		}
		break;
	case SM_PHASE_EXIT:
	default:
		break;
	}
}
