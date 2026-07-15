/**
 * @file cyclic.c
 * @brief 周期数据访问
 */

#include <string.h>

#include "mo_ecat/mo_ecat_cyclic.h"
#include "master_priv.h"

static int pdo_entry_mapping_in_bounds(const struct master_pdo_image *image,
				       const struct master_pdo_entry_mapping *mapping)
{
	size_t image_bits;
	size_t start_bit;
	size_t end_bit;

	if (!image || !mapping || !image->memory || mapping->entry.bit_length == 0 ||
	    mapping->byte_offset >= image->size) {
		return 0;
	}

	image_bits = image->size * 8U;
	start_bit = (size_t)mapping->byte_offset * 8U + mapping->bit_offset;
	end_bit = start_bit + mapping->entry.bit_length;
	return end_bit >= start_bit && end_bit <= image_bits;
}

int master_cycle_begin(struct mo_ecat_master *master, struct mo_ecat_cycle_result *result)
{
	int backend_result;

	if (!master || !result) {
		return -1;
	}

	if (!master->pdo_mapping.active) {
		return -1;
	}

	memset(result, 0, sizeof(*result));
	backend_result = backend_cycle_begin(&master->backend, result);
	return backend_result;
}

int master_cycle_end(struct mo_ecat_master *master, struct mo_ecat_cycle_result *result)
{
	int backend_result;

	if (!master || !result) {
		return -1;
	}

	if (!master->pdo_mapping.active) {
		return -1;
	}

	backend_result = backend_cycle_end(&master->backend, result);
	return backend_result;
}

size_t mo_ecat_master_get_cyclic_entry_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	count = master->pdo_mapping.entry_count;
	return count;
}

int mo_ecat_master_get_cyclic_entry(
	const struct mo_ecat_master *master,
	size_t index,
	struct mo_ecat_cyclic_entry *entry)
{
	if (!master || !entry) {
		return -1;
	}

	if (index >= master->pdo_mapping.entry_count) {
		return -1;
	}

	*entry = master->pdo_mapping.entries[index].entry;
	return 0;
}

static const struct master_pdo_entry_mapping *master_find_pdo_entry_mapping(
    const struct mo_ecat_master *master, const struct mo_ecat_cyclic_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;

	if (!master || !entry) {
		return NULL;
	}

	if ((size_t)entry->id >= master->pdo_mapping.entry_count) {
		return NULL;
	}

	mapping = &master->pdo_mapping.entries[entry->id];
	if (mapping->entry.id != entry->id ||
	    mapping->entry.node_index != entry->node_index ||
	    mapping->entry.object_index != entry->object_index ||
	    mapping->entry.object_subindex != entry->object_subindex ||
	    mapping->entry.bit_length != entry->bit_length ||
	    mapping->entry.direction != entry->direction) {
		return NULL;
	}

	return mapping;
}

const void *mo_ecat_cyclic_input(const struct mo_ecat_master *master,
				 const struct mo_ecat_cyclic_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;
	const void *data;

	if (!master || !entry || entry->direction != MO_ECAT_CYCLIC_INPUT) {
		return NULL;
	}
	mapping = master_find_pdo_entry_mapping(master, entry);

	if (!mapping || mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	return data;
}

void *mo_ecat_cyclic_output(struct mo_ecat_master *master,
			    const struct mo_ecat_cyclic_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;
	void *data;

	if (!master || !entry || entry->direction != MO_ECAT_CYCLIC_OUTPUT) {
		return NULL;
	}
	mapping = master_find_pdo_entry_mapping(master, entry);

	if (!mapping || mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	return data;
}
