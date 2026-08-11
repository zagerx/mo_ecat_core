/*
 * mo_ecat_master.c - 主站对象、生命周期与命令接口
 *
 * 提供主站对象的创建/销毁、命令写入、状态查询以及状态机调度等公开接口。
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "master_priv.h"

/**
 * master_take_cmd - 取出主站当前命令
 * @master: 主站对象指针
 *
 * Return: 当前待处理的命令值；@master 为 NULL 时返回 MO_ECAT_MASTER_CMD_NONE
 */
enum mo_ecat_master_cmd master_take_cmd(struct mo_ecat_master *master)
{
	if (!master) {
		return MO_ECAT_MASTER_CMD_NONE;
	}

	return atomic_exchange(&master->command, MO_ECAT_MASTER_CMD_NONE);
}

/**
 * master_write_cmd - 向主站写入命令
 * @master: 主站对象指针
 * @cmd: 待写入的命令
 */
void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd)
{
	if (master) {
		atomic_store(&master->command, cmd);
	}
}

/**
 * master_init - 初始化主站对象
 * @master: 主站对象指针
 * @callback: 周期控制回调
 * @user_data: 用户私有数据
 *
 * Return: 0 成功，非 0 失败
 */
static int master_init(struct mo_ecat_master *master, mo_ecat_cyclic_callback callback,
		       void *user_data)
{
	if (!master) {
		return -1;
	}

	memset(master, 0, sizeof(*master));
	master->cyclic_callback = callback;
	master->user_data = user_data;
	atomic_init(&master->command, MO_ECAT_MASTER_CMD_NONE);
	atomic_init(&master->command_arg, -1L);
	atomic_init(&master->state, MO_ECAT_MASTER_STATE_INIT);
	atomic_init(&master->error_code, MO_ECAT_MASTER_ERROR_NONE);
	if (pthread_mutex_init(&master->slave_table_mutex, NULL) != 0) {
		return -1;
	}
	master->last_error.master_error = MO_ECAT_MASTER_ERROR_NONE;
	master->last_error.detail = BACKEND_ERROR_NONE;
	master->last_error.source = MASTER_ERROR_SOURCE_CORE;
	master->last_error.slave_index = SIZE_MAX;
	sm_init(&master->sm, master, master_state_init);
	return 0;
}

/**
 * master_deinit - 反初始化主站对象
 * @master: 主站对象指针
 */
static void master_deinit(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	master_resources_release(master);
	pthread_mutex_destroy(&master->slave_table_mutex);
	memset(master, 0, sizeof(*master));
}

/**
 * mo_ecat_master_create - 创建主站对象
 * @callback: 周期控制回调
 * @user_data: 用户私有数据
 *
 * 实例数量不受限制；每个实例持有独立的状态与后端上下文。
 *
 * Return: 成功返回主站对象指针，失败返回 NULL
 */
struct mo_ecat_master *mo_ecat_master_create(mo_ecat_cyclic_callback callback, void *user_data)
{
	struct mo_ecat_master *master;

	master = calloc(1, sizeof(*master));
	if (!master) {
		return NULL;
	}

	if (master_init(master, callback, user_data) < 0) {
		free(master);
		return NULL;
	}

	return master;
}

/**
 * mo_ecat_master_binding - 绑定主站配置
 * @master: 主站对象指针
 * @config: 主站配置指针（含EtherCAT网口），由调用方持有并保证唯一；
 *          主站不复制内容，配置对象必须比主站存活更久
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_binding(struct mo_ecat_master *master,
			   const struct mo_ecat_master_config *config)
{
	if (!master || !config || config->interface_name[0] == '\0') {
		return -1;
	}

	master->config = config;
	return 0;
}

/**
 * mo_ecat_master_destroy - 销毁主站对象
 * @master: 主站对象指针
 */
void mo_ecat_master_destroy(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	master_deinit(master);
	free(master);
}

/**
 * mo_ecat_master_write_cmd - 向主站写入命令
 * @master: 主站对象指针
 * @cmd: 待写入的命令
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd)
{
	if (!master || cmd <= MO_ECAT_MASTER_CMD_NONE ||
	    cmd > MO_ECAT_MASTER_CMD_SET_SLAVE_AL_STATE) {
		return -1;
	}

	master_write_cmd(master, cmd);
	return 0;
}

/**
 * mo_ecat_master_get_error_code - 获取主站故障码
 * @master: 主站对象指针
 *
 * Return: 当前故障码；@master 为 NULL 时返回 MO_ECAT_MASTER_ERROR_NONE
 */
enum mo_ecat_master_error mo_ecat_master_get_error_code(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_error error;

	if (!master) {
		return MO_ECAT_MASTER_ERROR_NONE;
	}

	error = atomic_load(&master->error_code);
	return error;
}

/**
 * mo_ecat_master_dispatch - 调度主站状态机
 * @master: 主站对象指针
 */
void mo_ecat_master_dispatch(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	sm_dispatch(&master->sm);
}

/**
 * mo_ecat_master_get_state - 获取主站当前状态
 * @master: 主站对象指针
 *
 * Return: 当前主站状态；@master 为 NULL 时返回 MO_ECAT_MASTER_STATE_INIT
 */
enum mo_ecat_master_state mo_ecat_master_get_state(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state;

	if (!master) {
		return MO_ECAT_MASTER_STATE_INIT;
	}

	state = atomic_load(&master->state);
	return state;
}

/**
 * mo_ecat_master_sync0_configure - 激活/关闭从站 DC Sync0 输出
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @enable: 非 0 激活，0 关闭
 * @cycle_time_ns: Sync0 周期（ns）
 * @shift_time_ns: Sync0 相位偏移（ns）
 *
 * 只在总线配置阶段调用；运行期重复调用会导致 Sync0 重建。
 *
 * Return: 0 成功；负 errno
 */
int mo_ecat_master_sync0_configure(struct mo_ecat_master *master, size_t slave_index, int enable,
				   uint32_t cycle_time_ns, int32_t shift_time_ns)
{
	enum backend_error error;

	if (!master) {
		return -1;
	}
	error = backend_sync0_configure(&master->backend, slave_index, enable, cycle_time_ns,
					shift_time_ns);
	return error;
}

/**
 * mo_ecat_master_sync0_status - 读回从站 Sync0 当前状态
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @status: 状态输出缓冲区
 *
 * Return: 0 成功；负 errno
 */
int mo_ecat_master_sync0_status(struct mo_ecat_master *master, size_t slave_index,
				struct mo_ecat_sync0_status *status)
{
	enum backend_error error;

	if (!master) {
		return -1;
	}
	error = backend_sync0_read_status(&master->backend, slave_index, status);
	return error;
}

/**
 * mo_ecat_master_request_slave_recovery - 请求恢复单个从站到 OP
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 *
 * Return: 0 已受理；非 0 拒绝
 */
int mo_ecat_master_request_slave_recovery(struct mo_ecat_master *master, size_t slave_index)
{
	int valid = 0;

	if (!master) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_RUNNING) {
		return -1;
	}

	pthread_mutex_lock(&master->slave_table_mutex);
	if (slave_index < master->slave_table.slave_count &&
	    master->slave_table.slaves[slave_index].state.is_online) {
		valid = 1;
	}
	pthread_mutex_unlock(&master->slave_table_mutex);
	if (!valid) {
		return -1;
	}

	atomic_store(&master->command_arg, (long)slave_index);
	master_write_cmd(master, MO_ECAT_MASTER_CMD_RECOVER_SLAVE);
	return 0;
}

/**
 * mo_ecat_master_request_enter_debug - 请求进入从站调试态
 * @master: 主站对象指针
 *
 * 仅 IDLE 状态受理（总线已扫描、空闲未激活）。
 *
 * Return: 0 已受理；非 0 拒绝
 */
int mo_ecat_master_request_enter_debug(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_IDLE) {
		return -1;
	}

	master_write_cmd(master, MO_ECAT_MASTER_CMD_ENTER_DEBUG);
	return 0;
}

/**
 * mo_ecat_master_request_exit_debug - 请求退出从站调试态
 * @master: 主站对象指针
 *
 * 退出后回到 IDLE 状态。
 *
 * Return: 0 已受理；非 0 拒绝
 */
int mo_ecat_master_request_exit_debug(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_DEBUG_SLAVE) {
		return -1;
	}

	master_write_cmd(master, MO_ECAT_MASTER_CMD_EXIT_DEBUG);
	return 0;
}

/**
 * mo_ecat_master_request_set_slave_al_state - 请求设置单个从站 AL 状态
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @target_state: 目标 AL 状态
 *
 * 仅 DEBUG_SLAVE 状态受理。参数编码：(slave_index<<16)|target_state。
 *
 * Return: 0 已受理；非 0 拒绝
 */
int mo_ecat_master_request_set_slave_al_state(struct mo_ecat_master *master, size_t slave_index,
					      enum mo_ecat_node_al_state target_state)
{
	if (!master) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_DEBUG_SLAVE) {
		return -1;
	}

	pthread_mutex_lock(&master->slave_table_mutex);
	const int valid = (slave_index < master->slave_table.slave_count);
	pthread_mutex_unlock(&master->slave_table_mutex);
	if (!valid) {
		return -1;
	}

	const long arg = ((long)slave_index << 16) | (long)target_state;
	atomic_store(&master->command_arg, arg);
	master_write_cmd(master, MO_ECAT_MASTER_CMD_SET_SLAVE_AL_STATE);
	return 0;
}

/**
 * mo_ecat_master_request_sdo_read - 通过 CoE SDO 读取从站对象字典
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @object_index: 对象字典索引
 * @object_subindex: 子索引
 * @data: 数据缓冲区
 * @data_size: 输入期望字节数，输出实际读取字节数
 *
 * 仅 DEBUG_SLAVE 状态受理。调用线程阻塞至 SDO 传输完成。
 *
 * Return: 0 成功；非 0 失败
 */
int mo_ecat_master_request_sdo_read(struct mo_ecat_master *master, size_t slave_index,
				    uint16_t object_index, uint8_t object_subindex, void *data,
				    size_t *data_size)
{
	if (!master || !data || !data_size || *data_size == 0U || *data_size > (size_t)INT_MAX) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_DEBUG_SLAVE) {
		return -1;
	}

	pthread_mutex_lock(&master->slave_table_mutex);
	const int valid = (slave_index < master->slave_table.slave_count);
	pthread_mutex_unlock(&master->slave_table_mutex);
	if (!valid) {
		return -1;
	}

	int sdo_size = (int)(*data_size);
	const enum backend_error error = backend_sdo_read(
		&master->backend, slave_index, object_index, object_subindex, &sdo_size, data);
	*data_size = (size_t)sdo_size;
	return (error == BACKEND_ERROR_NONE) ? 0 : -1;
}

/**
 * mo_ecat_master_request_sdo_write - 通过 CoE SDO 写入从站对象字典
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @object_index: 对象字典索引
 * @object_subindex: 子索引
 * @data: 数据缓冲区
 * @data_size: 写入字节数
 *
 * 仅 DEBUG_SLAVE 状态受理。调用线程阻塞至 SDO 传输完成。
 *
 * Return: 0 成功；非 0 失败
 */
int mo_ecat_master_request_sdo_write(struct mo_ecat_master *master, size_t slave_index,
				     uint16_t object_index, uint8_t object_subindex,
				     const void *data, size_t data_size)
{
	if (!master || !data || data_size == 0U || data_size > (size_t)INT_MAX) {
		return -1;
	}
	if (mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_DEBUG_SLAVE) {
		return -1;
	}

	pthread_mutex_lock(&master->slave_table_mutex);
	const int valid = (slave_index < master->slave_table.slave_count);
	pthread_mutex_unlock(&master->slave_table_mutex);
	if (!valid) {
		return -1;
	}

	const enum backend_error error = backend_sdo_write(
		&master->backend, slave_index, object_index, object_subindex, (int)data_size, data);
	return (error == BACKEND_ERROR_NONE) ? 0 : -1;
}
