/**
 * @file master_discovery.c
 * @brief 主站扫描资源与后端打开管理
 */

#include <stdlib.h>
#include <string.h>

#include "master_priv.h"

static int allocate_discovery_memory(struct mo_ecat_master *master, size_t slave_count)
{
	if (slave_count > SIZE_MAX / sizeof(*master->slave_table.slaves)) {
		return -1;
	}

	if (slave_count == 0) {
		return 0;
	}

	master->slave_table.slaves = calloc(slave_count, sizeof(*master->slave_table.slaves));
	if (!master->slave_table.slaves) {
		return -1;
	}

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

	backend_close(&master->backend);

	free(master->process.pdo_refs.refs);
	free(master->slave_table.slaves);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->process.image, 0, sizeof(master->process.image));
	master->slave_table.slaves = NULL;
	master->slave_table.count = 0;
	master->process.pdo_refs.refs = NULL;
	master->process.pdo_refs.count = 0;
}

int master_backend_open(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	return backend_open(&master->backend, master->config);
}

int master_scan(struct mo_ecat_master *master, size_t *slave_count)
{
	if (!master || !slave_count) {
		return -1;
	}

	return backend_scan(&master->backend, slave_count);
}

int master_build_topology(struct mo_ecat_master *master, size_t slave_count)
{
	if (!master) {
		return -1;
	}

	if (slave_count > 0 && allocate_discovery_memory(master, slave_count) < 0) {
		return -1;
	}

	master->slave_table.count = slave_count;
	if (slave_count > 0 && backend_read_discovered_slaves(&master->backend,
							    master->slave_table.slaves,
							    slave_count) < 0) {
		return -1;
	}
	if (backend_read_slave_states(&master->backend, master->slave_table.slaves,
				      master->slave_table.count) < 0) {
		return -1;
	}
	return 0;
}

int master_read_pdo_entries(struct mo_ecat_master *master)
{
	if (!master || (master->slave_table.count > 0 && !master->slave_table.slaves)) {
		return -1;
	}

	return backend_read_pdo_entries(&master->backend,
					master->slave_table.slaves,
					master->slave_table.count);
}

static size_t master_count_pdo_refs(const struct mo_ecat_master *master)
{
	size_t count = 0;

	for (size_t i = 0; i < master->slave_table.count; ++i) {
		count += master->slave_table.slaves[i].pdo_entry_count;
	}

	return count;
}

static void master_build_pdo_refs(const struct mo_ecat_master *master,
				  struct mo_ecat_slave_pdo_ref *refs)
{
	size_t idx = 0;

	for (size_t i = 0; i < master->slave_table.count; ++i) {
		const struct mo_ecat_slave *slave = &master->slave_table.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct mo_ecat_slave_pdo_entry *entry = &slave->pdo_entries[j];
			struct mo_ecat_slave_pdo_ref *ref = &refs[idx++];

			ref->slave_index = i;
			ref->object_index = entry->object_index;
			ref->object_subindex = entry->object_subindex;
			ref->bit_length = entry->bit_length;
			ref->direction = entry->direction;
		}
	}
}

int master_configure(struct mo_ecat_master *master)
{
	size_t pdo_ref_count;
	struct mo_ecat_slave_pdo_ref *refs = NULL;

	if (!master) {
		return -1;
	}

	pdo_ref_count = master_count_pdo_refs(master);
	if (pdo_ref_count > 0) {
		refs = calloc(pdo_ref_count, sizeof(*refs));
		if (!refs) {
			return -1;
		}
		master_build_pdo_refs(master, refs);
	}

	if (backend_configure(&master->backend) < 0) {
		free(refs);
		return -1;
	}

	if (backend_get_process_image(&master->backend, &master->process.image) < 0) {
		free(refs);
		return -1;
	}
	master->process.image.generation++;

	if (pdo_ref_count > 0) {
		if (backend_fill_pdo_refs(&master->backend, refs, pdo_ref_count,
					  master->process.image.generation) < 0) {
			free(refs);
			return -1;
		}
	}

	backend_fill_slave_info(&master->backend, master->slave_table.slaves,
				master->slave_table.count);

	free(master->process.pdo_refs.refs);
	master->process.pdo_refs.refs = refs;
	master->process.pdo_refs.count = pdo_ref_count;
	return 0;
}

int master_activate(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	if (backend_activate(&master->backend) < 0) {
		return -1;
	}

	master->process.image.active = 1;
	return 0;
}

int master_deactivate(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	if (backend_deactivate(&master->backend) < 0) {
		return -1;
	}

	master->process.image.active = 0;
	return 0;
}
