/*
 * soem_cyclic.c - SOEM 周期数据交换与节点状态读取
 *
 * 实现 SOEM 后端的周期通信激活/去激活、过程数据收发以及从站状态读取。
 */

#include "soem_backend.h"
#include "topology_priv.h"

/**
 * soem_backend_activate - 激活 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * 发送一帧过程数据，并将所有从站切换到 OPERATIONAL 状态。
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_activate(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context) {
		return -1;
	}
	ecx_send_processdata(&context->context);
	context->context.slavelist[0].state = EC_STATE_OPERATIONAL;
	ecx_writestate(&context->context, 0);
	return ecx_statecheck(&context->context, 0, EC_STATE_OPERATIONAL, 50000) ==
		       EC_STATE_OPERATIONAL ? 0 : -1;
}

/**
 * soem_backend_cyclic_receive - SOEM 后端周期接收
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_cyclic_receive(struct backend_instance *backend,
				struct mo_ecat_cyclic_result *result)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	int wkc;

	if (!context || !result) {
		return -1;
	}
	wkc = ecx_receive_processdata(&context->context, EC_TIMEOUTRET);
	result->link_up = wkc > 0;
	result->actual_wkc = wkc > 0 ? (uint32_t)wkc : 0;
	result->expected_wkc = context->expected_wkc;
	result->dc_time_ns = context->context.DCtime;
	result->dc_time_valid = 1;
	if (wkc <= 0) {
		result->diagnostics_required = 1;
		return -1;
	}
	if (result->expected_wkc > 0 && result->actual_wkc != result->expected_wkc) {
		result->diagnostics_required = 1;
	}
	return 0;
}

/**
 * soem_backend_cyclic_send - SOEM 后端周期发送
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_cyclic_send(struct backend_instance *backend,
			     struct mo_ecat_cyclic_result *result)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !result) {
		return -1;
	}
	if (ecx_send_processdata(&context->context) <= 0) {
		result->diagnostics_required = 1;
		return -1;
	}
	return 0;
}

/**
 * soem_backend_read_all_slave_states - 读取 SOEM 所有从站状态
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_read_all_slave_states(struct backend_instance *backend,
				       struct master_slave *slaves, size_t slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || (slave_count > 0 && !slaves)) {
		return -1;
	}
	ecx_readstate(&context->context);
	if (slave_count != (size_t)context->context.slavecount) {
		return -1;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		const ec_slavet *slave = &context->context.slavelist[i + 1];

		slaves[i].state.al_state = soem_backend_node_al_state(slave->state);
		slaves[i].state.has_error = slave->ALstatuscode != 0;
		slaves[i].state.al_status_code = slave->ALstatuscode;
		slaves[i].state.is_online = 1;
		slaves[i].state.is_operational =
			slaves[i].state.al_state == MO_ECAT_NODE_AL_STATE_OP;
	}

	return 0;
}

/**
 * soem_backend_read_single_slave_state - 读取 SOEM 单个从站状态
 * @backend: 后端实例指针
 * @slave_index: 从站索引
 * @state: 用于返回从站状态的指针
 *
 * 通过 FPRD 读取指定从站的 AL 状态寄存器。
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_read_single_slave_state(struct backend_instance *backend,
					 size_t slave_index,
					 struct mo_ecat_node_state *state)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	ec_alstatust al_status;
	uint16_t config_address;
	int wkc;

	if (!context || !state || slave_index >= (size_t)context->context.slavecount) {
		return -1;
	}
	config_address = context->context.slavelist[slave_index + 1].configadr;
	wkc = ecx_FPRD(&context->context.port, config_address, ECT_REG_ALSTAT,
		       sizeof(al_status), &al_status, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return -1;
	}

	state->al_state = soem_backend_node_al_state(etohs(al_status.alstatus));
	state->has_error = (etohs(al_status.alstatus) & EC_STATE_ERROR) != 0;
	state->al_status_code = etohs(al_status.alstatuscode);
	state->is_online = 1;
	state->is_operational = state->al_state == MO_ECAT_NODE_AL_STATE_OP;
	return 0;
}

/**
 * soem_backend_deactivate - 停用 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * 将所有从站切换到 SAFE_OP 状态。
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_deactivate(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context) {
		return -1;
	}
	context->context.slavelist[0].state = EC_STATE_SAFE_OP;
	ecx_writestate(&context->context, 0);
	return 0;
}
