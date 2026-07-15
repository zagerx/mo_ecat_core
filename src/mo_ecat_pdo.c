/**
 * @file mo_ecat_pdo.c
 * @brief 周期 PDO 数据访问
 */

#include <string.h>

#include "mo_ecat/mo_ecat_pdo.h"
#include "master_priv.h"

static int pdo_entry_mapping_in_bounds(const struct master_pdo_image *image,
				       const struct mo_ecat_pdo_entry_mapping *mapping)
{
	size_t image_bits;
	size_t start_bit;
	size_t end_bit;

	if (!image || !mapping || !image->memory || mapping->bit_length == 0 ||
	    mapping->byte_offset >= image->size) {
		return 0;
	}

	image_bits = image->size * 8U;
	start_bit = (size_t)mapping->byte_offset * 8U + mapping->bit_offset;
	end_bit = start_bit + mapping->bit_length;
	return end_bit >= start_bit && end_bit <= image_bits;
}

int mo_ecat_master_cycle_begin(struct mo_ecat_master *master, struct mo_ecat_cycle_result *result)
{
	int backend_result;

	if (!master || !result) {
		return -1;
	}

	pthread_mutex_lock(&master->lock);
	if (!master->pdo_mapping.active) {
		pthread_mutex_unlock(&master->lock);
		return -1;
	}

	memset(result, 0, sizeof(*result));
	backend_result = backend_cycle_begin(&master->backend, result);
	pthread_mutex_unlock(&master->lock);
	return backend_result;
}

int mo_ecat_master_cycle_end(struct mo_ecat_master *master, struct mo_ecat_cycle_result *result)
{
	int backend_result;

	if (!master || !result) {
		return -1;
	}

	pthread_mutex_lock(&master->lock);
	if (!master->pdo_mapping.active) {
		pthread_mutex_unlock(&master->lock);
		return -1;
	}

	backend_result = backend_cycle_end(&master->backend, result);
	if (backend_result == 0) {
		master->cycle_result.last = *result;
	}
	pthread_mutex_unlock(&master->lock);
	return backend_result;
}

size_t mo_ecat_master_get_pdo_entry_mapping_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	count = master->pdo_mapping.entry_count;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return count;
}

int mo_ecat_master_get_pdo_entry_mapping(
	const struct mo_ecat_master *master,
	size_t index,
	struct mo_ecat_pdo_entry_mapping *mapping)
{
	if (!master || !mapping) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (index >= master->pdo_mapping.entry_count) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return -1;
	}

	*mapping = master->pdo_mapping.entries[index];
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

int mo_ecat_master_get_pdo_image_view(const struct mo_ecat_master *master,
				      struct mo_ecat_pdo_image_view *view)
{
	enum mo_ecat_master_state state;

	if (!master || !view) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	state = master_state_from_sm(master);
	if (state != MO_ECAT_MASTER_STATE_READY &&
	    state != MO_ECAT_MASTER_STATE_RUNNING) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return -1;
	}

	if (!master->pdo_mapping.image.memory || master->pdo_mapping.image.size == 0) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return -1;
	}

	view->memory = master->pdo_mapping.image.memory;
	view->size = master->pdo_mapping.image.size;
	view->generation = master->pdo_mapping.generation;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

int mo_ecat_master_get_cycle_result(const struct mo_ecat_master *master,
				    struct mo_ecat_cycle_result *result)
{
	if (!master || !result) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	*result = master->cycle_result.last;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
			      const struct mo_ecat_pdo_entry_mapping *mapping)
{
	const void *data;

	if (!master || !mapping) {
		return NULL;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (mapping->direction != MO_ECAT_PDO_INPUT ||
	    mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return data;
}

void *mo_ecat_pdo_output(struct mo_ecat_master *master,
			 const struct mo_ecat_pdo_entry_mapping *mapping)
{
	void *data;

	if (!master || !mapping) {
		return NULL;
	}

	pthread_mutex_lock(&master->lock);
	if (mapping->direction != MO_ECAT_PDO_OUTPUT ||
	    mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		pthread_mutex_unlock(&master->lock);
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	pthread_mutex_unlock(&master->lock);
	return data;
}
