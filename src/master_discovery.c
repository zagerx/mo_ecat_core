/**
 * @file master_discovery.c
 * @brief 主站扫描资源与后端打开管理
 */

#include <stdlib.h>
#include <string.h>

#include "master_priv.h"

/**
 * 释放主站占用的扫描与映射资源。
 *
 * 关闭后端、释放 PDO entry 映射数组和从站表，但保留 generation
 * 以便后续重新映射时能够区分旧映射。
 */
void master_resources_release(struct mo_ecat_master *master)
{
	uint32_t generation;

	if (!master) {
		return;
	}

	generation = master->pdo_mapping.generation;
	backend_close(&master->backend);

	free(master->pdo_mapping.entries);
	free(master->topology.slaves);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->pdo_mapping, 0, sizeof(master->pdo_mapping));
	master->pdo_mapping.generation = generation;
	master->topology.slaves = NULL;
	master->topology.slave_count = 0;
}

/** 为同一批已解析地址的 PDO entry 标记所属映射版本。 */
static void master_pdo_entry_mappings_set_generation(
	struct master_pdo_entry_mapping *entries, size_t entry_count, uint32_t generation)
{
	for (size_t i = 0; i < entry_count; ++i) {
		entries[i].generation = generation;
	}
}

/**
 * 构建主站从站表。
 *
 * 从后端获取扫描到的从站数量，分配从站信息数组，并由后端将
 * 适配层从站信息翻译到核心层结构。
 */
int master_topology_build(struct mo_ecat_master *master)
{
	size_t slave_count;

	if (!master) {
		return -1;
	}

	if (backend_get_slave_count(&master->backend, &slave_count) < 0) {
		return -1;
	}

	if (slave_count > 0) {
		if (slave_count > SIZE_MAX / sizeof(*master->topology.slaves)) {
			return -1;
		}
		master->topology.slaves = calloc(slave_count, sizeof(*master->topology.slaves));
		if (!master->topology.slaves) {
			return -1;
		}
	}

	master->topology.slave_count = slave_count;
	if (slave_count > 0 &&
	    backend_translate_slave_info(&master->backend, master->topology.slaves,
					 slave_count) < 0) {
		return -1;
	}
	return 0;
}

/** 读取所有从站当前状态到从站表。 */
int master_topology_refresh_states(struct mo_ecat_master *master)
{
	if (!master || (master->topology.slave_count > 0 && !master->topology.slaves)) {
		return -1;
	}

	return backend_read_all_slave_states(&master->backend, master->topology.slaves,
					     master->topology.slave_count);
}

/**
 * 将所有从站的 PDO entry 描述展开为扁平映射数组。
 *
 * 本函数只填充 slave_index、object_index、object_subindex、bit_length
 * 和 direction；字节/位偏移由后端在建立 IOmap/domain 时回填。
 */
static void master_pdo_entry_mappings_build(const struct mo_ecat_master *master,
					     struct master_pdo_entry_mapping *entries)
{
	size_t idx = 0;

	for (size_t i = 0; i < master->topology.slave_count; ++i) {
		const struct master_slave *slave = &master->topology.slaves[i];

		for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
			const struct master_slave_pdo_entry *entry = &slave->pdo_entries[j];
			struct master_pdo_entry_mapping *mapping = &entries[idx];

			mapping->entry.entry_id = (uint32_t)idx;
			mapping->entry.node_index = i;
			mapping->entry.object_index = entry->object_index;
			mapping->entry.object_subindex = entry->object_subindex;
			mapping->entry.bit_length = entry->bit_length;
			mapping->entry.direction = entry->direction;
			++idx;
		}
	}
}

/**
 * 建立主站 PDO 映射并一次性提交结果。
 *
 * 调用前 DC 必须已由状态机配置成功。从站 PDO 描述先被展开为 entries，
 * 后端在建立 IOmap/domain 时回填地址偏移；仅当映射、PDO 数据区域和从站
 * 信息刷新均成功时，才替换 master->pdo_mapping 中的旧映射。
 */
int master_pdo_mapping_build(struct mo_ecat_master *master)
{
	struct master_pdo_image image = {0};
	struct master_pdo_entry_mapping *entries = NULL;
	size_t entry_count;
	uint32_t generation;

	if (!master) {
		return -1;
	}

	entry_count = 0;
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
 * 激活主站 PDO 周期交换。
 *
 * 激活成功后，周期数据接收、发送与访问接口方可使用。
 */
int master_pdo_mapping_activate(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	if (backend_activate(&master->backend) < 0) {
		return -1;
	}

	master->pdo_mapping.is_active = 1;
	return 0;
}

/** 停止主站 PDO 周期交换。 */
int master_pdo_mapping_deactivate(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	if (backend_deactivate(&master->backend) < 0) {
		return -1;
	}

	master->pdo_mapping.is_active = 0;
	return 0;
}
