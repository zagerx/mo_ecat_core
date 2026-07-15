/**
 * @file master_topology.c
 * @brief 主站内部拓扑构建与状态刷新
 */

#include <stdlib.h>

#include "master_priv.h"
#include "master_topology.h"

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

int master_topology_refresh_states(struct mo_ecat_master *master)
{
	if (!master || (master->topology.slave_count > 0 && !master->topology.slaves)) {
		return -1;
	}

	return backend_read_all_slave_states(&master->backend, master->topology.slaves,
					     master->topology.slave_count);
}
