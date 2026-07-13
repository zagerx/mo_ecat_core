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

	master->diag.memory = calloc(1, total_size);
	if (!master->diag.memory) {
		return -1;
	}
	master->diag.size = total_size;
	master->diag.slaves = aligned_region(master->diag.memory, &offset,
					     _Alignof(struct mo_ecat_slave), slave_size);
	master->diag.states = aligned_region(master->diag.memory, &offset,
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

	free(master->process.pdo_refs.refs);
	free(master->diag.memory);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->process.image, 0, sizeof(master->process.image));
	master->diag.slaves = NULL;
	master->diag.states = NULL;
	master->diag.count = 0;
	master->diag.memory = NULL;
	master->diag.size = 0;
	master->process.pdo_refs.refs = NULL;
	master->process.pdo_refs.count = 0;
}

int master_backend_open(struct mo_ecat_master *master)
{
	if (!master || !master->backend.ops || !master->backend.ops->open) {
		return -1;
	}

	return master->backend.ops->open(&master->backend, master->config);
}

int master_scan(struct mo_ecat_master *master, size_t *slave_count)
{
	if (!master || !slave_count || !master->backend.ops || !master->backend.ops->scan) {
		return -1;
	}

	return master->backend.ops->scan(&master->backend, slave_count);
}

int master_build_topology(struct mo_ecat_master *master, size_t slave_count)
{
	if (!master || !master->backend.ops) {
		return -1;
	}

	if (slave_count > 0 && allocate_discovery_memory(master, slave_count) < 0) {
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

int master_read_pdo_entries(struct mo_ecat_master *master)
{
	if (!master || !master->backend.ops || !master->backend.ops->read_pdo_entries ||
	    (master->diag.count > 0 && !master->diag.slaves)) {
		return -1;
	}

	return master->backend.ops->read_pdo_entries(&master->backend,
						     master->diag.slaves, master->diag.count);
}

static size_t master_count_pdo_refs(const struct mo_ecat_master *master)
{
	size_t count = 0;

	for (size_t i = 0; i < master->diag.count; ++i) {
		count += master->diag.slaves[i].pdo_entry_count;
	}

	return count;
}

static void master_build_pdo_refs(const struct mo_ecat_master *master,
				  struct mo_ecat_pdo_ref *refs)
{
	size_t idx = 0;

	for (size_t i = 0; i < master->diag.count; ++i) {
		const struct mo_ecat_slave *slave = &master->diag.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct mo_ecat_pdo_entry_info *entry = &slave->pdo_entries[j];
			struct mo_ecat_pdo_ref *ref = &refs[idx++];

			ref->slave_index = i;
			ref->index = entry->index;
			ref->subindex = entry->subindex;
			ref->bit_length = entry->bit_length;
			ref->direction = entry->direction;
		}
	}
}

int master_configure(struct mo_ecat_master *master)
{
	size_t pdo_ref_count;
	struct mo_ecat_pdo_ref *refs = NULL;

	if (!master || !master->backend.ops || !master->backend.ops->configure) {
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

	if (master->backend.ops->configure(&master->backend) < 0) {
		free(refs);
		return -1;
	}

	if (!master->backend.ops->get_process_image ||
	    master->backend.ops->get_process_image(&master->backend,
						   &master->process.image) < 0) {
		free(refs);
		return -1;
	}
	master->process.image.generation++;

	if (pdo_ref_count > 0) {
		if (!master->backend.ops->fill_pdo_refs ||
		    master->backend.ops->fill_pdo_refs(&master->backend, refs, pdo_ref_count,
						       master->process.image.generation) < 0) {
			free(refs);
			return -1;
		}
	}

	if (master->backend.ops->fill_slave_info) {
		master->backend.ops->fill_slave_info(&master->backend, master->diag.slaves,
						     master->diag.count);
	}

	free(master->process.pdo_refs.refs);
	master->process.pdo_refs.refs = refs;
	master->process.pdo_refs.count = pdo_ref_count;
	return 0;
}

int master_activate(struct mo_ecat_master *master)
{
	if (!master || !master->backend.ops || !master->backend.ops->activate) {
		return -1;
	}

	if (master->backend.ops->activate(&master->backend) < 0) {
		return -1;
	}

	master->process.image.active = 1;
	return 0;
}

int master_deactivate(struct mo_ecat_master *master)
{
	if (!master || !master->backend.ops || !master->backend.ops->deactivate) {
		return -1;
	}

	if (master->backend.ops->deactivate(&master->backend) < 0) {
		return -1;
	}

	master->process.image.active = 0;
	return 0;
}
