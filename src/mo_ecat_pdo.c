/**
 * @file mo_ecat_pdo.c
 * @brief 周期过程数据与 PDO 访问
 */

#include <string.h>

#include "mo_ecat/mo_ecat_pdo.h"
#include "master_priv.h"

static int pdo_ref_in_bounds(const struct mo_ecat_process_image *image,
			     const struct mo_ecat_pdo_ref *reference)
{
	size_t image_bits;
	size_t start_bit;
	size_t end_bit;

	if (!image || !reference || !image->memory || reference->bit_length == 0 ||
	    reference->byte_offset >= image->size) {
		return 0;
	}

	image_bits = image->size * 8U;
	start_bit = (size_t)reference->byte_offset * 8U + reference->bit_offset;
	end_bit = start_bit + reference->bit_length;
	return end_bit >= start_bit && end_bit <= image_bits;
}

int mo_ecat_master_cycle_begin(struct mo_ecat_master *master, struct mo_ecat_cycle_result *result)
{
	int backend_result;

	if (!master || !result) {
		return -1;
	}

	pthread_mutex_lock(&master->lock);
	if (!master->image.active || !master->backend.ops || !master->backend.ops->cycle_begin) {
		pthread_mutex_unlock(&master->lock);
		return -1;
	}

	memset(result, 0, sizeof(*result));
	backend_result = master->backend.ops->cycle_begin(&master->backend, result);
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
	if (!master->image.active || !master->backend.ops || !master->backend.ops->cycle_end) {
		pthread_mutex_unlock(&master->lock);
		return -1;
	}

	backend_result = master->backend.ops->cycle_end(&master->backend, result);
	if (backend_result == 0) {
		master->cycle.last = *result;
	}
	pthread_mutex_unlock(&master->lock);
	return backend_result;
}

size_t mo_ecat_master_get_pdo_ref_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	count = master->pdo.count;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return count;
}

const struct mo_ecat_pdo_ref *mo_ecat_master_get_pdo_ref(const struct mo_ecat_master *master,
							 size_t index)
{
	const struct mo_ecat_pdo_ref *reference;

	if (!master) {
		return NULL;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (index >= master->pdo.count) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return NULL;
	}
	reference = &master->pdo.refs[index];
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return reference;
}

int mo_ecat_master_get_cycle_result(const struct mo_ecat_master *master,
				    struct mo_ecat_cycle_result *result)
{
	if (!master || !result) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	*result = master->cycle.last;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

int mo_ecat_master_get_process_image(const struct mo_ecat_master *master, const uint8_t **memory,
				     size_t *size)
{
	if (!master || !memory || !size) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (!master->image.memory || master->image.size == 0) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return -1;
	}

	*memory = master->image.memory;
	*size = master->image.size;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
			      const struct mo_ecat_pdo_ref *reference)
{
	const void *data;

	if (!master || !reference) {
		return NULL;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (reference->direction != MO_ECAT_PDO_INPUT ||
	    reference->generation != master->image.generation ||
	    !pdo_ref_in_bounds(&master->image, reference)) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return NULL;
	}

	data = &master->image.memory[reference->byte_offset];
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return data;
}

void *mo_ecat_pdo_output(struct mo_ecat_master *master, const struct mo_ecat_pdo_ref *reference)
{
	void *data;

	if (!master || !reference) {
		return NULL;
	}

	pthread_mutex_lock(&master->lock);
	if (reference->direction != MO_ECAT_PDO_OUTPUT ||
	    reference->generation != master->image.generation ||
	    !pdo_ref_in_bounds(&master->image, reference)) {
		pthread_mutex_unlock(&master->lock);
		return NULL;
	}

	data = &master->image.memory[reference->byte_offset];
	pthread_mutex_unlock(&master->lock);
	return data;
}
