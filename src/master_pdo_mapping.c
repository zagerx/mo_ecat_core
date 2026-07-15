/*
 * master_pdo_mapping.c - 主站 PDO 映射建立与周期交换启停
 *
 * 根据拓扑扫描到的 PDO entry 建立核心层逻辑映射，调用后端建立物理
 * IOmap，并管理 PDO 映射的激活与去激活。
 */

#include <stdlib.h>

#include "master_pdo_mapping.h"
#include "master_priv.h"

/**
 * master_pdo_entry_mappings_set_generation - 为所有 PDO entry 映射设置代际
 * @entries: PDO entry 映射数组
 * @entry_count: 数组元素个数
 * @generation: 代际值
 */
static void master_pdo_entry_mappings_set_generation(
	struct master_pdo_entry_mapping *entries, size_t entry_count, uint32_t generation)
{
	for (size_t i = 0; i < entry_count; ++i) {
		entries[i].generation = generation;
	}
}

/**
 * master_pdo_entry_mappings_build - 根据拓扑信息构建 PDO entry 逻辑映射
 * @master: 主站对象指针
 * @entries: PDO entry 映射输出数组，已由调用者分配
 *
 * 遍历所有从站的 PDO entry 扫描缓存，填充逻辑描述（entry_id、node_index、
 * 对象索引、位长度、方向等）。
 */
static void master_pdo_entry_mappings_build(const struct mo_ecat_master *master,
					    struct master_pdo_entry_mapping *entries)
{
	size_t index = 0;

	for (size_t i = 0; i < master->topology.slave_count; ++i) {
		const struct master_slave *slave = &master->topology.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct master_slave_pdo_entry *entry = &slave->pdo_entries[j];
			struct master_pdo_entry_mapping *mapping = &entries[index];

			mapping->entry.entry_id = (uint32_t)index;
			mapping->entry.node_index = i;
			mapping->entry.object_index = entry->object_index;
			mapping->entry.object_subindex = entry->object_subindex;
			mapping->entry.bit_length = entry->bit_length;
			mapping->entry.direction = entry->direction;
			++index;
		}
	}
}

/**
 * master_pdo_mapping_build - 建立主站 PDO 映射
 * @master: 主站对象指针
 *
 * 统计所有从站的 PDO entry 总数，分配逻辑映射数组，调用后端建立物理映射，
 * 并获取 PDO 数据映像。新的映射会刷新代际计数器。
 *
 * Return: 0 成功，非 0 失败
 */
int master_pdo_mapping_build(struct mo_ecat_master *master)
{
	struct master_pdo_image image = {0};
	struct master_pdo_entry_mapping *entries = NULL;
	size_t entry_count = 0;
	uint32_t generation;

	if (!master) {
		return -1;
	}

	for (size_t i = 0; i < master->topology.slave_count; ++i) {
		entry_count += master->topology.slaves[i].pdo_entry_count;
	}
	if (entry_count > UINT32_MAX) {
		return -1;
	}

	if (entry_count > 0) {
		entries = calloc(entry_count, sizeof(*entries));
		if (!entries) {
			return -1;
		}
		master_pdo_entry_mappings_build(master, entries);
	}

	if (backend_build_pdo_mapping(&master->backend, entries, entry_count) < 0) {
		free(entries);
		return -1;
	}
	if (backend_get_pdo_image(&master->backend, &image) < 0) {
		free(entries);
		return -1;
	}
	if (backend_translate_slave_info(&master->backend, master->topology.slaves,
					 master->topology.slave_count) < 0) {
		free(entries);
		return -1;
	}

	generation = master->pdo_mapping.generation + 1U;
	if (generation == 0U) {
		generation = 1U;
	}
	master_pdo_entry_mappings_set_generation(entries, entry_count, generation);

	free(master->pdo_mapping.entries);
	master->pdo_mapping.image = image;
	master->pdo_mapping.entries = entries;
	master->pdo_mapping.entry_count = entry_count;
	master->pdo_mapping.generation = generation;
	master->pdo_mapping.is_active = 0;
	return 0;
}

/**
 * master_pdo_mapping_activate - 激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
int master_pdo_mapping_activate(struct mo_ecat_master *master)
{
	if (!master || backend_activate(&master->backend) < 0) {
		return -1;
	}

	master->pdo_mapping.is_active = 1;
	return 0;
}

/**
 * master_pdo_mapping_deactivate - 去激活 PDO 周期交换
 * @master: 主站对象指针
 *
 * Return: 0 成功，非 0 失败
 */
int master_pdo_mapping_deactivate(struct mo_ecat_master *master)
{
	if (!master || backend_deactivate(&master->backend) < 0) {
		return -1;
	}

	master->pdo_mapping.is_active = 0;
	return 0;
}
