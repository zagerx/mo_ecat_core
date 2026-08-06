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
static void master_set_fault(struct mo_ecat_master *master, enum mo_ecat_master_error error,
			     enum master_error_detail detail)
{
	if (master) {
		atomic_store(&master->error_code, error);
		master->last_error.master_error = error;
		master->last_error.detail = detail;
		master->last_error.source =
			(detail == MASTER_ERROR_INVALID_ARGUMENT || detail == MASTER_ERROR_INVALID_STATE ||
			 detail == MASTER_ERROR_NO_MEMORY) ?
				MASTER_ERROR_SOURCE_CORE : MASTER_ERROR_SOURCE_BACKEND;
		master->last_error.native_code = 0;
		master->last_error.slave_index = SIZE_MAX;
		master->last_error.object_index = 0;
		master->last_error.object_subindex = 0;
	}
}

/**
 * master_idle_fail - 结束 IDLE 状态中的配置流程并进入故障状态
 * @sm: 状态机实例指针
 * @master: 主站对象指针
 * @error: 本次流程失败对应的故障码
 *
 * IDLE 内的扫描、PDO 描述读取和映射建立共享相同的失败收尾：记录故障、
 * 释放已获取资源，并迁移至 FAULT。该函数只收敛这条固定故障出口。
 */
static void master_idle_fail(struct statemachine *sm, struct mo_ecat_master *master,
			     enum mo_ecat_master_error error,
			     enum master_error_detail detail)
{
	master_set_fault(master, error, detail);
	if (mo_ecat_master_get_node_count(master) > 0U) {
		(void)master_topology_refresh_states(master);
	}
	master_runtime_release(master);
	sm_transition(sm, master_state_fault);
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
	size_t slave_count;
	enum master_error_detail error;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case SM_PHASE_ENTER:
		if (master) {
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_IDLE);
			master->pdo_layout.is_active = 0;
		}
		sm->phase = MASTER_PHASE_START;
		break;

	case MASTER_PHASE_START: {
		switch (master_take_cmd(master)) {
		case MO_ECAT_MASTER_CMD_RESET:
			master_resources_release(master);
			break;
		case MO_ECAT_MASTER_CMD_SCAN:
			sm->phase = MASTER_PHASE_OPEN;
			break;
		case MO_ECAT_MASTER_CMD_NONE:
		default:
			break;
		}
	} break;

	case MASTER_PHASE_OPEN: {
		master_resources_release(master);
		atomic_store(&master->error_code, MO_ECAT_MASTER_ERROR_NONE);
		error = master_error_from_backend(backend_init(&master->backend));
		if (error == MASTER_ERROR_NONE) {
			error = master_error_from_backend(backend_open(&master->backend, master->config));
		}
		if (error != MASTER_ERROR_NONE) {
			master_idle_fail(sm, master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED, error);
			break;
		}
		sm->phase = MASTER_PHASE_SCAN_BUILD;
	} break;

	case MASTER_PHASE_SCAN_BUILD: {
		error = master_error_from_backend(backend_load_slave_info(&master->backend,
									    &slave_count));
		if (error == MASTER_ERROR_NONE) {
			error = master_topology_build(master, slave_count);
		}
		if (error != MASTER_ERROR_NONE) {
			master_idle_fail(sm, master, MO_ECAT_MASTER_ERROR_DISCOVER_FAILED, error);
			break;
		}
		sm->phase = MASTER_PHASE_READ_STATE;
	} break;

	case MASTER_PHASE_READ_STATE: {
		error = master_topology_refresh_states(master);
		if (error != MASTER_ERROR_NONE) {
			master_idle_fail(sm, master, MO_ECAT_MASTER_ERROR_BUS_FAULT, error);
			break;
		}
		sm->phase = MASTER_PHASE_READ_PDO;
	} break;

	case MASTER_PHASE_READ_PDO: {
		pthread_mutex_lock(&master->topology_mutex);
		error = master_error_from_backend(backend_read_pdo_entries(
			&master->backend, master->topology.slaves, master->topology.slave_count));
		pthread_mutex_unlock(&master->topology_mutex);
		if (error != MASTER_ERROR_NONE) {
			master_idle_fail(sm, master,
					 MO_ECAT_MASTER_ERROR_READ_PDO_DESCRIPTION_FAILED, error);
			break;
		}
		sm->phase = MASTER_PHASE_WAIT_CONFIGURE;
	} break;

	case MASTER_PHASE_WAIT_CONFIGURE: {
		switch (master_take_cmd(master)) {
		case MO_ECAT_MASTER_CMD_RESET:
			master_resources_release(master);
			sm->phase = MASTER_PHASE_START;
			break;
		case MO_ECAT_MASTER_CMD_SCAN:
			sm->phase = MASTER_PHASE_OPEN;
			break;
		case MO_ECAT_MASTER_CMD_CONFIGURE:
			sm->phase = MASTER_PHASE_CONFIGURE_DC;
			break;
		case MO_ECAT_MASTER_CMD_NONE:
		default:
			break;
		}
	} break;

	case MASTER_PHASE_CONFIGURE_DC: { /* DC 配置独立失败，不能继续建立 PDO 映射。 */
		error = master_error_from_backend(backend_configure_dc(&master->backend));
		if (error != MASTER_ERROR_NONE) {
			master_idle_fail(sm, master, MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED, error);
			break;
		}
		sm->phase = MASTER_PHASE_BUILD_PDO_MAPPING;
	} break;

	case MASTER_PHASE_BUILD_PDO_MAPPING:
		/* 后端建立 PDO 数据区域并回填所有 PDO entry 的地址偏移。 */
		{
			error = master_pdo_layout_build(master);
			if (error != MASTER_ERROR_NONE) {
				master_idle_fail(
					sm, master,
					MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_MAPPING_FAILED, error);
				break;
			}
			sm_transition(sm, master_state_ready);
		}
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
	enum master_error_detail error;

	if (!sm) {
		return;
	}

	master = (struct mo_ecat_master *)sm->data;
	switch (sm->phase) {
	case SM_PHASE_ENTER:
		if (master) {
			atomic_store(&master->state, MO_ECAT_MASTER_STATE_READY);
			master->pdo_layout.is_active = 0;
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
			error = master_pdo_layout_activate(master);
			if (error != MASTER_ERROR_NONE) {
				master_set_fault(master, MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED, error);
				(void)master_topology_refresh_states(master);
				master_runtime_release(master);
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
	enum master_error_detail error;

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
			error = master_pdo_layout_deactivate(master);
			if (error != MASTER_ERROR_NONE) {
				master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT, error);
				(void)master_topology_refresh_states(master);
				master_runtime_release(master);
				sm_transition(sm, master_state_fault);
			} else {
				sm_transition(sm, master_state_ready);
			}
			break;
		}

		error = master_cyclic_receive(master, &result);
		if (error != MASTER_ERROR_NONE) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT, error);
			(void)master_topology_refresh_states(master);
			master_runtime_release(master);
			sm_transition(sm, master_state_fault);
			break;
		}

		if (master->cyclic_callback) {
			master->cyclic_callback(master, &result, master->user_data);
		}

		error = master_cyclic_send(master, &result);
		if (error != MASTER_ERROR_NONE) {
			master_set_fault(master, MO_ECAT_MASTER_ERROR_BUS_FAULT, error);
			(void)master_topology_refresh_states(master);
			master_runtime_release(master);
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
			master->pdo_layout.is_active = 0;
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
