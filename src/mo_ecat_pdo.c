/*
 * mo_ecat_pdo.c - PDO 布局与周期数据访问
 *
 * 提供 PDO 周期收发、PDO entry 枚举以及输入/输出数据指针查询接口。
 */

#include <string.h>

#include "mo_ecat/mo_ecat_pdo.h"
#include "master_priv.h"

/**
 * pdo_entry_equal - 比较两个 PDO entry 最小规格是否相同
 * @left: 左侧 PDO entry 规格
 * @right: 右侧 PDO entry 规格
 *
 * Return: 所有协议字段相同返回非 0，否则返回 0
 */
static int pdo_entry_equal(const struct pdo_entry *left, const struct pdo_entry *right)
{
	return left && right && left->object_index == right->object_index &&
	       left->object_subindex == right->object_subindex &&
	       left->bit_length == right->bit_length && left->direction == right->direction;
}

/**
 * pdo_image_entry_in_bounds - 检查 PDO 映像条目是否在数据映像范围内
 * @image: PDO 数据映像
 * @entry: PDO 映像条目
 *
 * Return: 在范围内返回非 0，否则返回 0
 */
static int pdo_image_entry_in_bounds(const struct pdo_image *image,
				     const struct pdo_image_entry *entry)
{
	size_t image_bits;
	size_t start_bit;
	size_t end_bit;

	if (!image || !entry || !image->memory || entry->record.spec.bit_length == 0 ||
	    entry->byte_offset >= image->size) {
		return 0;
	}

	image_bits = image->size * 8U;
	start_bit = (size_t)entry->byte_offset * 8U + entry->bit_offset;
	end_bit = start_bit + entry->record.spec.bit_length;
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

	if (!master->pdo_layout.is_active) {
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

	if (!master->pdo_layout.is_active) {
		return MASTER_ERROR_INVALID_STATE;
	}

	error = backend_cyclic_send(&master->backend, result);
	return master_error_from_backend(error);
}

/**
 * mo_ecat_master_get_pdo_entry_count - 获取 PDO entry 数量
 * @master: 主站对象指针
 *
 * Return: PDO entry 数量；@master 为 NULL 时返回 0
 */
size_t mo_ecat_master_get_pdo_entry_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	count = master->pdo_layout.entry_count;
	return count;
}

/**
 * mo_ecat_master_get_pdo_entry - 获取指定 PDO entry 逻辑描述
 * @master: 主站对象指针
 * @index: PDO entry 索引
 * @record: PDO entry 记录输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_pdo_entry(const struct mo_ecat_master *master, size_t index,
				 struct pdo_entry_record *record)
{
	if (!master || !record) {
		return -1;
	}

	if (index >= master->pdo_layout.entry_count) {
		return -1;
	}

	*record = master->pdo_layout.entries[index].record;
	return 0;
}

/**
 * master_resolve_pdo_image_entry - 根据 PDO 记录解析数据映像条目
 * @master: 主站对象指针
 * @record: Master 发现的 PDO entry 记录
 *
 * Return: 成功返回映射指针，失败返回 NULL
 */
static const struct pdo_image_entry *
master_resolve_pdo_image_entry(const struct mo_ecat_master *master,
			       const struct pdo_entry_record *record)
{
	const struct pdo_image_entry *entry;

	if (!master || !record) {
		return NULL;
	}

	if ((size_t)record->entry_id >= master->pdo_layout.entry_count) {
		return NULL;
	}

	entry = &master->pdo_layout.entries[record->entry_id];
	if (entry->record.entry_id != record->entry_id ||
	    entry->record.slave_index != record->slave_index ||
	    !pdo_entry_equal(&entry->record.spec, &record->spec)) {
		return NULL;
	}

	return entry;
}

/**
 * mo_ecat_pdo_input - 获取输入 PDO entry 数据指针
 * @master: 主站对象指针
 * @record: Master 发现的 PDO entry 记录
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
			      const struct pdo_entry_record *record)
{
	const struct pdo_image_entry *entry;
	const void *data;

	if (!master || !record || record->spec.direction != MO_ECAT_PDO_INPUT) {
		return NULL;
	}
	entry = master_resolve_pdo_image_entry(master, record);

	if (!entry || entry->generation != master->pdo_layout.generation ||
	    !pdo_image_entry_in_bounds(&master->pdo_layout.image, entry)) {
		return NULL;
	}

	data = &master->pdo_layout.image.memory[entry->byte_offset];
	return data;
}

/**
 * mo_ecat_pdo_output - 获取输出 PDO entry 数据指针
 * @master: 主站对象指针
 * @record: Master 发现的 PDO entry 记录
 *
 * Return: 成功返回可写数据指针，失败返回 NULL
 */
void *mo_ecat_pdo_output(struct mo_ecat_master *master, const struct pdo_entry_record *record)
{
	const struct pdo_image_entry *entry;
	void *data;

	if (!master || !record || record->spec.direction != MO_ECAT_PDO_OUTPUT) {
		return NULL;
	}
	entry = master_resolve_pdo_image_entry(master, record);

	if (!entry || entry->generation != master->pdo_layout.generation ||
	    !pdo_image_entry_in_bounds(&master->pdo_layout.image, entry)) {
		return NULL;
	}

	data = &master->pdo_layout.image.memory[entry->byte_offset];
	return data;
}

/**
 * pdo_handle_resolve - 常数时间校验并解析 PDO 句柄数据地址
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 * @direction: 本次访问期望的方向
 *
 * 校验：句柄有效、代际匹配、映射存在、周期活动、方向一致。
 *
 * Return: 成功返回数据地址，失败返回 NULL
 */
static void *pdo_handle_resolve(const struct mo_ecat_master *master,
				const struct mo_ecat_pdo_handle *handle,
				enum mo_ecat_pdo_direction direction)
{
	if (!master || !handle || !handle->data || handle->generation == 0U ||
	    handle->generation != master->pdo_layout.generation ||
	    handle->direction != (uint8_t)direction || !master->pdo_layout.image.memory ||
	    !master->pdo_layout.is_active) {
		return NULL;
	}

	return handle->data;
}

int mo_ecat_pdo_bind(struct mo_ecat_master *master, const struct pdo_entry_record *record,
		     struct mo_ecat_pdo_handle *handle)
{
	const struct pdo_image_entry *entry;

	if (!master || !record || !handle) {
		return -1;
	}
	entry = master_resolve_pdo_image_entry(master, record);
	if (!entry || entry->generation != master->pdo_layout.generation ||
	    !pdo_image_entry_in_bounds(&master->pdo_layout.image, entry)) {
		return -1;
	}

	handle->data = &master->pdo_layout.image.memory[entry->byte_offset];
	handle->generation = entry->generation;
	handle->bit_length = entry->record.spec.bit_length;
	handle->bit_offset = entry->bit_offset;
	handle->direction = (uint8_t)entry->record.spec.direction;
	return 0;
}

const void *mo_ecat_pdo_read(const struct mo_ecat_master *master,
			     const struct mo_ecat_pdo_handle *handle)
{
	return pdo_handle_resolve(master, handle, MO_ECAT_PDO_INPUT);
}

void *mo_ecat_pdo_write(struct mo_ecat_master *master, const struct mo_ecat_pdo_handle *handle)
{
	return pdo_handle_resolve(master, handle, MO_ECAT_PDO_OUTPUT);
}
