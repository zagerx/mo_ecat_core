/**
 * @file master_discovery.c
 * @brief 主站扫描资源与后端打开管理
 */

#include <stdlib.h>
#include <string.h>

#include "master_priv.h"

static int add_aligned_size(size_t *total, size_t alignment, size_t size)
{
	size_t padding;

	if (!total || alignment == 0) {
		return -1;
	}

	padding = (alignment - (*total % alignment)) % alignment;
	if (*total > SIZE_MAX - padding || *total + padding > SIZE_MAX - size) {
		return -1;
	}

	*total += padding + size;
	return 0;
}

static void *aligned_region(void *memory, size_t *offset, size_t alignment, size_t size)
{
	size_t padding = (alignment - (*offset % alignment)) % alignment;
	void *region = (unsigned char *)memory + *offset + padding;

	*offset += padding + size;
	return region;
}

static int allocate_discovery_memory(struct mo_ecat_master *master, size_t slave_count)
{
	size_t slave_size;
	size_t state_size;
	size_t total_size = 0;
	size_t offset = 0;

	if (slave_count > SIZE_MAX / sizeof(*master->diag.slaves) ||
	    slave_count > SIZE_MAX / sizeof(*master->diag.states)) {
		return -1;
	}

	slave_size = slave_count * sizeof(*master->diag.slaves);
	state_size = slave_count * sizeof(*master->diag.states);
	if (add_aligned_size(&total_size, _Alignof(struct mo_ecat_slave), slave_size) < 0 ||
	    add_aligned_size(&total_size, _Alignof(struct mo_ecat_slave_state), state_size) < 0) {
		return -1;
	}

	if (total_size == 0) {
		return 0;
	}

	master->runtime_memory.memory = calloc(1, total_size);
	if (!master->runtime_memory.memory) {
		return -1;
	}
	master->runtime_memory.size = total_size;
	master->diag.slaves = aligned_region(master->runtime_memory.memory, &offset,
					     _Alignof(struct mo_ecat_slave), slave_size);
	master->diag.states = aligned_region(master->runtime_memory.memory, &offset,
					     _Alignof(struct mo_ecat_slave_state), state_size);
	return 0;
}

void master_clear_cmd(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	master->command = MO_ECAT_MASTER_CMD_NONE;
}

void master_release_resources(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	if (master->backend.ops && master->backend.ops->close) {
		master->backend.ops->close(&master->backend);
	}

	free(master->runtime_memory.memory);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->image, 0, sizeof(master->image));
	master->diag.slaves = NULL;
	master->diag.states = NULL;
	master->diag.count = 0;
	master->pdo.refs = NULL;
	master->pdo.count = 0;
	master->runtime_memory.memory = NULL;
	master->runtime_memory.size = 0;
}

int master_backend_open(struct mo_ecat_master *master)
{
	if (!master || !master->backend.ops || !master->backend.ops->open) {
		return -1;
	}

	return master->backend.ops->open(&master->backend, &master->options);
}

int master_scan(struct mo_ecat_master *master, size_t *slave_count)
{
	if (!master || !slave_count || !master->backend.ops ||
	    !master->backend.ops->scan) {
		return -1;
	}

	return master->backend.ops->scan(&master->backend, slave_count);
}

int master_build_topology(struct mo_ecat_master *master, size_t slave_count)
{
	if (!master || allocate_discovery_memory(master, slave_count) < 0) {
		return -1;
	}

	master->diag.count = slave_count;
	if (slave_count > 0 && (!master->backend.ops->read_discovered_slaves ||
				master->backend.ops->read_discovered_slaves(
					&master->backend, master->diag.slaves, slave_count) < 0)) {
		return -1;
	}
	if (master->backend.ops->read_diagnostics) {
		if (master->backend.ops->read_diagnostics(&master->backend, master->diag.states,
							  master->diag.count) < 0) {
			return -1;
		}
		for (size_t i = 0; i < master->diag.count; ++i) {
			master->diag.slaves[i].state = master->diag.states[i];
		}
	}
	return 0;
}
