/*
 * pdo_layout.c - PDO 数据映像布局建立与周期交换启停
 *
 * 根据从站表扫描到的 PDO entry 建立核心层逻辑映射，调用后端建立物理
 * IOmap，并管理 PDO 映射的激活与去激活。
 */

#include <stdlib.h>

#include "pdo_layout.h"
#include "master_priv.h"

/* 静态辅助函数前向声明 */

static void _pdo_image_entries_build(const struct mo_ecat_master *master,
				     struct pdo_image_entry *entries);

/**
 * pdo_layout_build - 建立 PDO 数据映像布局
 * @master: 主站对象指针
 *
 * 统计所有从站的 PDO entry 总数，分配逻辑映射数组，调用后端建立物理映射，
 * 并获取 PDO 数据映像。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error pdo_layout_build(struct mo_ecat_master *master)
{
	struct pdo_image image = {0};
	struct pdo_image_entry *entries = NULL;
	enum backend_error error;
	size_t entry_count = 0;

	if (!master) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	for (size_t i = 0; i < master->slave_table.slave_count; ++i) {
		entry_count += master->slave_table.slaves[i].pdo_entry_count;
	}
	if (entry_count > UINT32_MAX) {
		return BACKEND_ERROR_PDO_MAPPING_FAILED;
	}

	if (entry_count > 0) {
		entries = calloc(entry_count, sizeof(*entries));
		if (!entries) {
			return BACKEND_ERROR_NO_MEMORY;
		}
		_pdo_image_entries_build(master, entries);
	}

	error = backend_build_pdo_mapping(&master->backend, entries, entry_count);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return error;
	}
	error = backend_get_pdo_image(&master->backend, &image);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return error;
	}
	pthread_mutex_lock(&master->slave_table_mutex);
	error = backend_translate_slave_info(&master->backend, master->slave_table.slaves,
					     master->slave_table.slave_count);
	pthread_mutex_unlock(&master->slave_table_mutex);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return error;
	}

	free(master->pdo_layout.entries);
	master->pdo_layout.image = image;
	master->pdo_layout.entries = entries;
	master->pdo_layout.entry_count = entry_count;
	master->pdo_layout.is_active = 0;
	return BACKEND_ERROR_NONE;
}

/**
 * pdo_layout_activate - 激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error pdo_layout_activate(struct mo_ecat_master *master)
{
	enum backend_error error;

	if (!master) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	error = backend_activate(&master->backend);
	if (error != BACKEND_ERROR_NONE) {
		return error;
	}

	master->pdo_layout.is_active = 1;
	return BACKEND_ERROR_NONE;
}

/**
 * pdo_layout_deactivate - 去激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error pdo_layout_deactivate(struct mo_ecat_master *master)
{
	enum backend_error error;

	if (!master) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	error = backend_deactivate(&master->backend);
	if (error != BACKEND_ERROR_NONE) {
		return error;
	}

	master->pdo_layout.is_active = 0;
	return BACKEND_ERROR_NONE;
}

/**
 * _pdo_image_entries_build - 根据从站表信息构建 PDO 映像条目
 * @master: 主站对象指针
 * @entries: PDO entry 映射输出数组，已由调用者分配
 *
 * 遍历所有从站的 PDO entry 扫描缓存，填充逻辑描述（entry_id、slave_index、
 * 对象索引、位长度、方向等）。
 */
static void _pdo_image_entries_build(const struct mo_ecat_master *master,
				     struct pdo_image_entry *entries)
{
	size_t index = 0;

	for (size_t i = 0; i < master->slave_table.slave_count; ++i) {
		const struct slave *slave = &master->slave_table.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct pdo_entry *entry = &slave->pdo_entries[j];
			struct pdo_image_entry *image_entry = &entries[index];

			image_entry->slave_entry.entry_id = (uint32_t)index;
			image_entry->slave_entry.slave_index = i;
			image_entry->slave_entry.spec = *entry;
			++index;
		}
	}
}
