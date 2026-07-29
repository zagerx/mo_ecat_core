/*
 * mo_ecat_pdo.c - PDO 布局与周期数据访问
 *
 * 提供 PDO 周期收发、PDO entry 枚举以及输入/输出数据指针查询接口。
 */

#include <string.h>

#include "mo_ecat/mo_ecat_pdo.h"
#include "master_priv.h"

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
 * mo_ecat_master_get_pdo_generation - 获取当前 PDO 数据映像布局代际
 * @master: 主站对象指针
 *
 * Return: 当前布局代际；@master 为 NULL 时返回 0
 */
uint32_t mo_ecat_master_get_pdo_generation(const struct mo_ecat_master *master)
{
	return master ? master->pdo_layout.generation : 0U;
}

/**
 * pdo_image_entry_data - 根据 entry_id 和方向解析 PDO 数据地址
 * @master: 主站对象指针
 * @entry_id: Master 当前 PDO 布局中的全局条目编号
 * @direction: 期望的数据方向
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
static void *pdo_image_entry_data(const struct mo_ecat_master *master, uint32_t entry_id,
				  enum mo_ecat_pdo_direction direction)
{
	const struct pdo_image_entry *entry;

	if (!master || !master->pdo_layout.is_active || !master->pdo_layout.image.memory ||
	    (size_t)entry_id >= master->pdo_layout.entry_count) {
		return NULL;
	}

	entry = &master->pdo_layout.entries[entry_id];
	if (entry->record.entry_id != entry_id || entry->record.spec.direction != direction ||
	    entry->bit_offset != 0U ||
	    !pdo_image_entry_in_bounds(&master->pdo_layout.image, entry)) {
		return NULL;
	}

	return &master->pdo_layout.image.memory[entry->byte_offset];
}

/**
 * mo_ecat_pdo_read - 根据 entry_id 获取输入 PDO 数据指针
 * @master: 主站对象指针
 * @entry_id: Master 当前 PDO 布局中的全局条目编号
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
const void *mo_ecat_pdo_read(const struct mo_ecat_master *master, uint32_t entry_id)
{
	return pdo_image_entry_data(master, entry_id, MO_ECAT_PDO_INPUT);
}

/**
 * mo_ecat_pdo_write - 根据 entry_id 获取输出 PDO 数据可写指针
 * @master: 主站对象指针
 * @entry_id: Master 当前 PDO 布局中的全局条目编号
 *
 * Return: 成功返回可写数据指针，失败返回 NULL
 */
void *mo_ecat_pdo_write(struct mo_ecat_master *master, uint32_t entry_id)
{
	return pdo_image_entry_data(master, entry_id, MO_ECAT_PDO_OUTPUT);
}
