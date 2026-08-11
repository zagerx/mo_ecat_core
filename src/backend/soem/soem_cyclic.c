/*
 * soem_cyclic.c - SOEM 周期数据交换与节点状态读取
 *
 * 实现 SOEM 后端的周期通信激活/去激活、过程数据收发以及从站状态读取。
 */

#include "soem_backend.h"
#include "slave_priv.h"

/**
 * soem_backend_activate - 激活 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * 发送一帧过程数据，并将所有从站切换到 OPERATIONAL 状态。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_activate(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	enum {
		SOEM_OPERATIONAL_RETRIES = 40
	};

	if (!context) {
		return BACKEND_ERROR_NOT_READY;
	}
	/* OP 切换需要有效过程数据。先预发送一帧，再在等待期间持续交换，
	 * 与 SOEM 官方示例的激活时序保持一致。 */
	ecx_send_processdata(&context->context);
	(void)ecx_receive_processdata(&context->context, EC_TIMEOUTRET);
	context->context.slavelist[0].state = EC_STATE_OPERATIONAL;
	ecx_writestate(&context->context, 0);
	for (int attempt = 0; attempt < SOEM_OPERATIONAL_RETRIES; ++attempt) {
		ecx_send_processdata(&context->context);
		(void)ecx_receive_processdata(&context->context, EC_TIMEOUTRET);
		if (ecx_statecheck(&context->context, 0, EC_STATE_OPERATIONAL, 50000) ==
		    EC_STATE_OPERATIONAL) {
			/* RUNNING 的第一个动作是 receive，预留一帧待接收过程数据。 */
			if (ecx_send_processdata(&context->context) <= 0) {
				return BACKEND_ERROR_ACTIVATE_FAILED;
			}
			return BACKEND_ERROR_NONE;
		}
	}
	(void)ecx_readstate(&context->context);
	return BACKEND_ERROR_ACTIVATE_FAILED;
}

/**
 * soem_backend_cyclic_receive - SOEM 后端周期接收
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_cyclic_receive(struct backend_instance *backend,
					       struct mo_ecat_cyclic_result *result)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	int wkc;

	if (!context || !result) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	wkc = ecx_receive_processdata(&context->context, EC_TIMEOUTRET);
	result->link_up = wkc > 0;
	result->actual_wkc = wkc > 0 ? (uint32_t)wkc : 0;
	result->expected_wkc = context->expected_wkc;
	result->dc_time_ns = context->context.DCtime;
	result->dc_time_valid = 1;
	if (wkc <= 0) {
		result->diagnostics_required = 1;
		return BACKEND_ERROR_CYCLIC_RECEIVE_FAILED;
	}
	if (result->expected_wkc > 0 && result->actual_wkc != result->expected_wkc) {
		result->diagnostics_required = 1;
	}
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_cyclic_send - SOEM 后端周期发送
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_cyclic_send(struct backend_instance *backend,
					    struct mo_ecat_cyclic_result *result)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !result) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (ecx_send_processdata(&context->context) <= 0) {
		result->diagnostics_required = 1;
		return BACKEND_ERROR_CYCLIC_SEND_FAILED;
	}
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_read_all_slave_states - 读取 SOEM 所有从站状态
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_read_all_slave_states(struct backend_instance *backend,
						      struct slave *slaves, size_t slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	size_t online_count = 0;

	if (!context || !context->opened || (slave_count > 0 && !slaves)) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (slave_count != (size_t)context->context.slavecount) {
		return BACKEND_ERROR_READ_NODE_STATE_FAILED;
	}

	/* ecx_readstate 返回值是"最低状态值"而非 WKC，不能用作在线依据。
	 * SOEM 对无应答从站写入 state=0（EC_STATE_NONE），据此逐站标记。 */
	(void)ecx_readstate(&context->context);

	for (size_t i = 0; i < slave_count; ++i) {
		const ec_slavet *slave = &context->context.slavelist[i + 1];

		slaves[i].state.al_state = soem_backend_node_al_state(slave->state);
		slaves[i].state.has_error = slave->ALstatuscode != 0;
		slaves[i].state.al_status_code = slave->ALstatuscode;
		slaves[i].state.is_online = (slave->state & 0x0f) != EC_STATE_NONE;
		slaves[i].state.is_operational =
			slaves[i].state.is_online != 0 &&
			slaves[i].state.al_state == MO_ECAT_NODE_AL_STATE_OP;
		if (slaves[i].state.is_online) {
			++online_count;
		}
	}

	return online_count > 0 ? BACKEND_ERROR_NONE : BACKEND_ERROR_READ_NODE_STATE_FAILED;
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
enum backend_error soem_backend_read_single_slave_state(struct backend_instance *backend,
							size_t slave_index,
							struct mo_ecat_node_state *state)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	ec_alstatust al_status;
	uint16_t config_address;
	int wkc;

	if (!context || !state || slave_index >= (size_t)context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	config_address = context->context.slavelist[slave_index + 1].configadr;
	wkc = ecx_FPRD(&context->context.port, config_address, ECT_REG_ALSTAT, sizeof(al_status),
		       &al_status, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return BACKEND_ERROR_READ_NODE_STATE_FAILED;
	}

	state->al_state = soem_backend_node_al_state(etohs(al_status.alstatus));
	state->has_error = (etohs(al_status.alstatus) & EC_STATE_ERROR) != 0;
	state->al_status_code = etohs(al_status.alstatuscode);
	state->is_online = 1;
	state->is_operational = state->al_state == MO_ECAT_NODE_AL_STATE_OP;
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_deactivate - 停用 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * 将所有从站切换到 SAFE_OP 状态。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_deactivate(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context) {
		return BACKEND_ERROR_NOT_READY;
	}
	context->context.slavelist[0].state = EC_STATE_SAFE_OP;
	ecx_writestate(&context->context, 0);
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_recover_slave - 恢复单个从站到 OP
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（核心层逻辑下标，0 起）
 *
 * 处理链路恢复后停在 SAFE-OP(+错误) 的从站：清 AL 错误码后请求迁移 OP。
 * ESC 的 SM/FMMU/DC 配置在链路断开期间保持有效，无需重新配置。
 *
 * 只下发请求不等待迁移完成：本函数在调度线程内执行，同步等待会
 * 中断周期帧导致其他从站 SM 看门狗超时。迁移结果由核心层周期
 * 状态刷新呈现。
 *
 * Return: 0 请求已下发，非 0 失败
 */
enum backend_error soem_backend_recover_slave(struct backend_instance *backend, size_t slave_index)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);
	uint16_t al_status = 0;
	uint16_t state;

	if (!context || !context->pdo_mapping_ready) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (slave_number > (uint16_t)context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	if (ecx_FPRD(&context->context.port, context->context.slavelist[slave_number].configadr,
		     ECT_REG_ALSTAT, sizeof(al_status), &al_status, EC_TIMEOUTRET) <= 0) {
		return BACKEND_ERROR_SLAVE_RECOVER_FAILED;
	}
	state = etohs(al_status);

	if ((state & 0x0fU) == EC_STATE_OPERATIONAL) {
		return BACKEND_ERROR_NONE;
	}
	if ((state & 0x0fU) != EC_STATE_SAFE_OP) {
		/* INIT/PRE-OP 需重新配置，当前仅支持 SAFE-OP 恢复。 */
		return BACKEND_ERROR_SLAVE_RECOVER_FAILED;
	}

	if ((state & EC_STATE_ERROR) != 0U) {
		context->context.slavelist[slave_number].state = EC_STATE_SAFE_OP | EC_STATE_ACK;
		ecx_writestate(&context->context, slave_number);
	}

	context->context.slavelist[slave_number].state = EC_STATE_OPERATIONAL;
	ecx_writestate(&context->context, slave_number);
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_al_state_to_soem - 核心层 AL 状态转换为 SOEM 状态值
 * @al_state: 核心层 AL 状态枚举
 *
 * Return: SOEM 状态值；UNKNOWN 时返回 EC_STATE_NONE
 */
static uint16_t soem_backend_al_state_to_soem(enum mo_ecat_node_al_state al_state)
{
	switch (al_state) {
	case MO_ECAT_NODE_AL_STATE_INIT:
		return EC_STATE_INIT;
	case MO_ECAT_NODE_AL_STATE_PRE_OP:
		return EC_STATE_PRE_OP;
	case MO_ECAT_NODE_AL_STATE_SAFE_OP:
		return EC_STATE_SAFE_OP;
	case MO_ECAT_NODE_AL_STATE_OP:
		return EC_STATE_OPERATIONAL;
	case MO_ECAT_NODE_AL_STATE_BOOTSTRAP:
		return EC_STATE_BOOT;
	default:
		return EC_STATE_NONE;
	}
}

/**
 * soem_backend_set_slave_al_state - 设置单个从站 AL 状态（调试用途）
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（核心层逻辑下标，0 起）
 * @target_state: 目标 AL 状态
 *
 * 直接写从站 AL Control 寄存器，不检查当前状态、不清错误码、
 * 不等待迁移完成。操作结果由核心层周期状态刷新呈现。
 *
 * 仅 DEBUG_SLAVE 状态调用；配置不完整（如无 PDO 映射）时不检查。
 *
 * Return: 0 请求已下发，非 0 失败
 */
enum backend_error soem_backend_set_slave_al_state(struct backend_instance *backend,
						   size_t slave_index,
						   enum mo_ecat_node_al_state target_state)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);
	const uint16_t soem_state = soem_backend_al_state_to_soem(target_state);

	if (!context || !context->opened) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (slave_number > (uint16_t)context->context.slavecount || slave_number == 0U) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (soem_state == EC_STATE_NONE) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	context->context.slavelist[slave_number].state = soem_state;
	ecx_writestate(&context->context, slave_number);
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_sdo_read - SOEM SDO 读
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（0 起）
 * @object_index: CoE 对象字典索引
 * @object_subindex: 子索引
 * @psize: 输入期望字节数，输出实际读取字节数
 * @data: 数据缓冲区
 *
 * 封装 ecx_SDOread，调用线程阻塞至传输完成。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_sdo_read(struct backend_instance *backend, size_t slave_index,
					 uint16_t object_index, uint8_t object_subindex, int *psize,
					 void *data)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);

	if (!context || !context->opened) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (slave_number > (uint16_t)context->context.slavecount || slave_number == 0U) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (!psize || !data || *psize <= 0) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	const int wkc = ecx_SDOread(&context->context, slave_number, object_index, object_subindex,
				    FALSE, psize, data, EC_TIMEOUTRXM);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}
	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_sdo_write - SOEM SDO 写
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（0 起）
 * @object_index: CoE 对象字典索引
 * @object_subindex: 子索引
 * @size: 写入数据字节数
 * @data: 数据缓冲区
 *
 * 封装 ecx_SDOwrite，调用线程阻塞至传输完成。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_sdo_write(struct backend_instance *backend, size_t slave_index,
					  uint16_t object_index, uint8_t object_subindex, int size,
					  const void *data)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);

	if (!context || !context->opened) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (slave_number > (uint16_t)context->context.slavecount || slave_number == 0U) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (!data || size <= 0) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	const int wkc = ecx_SDOwrite(&context->context, slave_number, object_index, object_subindex,
				     FALSE, size, data, EC_TIMEOUTRXM);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_WRITE_FAILED;
	}
	return BACKEND_ERROR_NONE;
}
