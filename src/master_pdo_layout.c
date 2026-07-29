/*
 * master_pdo_layout.c - 主站 PDO 数据映像布局建立与周期交换启停
 *
 * 根据拓扑扫描到的 PDO entry 建立核心层逻辑映射，调用后端建立物理
 * IOmap，并管理 PDO 映射的激活与去激活。
 */

#include <stdlib.h>

#include "master_pdo_layout.h"
#include "master_priv.h"

/**
 * master_pdo_entry_export - 将从站私有扫描条目转换为公开 PDO entry
 * @destination: 公开 PDO entry 输出
 * @source: 从站扫描得到的最小 PDO entry 规格
 * @entry_id: Master 全局 entry 标识
 * @slave_index: entry 所属拓扑从站下标
 */
static void master_pdo_entry_export(struct pdo_entry_record *destination,
				    const struct pdo_entry *source, uint32_t entry_id,
				    size_t slave_index)
{
	destination->entry_id = entry_id;
	destination->slave_index = slave_index;
	destination->spec = *source;
}

/**
 * pdo_image_entries_build - 根据拓扑信息构建 PDO 映像条目
 * @master: 主站对象指针
 * @entries: PDO entry 映射输出数组，已由调用者分配
 *
 * 遍历所有从站的 PDO entry 扫描缓存，填充逻辑描述（entry_id、slave_index、
 * 对象索引、位长度、方向等）。
 */
static void pdo_image_entries_build(const struct mo_ecat_master *master,
				    struct pdo_image_entry *entries)
{
	size_t index = 0;

	for (size_t i = 0; i < master->topology.slave_count; ++i) {
		const struct slave *slave = &master->topology.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct pdo_entry *entry = &slave->pdo_entries[j];
			struct pdo_image_entry *image_entry = &entries[index];

			master_pdo_entry_export(&image_entry->record, entry, (uint32_t)index, i);
			++index;
		}
	}
}

/**
 * master_pdo_layout_build - 建立主站 PDO 数据映像布局
 * @master: 主站对象指针
 *
 * 统计所有从站的 PDO entry 总数，分配逻辑映射数组，调用后端建立物理映射，
 * 并获取 PDO 数据映像。新的映射会刷新代际计数器。
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail master_pdo_layout_build(struct mo_ecat_master *master)
{
	struct pdo_image image = {0};
	struct pdo_image_entry *entries = NULL;
	enum backend_error error;
	size_t entry_count = 0;
	uint32_t generation;

	if (!master) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}

	for (size_t i = 0; i < master->topology.slave_count; ++i) {
		entry_count += master->topology.slaves[i].pdo_entry_count;
	}
	if (entry_count > UINT32_MAX) {
		return MASTER_ERROR_PDO_MAPPING_FAILED;
	}

	if (entry_count > 0) {
		entries = calloc(entry_count, sizeof(*entries));
		if (!entries) {
			return MASTER_ERROR_NO_MEMORY;
		}
		pdo_image_entries_build(master, entries);
	}

	error = backend_build_pdo_mapping(&master->backend, entries, entry_count);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return master_error_from_backend(error);
	}
	error = backend_get_pdo_image(&master->backend, &image);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return master_error_from_backend(error);
	}
	error = backend_translate_slave_info(&master->backend, master->topology.slaves,
					    master->topology.slave_count);
	if (error != BACKEND_ERROR_NONE) {
		free(entries);
		return master_error_from_backend(error);
	}

	generation = master->pdo_layout.generation + 1U;
	if (generation == 0U) {
		generation = 1U;
	}
	free(master->pdo_layout.entries);
	master->pdo_layout.image = image;
	master->pdo_layout.entries = entries;
	master->pdo_layout.entry_count = entry_count;
	master->pdo_layout.generation = generation;
	master->pdo_layout.is_active = 0;
	return MASTER_ERROR_NONE;
}

/**
 * master_pdo_layout_activate - 激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail master_pdo_layout_activate(struct mo_ecat_master *master)
{
	enum backend_error error;

	if (!master) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}
	error = backend_activate(&master->backend);
	if (error != BACKEND_ERROR_NONE) {
		return master_error_from_backend(error);
	}

	master->pdo_layout.is_active = 1;
	return MASTER_ERROR_NONE;
}

/**
 * master_pdo_layout_deactivate - 去激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
enum master_error_detail master_pdo_layout_deactivate(struct mo_ecat_master *master)
{
	enum backend_error error;

	if (!master) {
		return MASTER_ERROR_INVALID_ARGUMENT;
	}
	error = backend_deactivate(&master->backend);
	if (error != BACKEND_ERROR_NONE) {
		return master_error_from_backend(error);
	}

	master->pdo_layout.is_active = 0;
	return MASTER_ERROR_NONE;
}
