/*
 * mo_ecat_master.c - 主站对象、生命周期与命令接口
 *
 * 提供主站对象的创建/销毁、命令写入、状态查询以及状态机调度等公开接口。
 */

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
static int master_init(struct mo_ecat_master *master,
		       mo_ecat_cyclic_callback callback,
		       void *user_data)
{
	if (!master) {
		return -1;
	}

	memset(master, 0, sizeof(*master));
	master->cyclic_callback = callback;
	master->user_data = user_data;
	atomic_init(&master->command, MO_ECAT_MASTER_CMD_NONE);
	atomic_init(&master->state, MO_ECAT_MASTER_STATE_INIT);
	atomic_init(&master->error_code, MO_ECAT_MASTER_ERROR_NONE);
	master->last_error.master_error = MO_ECAT_MASTER_ERROR_NONE;
	master->last_error.detail = MASTER_ERROR_NONE;
	master->last_error.source = MASTER_ERROR_SOURCE_CORE;
	master->last_error.node_index = SIZE_MAX;
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
struct mo_ecat_master *mo_ecat_master_create(mo_ecat_cyclic_callback callback,
					     void *user_data)
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
	if (!master || cmd <= MO_ECAT_MASTER_CMD_NONE || cmd > MO_ECAT_MASTER_CMD_RESET) {
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
