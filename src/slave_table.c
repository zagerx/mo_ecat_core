/*
 * slave_table.c - 主站内部从站表构建与状态刷新
 *
 * 负责从后端读取从站数量、分配从站表内存，并在核心层与后端之间同步
 * 从站信息。
 */

#include <stdlib.h>

#include "master_priv.h"
#include "slave_table.h"

/**
 * slave_table_build - 构建主站从站表
 * @master: 主站对象指针
 * @slave_count: 扫描阶段得到的从站数量
 *
 * 根据扫描阶段得到的从站数量，分配并填充从站表。
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail slave_table_build(struct mo_ecat_master *master,
					   size_t slave_count)
{
	struct slave *slaves = NULL;
	enum backend_error error;

	if (!master) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}

	if (slave_count > 0) {
		if (slave_count > SIZE_MAX / sizeof(*slaves)) {
			return MASTER_ERROR_NO_MEMORY;
		}
		slaves = calloc(slave_count, sizeof(*slaves));
		if (!slaves) {
			return MASTER_ERROR_NO_MEMORY;
		}
	}

	if (slave_count > 0) {
		error = backend_translate_slave_info(&master->backend, slaves, slave_count);
		if (error != BACKEND_ERROR_NONE) {
			free(slaves);
			return master_error_from_backend(error);
		}
	}

	/* 先完整构建临时表，再一次性发布，避免读线程观察到半初始化从站表。 */
	pthread_mutex_lock(&master->slave_table_mutex);
	free(master->slave_table.slaves);
	master->slave_table.slaves = slaves;
	master->slave_table.slave_count = slave_count;
	pthread_mutex_unlock(&master->slave_table_mutex);

	return MASTER_ERROR_NONE;
}

/**
 * slave_table_refresh_states - 刷新所有从站运行状态
 * @master: 主站对象指针
 *
 * 调用后端读取所有从站的 AL 状态、错误标志等运行时信息。
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail slave_table_refresh_states(struct mo_ecat_master *master)
{
	enum backend_error error;

	if (!master) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}

	pthread_mutex_lock(&master->slave_table_mutex);
	if (master->slave_table.slave_count > 0 && !master->slave_table.slaves) {
		pthread_mutex_unlock(&master->slave_table_mutex);
		return MASTER_ERROR_INVALID_ARGUMENT;
	}
	error = backend_read_all_slave_states(&master->backend, master->slave_table.slaves,
					      master->slave_table.slave_count);
	pthread_mutex_unlock(&master->slave_table_mutex);
	return master_error_from_backend(error);
}
