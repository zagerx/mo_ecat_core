/*
 * master_topology.c - 主站内部拓扑构建与状态刷新
 *
 * 负责从后端读取从站数量、分配从站表内存，并在核心层与后端之间同步
 * 从站信息。
 */

#include <stdlib.h>

#include "master_priv.h"
#include "master_topology.h"

/**
 * master_topology_build - 构建主站从站拓扑
 * @master: 主站对象指针
 *
 * 从后端获取从站数量，分配并填充从站表。
 *
 * Return: 0 成功，非 0 失败
 */
int master_topology_build(struct mo_ecat_master *master)
{
	size_t slave_count;

	if (!master) {
		return -1;
	}

	if (backend_get_slave_count(&master->backend, &slave_count) < 0) {
		return -1;
	}

	if (slave_count > 0) {
		if (slave_count > SIZE_MAX / sizeof(*master->topology.slaves)) {
			return -1;
		}
		master->topology.slaves = calloc(slave_count, sizeof(*master->topology.slaves));
		if (!master->topology.slaves) {
			return -1;
		}
	}

	master->topology.slave_count = slave_count;
	if (slave_count > 0 &&
	    backend_translate_slave_info(&master->backend, master->topology.slaves,
					 slave_count) < 0) {
		return -1;
	}

	return 0;
}

/**
 * master_topology_refresh_states - 刷新所有从站运行状态
 * @master: 主站对象指针
 *
 * 调用后端读取所有从站的 AL 状态、错误标志等运行时信息。
 *
 * Return: 0 成功，非 0 失败
 */
int master_topology_refresh_states(struct mo_ecat_master *master)
{
	if (!master || (master->topology.slave_count > 0 && !master->topology.slaves)) {
		return -1;
	}

	return backend_read_all_slave_states(&master->backend, master->topology.slaves,
					     master->topology.slave_count);
}
