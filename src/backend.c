/**
 * @file backend.c
 * @brief 后端适配层统一入口
 *
 * 核心层通过这里的 wrapper 函数调用后端能力，而不是直接访问 ops 表。
 * 这样可以把空指针检查、参数校验集中在一处，也便于以后替换后端实现。
 */

#include "backend.h"
#include "backend/backend_ops.h"

int backend_open(struct backend_instance *backend,
		 const struct mo_ecat_master_config *config)
{
	if (!backend || !backend->ops || !backend->ops->open) {
		return -1;
	}

	return backend->ops->open(backend, config);
}

int backend_scan(struct backend_instance *backend, size_t *slave_count)
{
	if (!backend || !slave_count || !backend->ops || !backend->ops->scan) {
		return -1;
	}

	return backend->ops->scan(backend, slave_count);
}

int backend_read_discovered_slaves(struct backend_instance *backend,
				   struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->read_discovered_slaves) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->translation_ops->read_discovered_slaves(backend, slaves, slave_count);
}

int backend_read_pdo_entries(struct backend_instance *backend,
			     struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->read_pdo_entries) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->translation_ops->read_pdo_entries(backend, slaves, slave_count);
}

int backend_configure(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->configure) {
		return -1;
	}

	return backend->ops->configure(backend);
}

int backend_get_process_image(struct backend_instance *backend,
			      struct mo_ecat_process_image *image)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->get_process_image) {
		return -1;
	}

	if (!image) {
		return -1;
	}

	return backend->translation_ops->get_process_image(backend, image);
}

int backend_fill_pdo_refs(struct backend_instance *backend,
			  struct mo_ecat_slave_pdo_ref *refs,
			  size_t ref_count, uint32_t generation)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->fill_pdo_refs) {
		return -1;
	}

	if (ref_count > 0 && !refs) {
		return -1;
	}

	return backend->translation_ops->fill_pdo_refs(backend, refs, ref_count, generation);
}

int backend_fill_slave_info(struct backend_instance *backend,
			    struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->fill_slave_info) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->translation_ops->fill_slave_info(backend, slaves, slave_count);
}

int backend_activate(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->activate) {
		return -1;
	}

	return backend->ops->activate(backend);
}

int backend_cycle_begin(struct backend_instance *backend,
			struct mo_ecat_cycle_result *result)
{
	if (!backend || !backend->ops || !backend->ops->cycle_begin) {
		return -1;
	}

	if (!result) {
		return -1;
	}

	return backend->ops->cycle_begin(backend, result);
}

int backend_cycle_end(struct backend_instance *backend,
		      struct mo_ecat_cycle_result *result)
{
	if (!backend || !backend->ops || !backend->ops->cycle_end) {
		return -1;
	}

	if (!result) {
		return -1;
	}

	return backend->ops->cycle_end(backend, result);
}

int backend_read_slave_states(struct backend_instance *backend,
			      struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->ops || !backend->ops->read_slave_states) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->ops->read_slave_states(backend, slaves, slave_count);
}

int backend_deactivate(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->deactivate) {
		return -1;
	}

	return backend->ops->deactivate(backend);
}

void backend_close(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->close) {
		return;
	}

	backend->ops->close(backend);
}
