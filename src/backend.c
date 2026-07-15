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

int backend_load_slave_info(struct backend_instance *backend, size_t *slave_count)
{
	if (!backend || !slave_count || !backend->ops || !backend->ops->load_slave_info) {
		return -1;
	}

	return backend->ops->load_slave_info(backend, slave_count);
}

int backend_translate_slave_info(struct backend_instance *backend,
				   struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->translate_slave_info) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->translation_ops->translate_slave_info(backend, slaves, slave_count);
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

int backend_configure_dc(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->configure_dc) {
		return -1;
	}

	return backend->ops->configure_dc(backend);
}

/**
 * 调用后端建立 PDO 映射。
 *
 * 后端会根据 entries 中描述的 PDO entry（从站索引、对象索引、位长度、
 * 方向等）建立 IOmap/domain，并回填每个 entry 在 PDO 数据区域中的
 * byte_offset 和 bit_offset。
 *
 * @param backend     后端实例
 * @param entries     PDO entry 映射数组，由调用者分配
 * @param entry_count entries 数组元素个数，允许为 0
 * @return 0 成功，非 0 失败
 */
int backend_build_pdo_mapping(struct backend_instance *backend,
			      struct master_pdo_entry_mapping *entries,
			      size_t entry_count)
{
	if (!backend || !backend->ops || !backend->ops->build_pdo_mapping) {
		return -1;
	}

	if (entry_count > 0 && !entries) {
		return -1;
	}

	return backend->ops->build_pdo_mapping(backend, entries, entry_count);
}

int backend_get_pdo_image(struct backend_instance *backend,
			  struct master_pdo_image *image)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->get_pdo_image) {
		return -1;
	}

	if (!image) {
		return -1;
	}

	return backend->translation_ops->get_pdo_image(backend, image);
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

int backend_read_all_slave_states(struct backend_instance *backend,
				  struct mo_ecat_slave *slaves, size_t slave_count)
{
	if (!backend || !backend->ops || !backend->ops->read_all_slave_states) {
		return -1;
	}

	if (slave_count > 0 && !slaves) {
		return -1;
	}

	return backend->ops->read_all_slave_states(backend, slaves, slave_count);
}

int backend_read_single_slave_state(struct backend_instance *backend,
				    size_t slave_index,
				    struct mo_ecat_node_state *state)
{
	if (!backend || !backend->ops || !backend->ops->read_single_slave_state || !state) {
		return -1;
	}

	return backend->ops->read_single_slave_state(backend, slave_index, state);
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
