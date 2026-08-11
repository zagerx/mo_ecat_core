/*
 * backend.c - 后端适配层统一入口
 *
 * 核心层通过这里的 wrapper 函数调用后端能力，而不是直接访问 ops 表。
 * 这样可以把空指针检查、参数校验集中在一处，也便于以后替换后端实现。
 */

#include "backend.h"
#include "backend_ops.h"

/**
 * backend_open - 打开后端并连接 EtherCAT 总线
 * @backend: 后端实例指针
 * @config: 主站配置指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_open(struct backend_instance *backend,
				const struct mo_ecat_master_config *config)
{
	if (!backend || !backend->ops || !backend->ops->open) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (!config) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->open(backend, config);
}

/**
 * backend_load_slave_info - 加载从站信息
 * @backend: 后端实例指针
 * @slave_count: 从站数量输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_load_slave_info(struct backend_instance *backend, size_t *slave_count)
{
	if (!backend || !slave_count || !backend->ops || !backend->ops->load_slave_info) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->load_slave_info(backend, slave_count);
}

/**
 * backend_translate_slave_info - 转换从站信息到核心层结构
 * @backend: 后端实例指针
 * @slaves: 从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_translate_slave_info(struct backend_instance *backend,
						struct slave *slaves, size_t slave_count)
{
	if (!backend || !backend->translation_ops ||
	    !backend->translation_ops->translate_slave_info) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (slave_count > 0 && !slaves) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->translation_ops->translate_slave_info(backend, slaves, slave_count);
}

/**
 * backend_read_pdo_entries - 读取从站 PDO entry 描述
 * @backend: 后端实例指针
 * @slaves: 从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_read_pdo_entries(struct backend_instance *backend, struct slave *slaves,
					    size_t slave_count)
{
	if (!backend || !backend->translation_ops || !backend->translation_ops->read_pdo_entries) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (slave_count > 0 && !slaves) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->translation_ops->read_pdo_entries(backend, slaves, slave_count);
}

/**
 * backend_configure_dc - 配置分布式时钟
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_configure_dc(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->configure_dc) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->configure_dc(backend);
}

/**
 * backend_sync0_configure - 激活/关闭从站 DC Sync0 输出
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标
 * @enable: 非 0 激活，0 关闭
 * @cycle_time_ns: Sync0 周期（ns）
 * @shift_time_ns: Sync0 相位偏移（ns）
 *
 * 只在总线配置阶段调用，运行期重复调用会导致 Sync0 重建。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_sync0_configure(struct backend_instance *backend, size_t slave_index,
					   int enable, uint32_t cycle_time_ns,
					   int32_t shift_time_ns)
{
	if (!backend || !backend->ops || !backend->ops->sync0_configure) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->sync0_configure(backend, slave_index, enable, cycle_time_ns,
					     shift_time_ns);
}

/**
 * backend_sync0_read_status - 读回从站 Sync0 状态
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标
 * @status: 状态输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_sync0_read_status(struct backend_instance *backend, size_t slave_index,
					     struct mo_ecat_sync0_status *status)
{
	if (!backend || !backend->ops || !backend->ops->sync0_read_status) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (!status) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->sync0_read_status(backend, slave_index, status);
}

/**
 * backend_build_pdo_mapping - 调用后端建立 PDO 映射
 * @backend: 后端实例指针
 * @entries: PDO entry 映射数组，由调用者分配
 * @entry_count: entries 数组元素个数，允许为 0
 *
 * 后端会根据 entries 中描述的 PDO entry（从站索引、对象索引、位长度、
 * 方向等）建立 IOmap/domain，并回填每个 entry 在 PDO 数据区域中的
 * byte_offset 和 bit_offset。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_build_pdo_mapping(struct backend_instance *backend,
					     struct pdo_image_entry *entries, size_t entry_count)
{
	if (!backend || !backend->ops || !backend->ops->build_pdo_mapping) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (entry_count > 0 && !entries) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->build_pdo_mapping(backend, entries, entry_count);
}

/**
 * backend_get_pdo_image - 获取 PDO 数据映像
 * @backend: 后端实例指针
 * @image: PDO 数据映像输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_get_pdo_image(struct backend_instance *backend, struct pdo_image *image)
{
	if (!backend || !backend->translation_ops || !backend->translation_ops->get_pdo_image) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (!image) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->translation_ops->get_pdo_image(backend, image);
}

/**
 * backend_activate - 激活后端周期交换
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_activate(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->activate) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->activate(backend);
}

/**
 * backend_cyclic_receive - 后端周期接收
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_cyclic_receive(struct backend_instance *backend,
					  struct mo_ecat_cyclic_result *result)
{
	if (!backend || !backend->ops || !backend->ops->cyclic_receive) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (!result) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->cyclic_receive(backend, result);
}

/**
 * backend_cyclic_send - 后端周期发送
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_cyclic_send(struct backend_instance *backend,
				       struct mo_ecat_cyclic_result *result)
{
	if (!backend || !backend->ops || !backend->ops->cyclic_send) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (!result) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->cyclic_send(backend, result);
}

/**
 * backend_read_all_slave_states - 读取所有从站状态
 * @backend: 后端实例指针
 * @slaves: 从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_read_all_slave_states(struct backend_instance *backend,
						 struct slave *slaves, size_t slave_count)
{
	if (!backend || !backend->ops || !backend->ops->read_all_slave_states) {
		return BACKEND_ERROR_NOT_READY;
	}

	if (slave_count > 0 && !slaves) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->read_all_slave_states(backend, slaves, slave_count);
}

/**
 * backend_read_single_slave_state - 读取单个从站状态
 * @backend: 后端实例指针
 * @slave_index: 从站索引
 * @state: 从站状态输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_read_single_slave_state(struct backend_instance *backend,
						   size_t slave_index,
						   struct mo_ecat_node_state *state)
{
	if (!backend || !backend->ops || !backend->ops->read_single_slave_state || !state) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	return backend->ops->read_single_slave_state(backend, slave_index, state);
}

/**
 * backend_deactivate - 去激活后端周期交换
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_deactivate(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->deactivate) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->deactivate(backend);
}

/**
 * backend_recover_slave - 恢复单个从站到 OP
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_recover_slave(struct backend_instance *backend, size_t slave_index)
{
	if (!backend || !backend->ops || !backend->ops->recover_slave) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->recover_slave(backend, slave_index);
}

/**
 * backend_set_slave_al_state - 设置单个从站 AL 状态（调试用途）
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标
 * @target_state: 目标 AL 状态
 *
 * 直接写从站 AL Control 寄存器，不经过正常配置流程。
 * 仅 DEBUG_SLAVE 状态使用。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_set_slave_al_state(struct backend_instance *backend, size_t slave_index,
					      enum mo_ecat_node_al_state target_state)
{
	if (!backend || !backend->ops || !backend->ops->set_slave_al_state) {
		return BACKEND_ERROR_NOT_READY;
	}

	return backend->ops->set_slave_al_state(backend, slave_index, target_state);
}

/**
 * backend_close - 关闭后端
 * @backend: 后端实例指针
 */
void backend_close(struct backend_instance *backend)
{
	if (!backend || !backend->ops || !backend->ops->close) {
		return;
	}

	backend->ops->close(backend);
}
