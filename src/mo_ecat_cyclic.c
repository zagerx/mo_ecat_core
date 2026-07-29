/*
 * mo_ecat_cyclic.c - 周期数据访问
 *
 * 提供 PDO 周期收发、PDO entry 枚举以及输入/输出数据指针查询接口。
 */

#include <string.h>

#include "mo_ecat/mo_ecat_cyclic.h"
#include "master_priv.h"

/**
 * pdo_entry_mapping_in_bounds - 检查 PDO entry 映射是否在数据映像范围内
 * @image: PDO 数据映像
 * @mapping: PDO entry 物理映射
 *
 * Return: 在范围内返回非 0，否则返回 0
 */
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

/**
 * master_cyclic_receive - 主站周期接收
 * @master: 主站对象指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail master_cyclic_receive(struct mo_ecat_master *master,
							struct mo_ecat_cyclic_result *result)
{
	enum backend_error error;

	if (!master || !result) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}

	if (!master->pdo_mapping.is_active) {
		return MASTER_ERROR_INVALID_STATE;
	}

	memset(result, 0, sizeof(*result));
	error = backend_cyclic_receive(&master->backend, result);
	return master_error_from_backend(error);
}

/**
 * master_cyclic_send - 主站周期发送
 * @master: 主站对象指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail master_cyclic_send(struct mo_ecat_master *master,
						 struct mo_ecat_cyclic_result *result)
{
	enum backend_error error;

	if (!master || !result) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}

	if (!master->pdo_mapping.is_active) {
		return MASTER_ERROR_INVALID_STATE;
	}

	error = backend_cyclic_send(&master->backend, result);
	return master_error_from_backend(error);
}

/**
 * mo_ecat_master_get_cyclic_entry_count - 获取 PDO entry 数量
 * @master: 主站对象指针
 *
 * Return: PDO entry 数量；@master 为 NULL 时返回 0
 */
size_t mo_ecat_master_get_cyclic_entry_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	count = master->pdo_mapping.entry_count;
	return count;
}

/**
 * mo_ecat_master_get_cyclic_entry - 获取指定 PDO entry 逻辑描述
 * @master: 主站对象指针
 * @index: PDO entry 索引
 * @entry: PDO entry 输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_cyclic_entry(
	const struct mo_ecat_master *master,
	size_t index,
	struct mo_ecat_pdo_entry *entry)
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

/**
 * master_resolve_pdo_entry_mapping - 根据逻辑 entry 解析物理映射
 * @master: 主站对象指针
 * @entry: PDO entry 逻辑描述
 *
 * Return: 成功返回映射指针，失败返回 NULL
 */
static const struct master_pdo_entry_mapping *master_resolve_pdo_entry_mapping(
    const struct mo_ecat_master *master, const struct mo_ecat_pdo_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;

	if (!master || !entry) {
		return NULL;
	}

	if ((size_t)entry->entry_id >= master->pdo_mapping.entry_count) {
		return NULL;
	}

	mapping = &master->pdo_mapping.entries[entry->entry_id];
	if (mapping->entry.entry_id != entry->entry_id ||
	    mapping->entry.node_index != entry->node_index ||
	    mapping->entry.object_index != entry->object_index ||
	    mapping->entry.object_subindex != entry->object_subindex ||
	    mapping->entry.bit_length != entry->bit_length ||
	    mapping->entry.direction != entry->direction) {
		return NULL;
	}

	return mapping;
}

/**
 * mo_ecat_cyclic_input - 获取输入 PDO entry 数据指针
 * @master: 主站对象指针
 * @entry: PDO entry 逻辑描述
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
const void *mo_ecat_cyclic_input(const struct mo_ecat_master *master,
				 const struct mo_ecat_pdo_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;
	const void *data;

	if (!master || !entry || entry->direction != MO_ECAT_CYCLIC_INPUT) {
		return NULL;
	}
	mapping = master_resolve_pdo_entry_mapping(master, entry);

	if (!mapping || mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	return data;
}

/**
 * mo_ecat_cyclic_output - 获取输出 PDO entry 数据指针
 * @master: 主站对象指针
 * @entry: PDO entry 逻辑描述
 *
 * Return: 成功返回可写数据指针，失败返回 NULL
 */
void *mo_ecat_cyclic_output(struct mo_ecat_master *master,
			    const struct mo_ecat_pdo_entry *entry)
{
	const struct master_pdo_entry_mapping *mapping;
	void *data;

	if (!master || !entry || entry->direction != MO_ECAT_CYCLIC_OUTPUT) {
		return NULL;
	}
	mapping = master_resolve_pdo_entry_mapping(master, entry);

	if (!mapping || mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		return NULL;
	}

	data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	return data;
}

/**
 * cyclic_handle_resolve - 常数时间校验并解析句柄数据地址
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 * @direction: 本次访问期望的方向
 *
 * 校验：句柄有效、代际匹配、映射存在、周期活动、方向一致。
 *
 * Return: 成功返回数据地址，失败返回 NULL
 */
static void *cyclic_handle_resolve(const struct mo_ecat_master *master,
				   const struct mo_ecat_cyclic_handle *handle,
				   enum mo_ecat_cyclic_direction direction)
{
	if (!master || !handle || !handle->data ||
	    handle->generation == 0U ||
	    handle->generation != master->pdo_mapping.generation ||
	    handle->direction != (uint8_t)direction ||
	    !master->pdo_mapping.image.memory ||
	    !master->pdo_mapping.is_active) {
		return NULL;
	}

	return handle->data;
}

int mo_ecat_cyclic_bind(struct mo_ecat_master *master,
			const struct mo_ecat_pdo_entry *entry,
			struct mo_ecat_cyclic_handle *handle)
{
	const struct master_pdo_entry_mapping *mapping;

	if (!master || !entry || !handle) {
		return -1;
	}
	mapping = master_resolve_pdo_entry_mapping(master, entry);
	if (!mapping || mapping->generation != master->pdo_mapping.generation ||
	    !pdo_entry_mapping_in_bounds(&master->pdo_mapping.image, mapping)) {
		return -1;
	}

	handle->data = &master->pdo_mapping.image.memory[mapping->byte_offset];
	handle->generation = mapping->generation;
	handle->bit_length = mapping->entry.bit_length;
	handle->bit_offset = mapping->bit_offset;
	handle->direction = (uint8_t)mapping->entry.direction;
	return 0;
}

const void *mo_ecat_cyclic_read(const struct mo_ecat_master *master,
				const struct mo_ecat_cyclic_handle *handle)
{
	return cyclic_handle_resolve(master, handle, MO_ECAT_CYCLIC_INPUT);
}

void *mo_ecat_cyclic_write(struct mo_ecat_master *master,
			   const struct mo_ecat_cyclic_handle *handle)
{
	return cyclic_handle_resolve(master, handle, MO_ECAT_CYCLIC_OUTPUT);
}
