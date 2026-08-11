/*
 * mo_ecat_topology.c - 主站拓扑节点查询接口
 *
 * 提供应用层查询已发现从站数量与节点信息的公开接口。
 */

#include <string.h>

#include "mo_ecat/mo_ecat_topology.h"
#include "master_priv.h"

/**
 * mo_ecat_master_get_node_count - 获取已发现从站数量
 * @master: 主站对象指针
 *
 * Return: 从站数量；@master 为 NULL 时返回 0
 */
size_t mo_ecat_master_get_node_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->slave_table_mutex);
	count = master->slave_table.slave_count;
	pthread_mutex_unlock((pthread_mutex_t *)&master->slave_table_mutex);
	return count;
}

/**
 * mo_ecat_master_get_node_info - 获取指定从站节点信息
 * @master: 主站对象指针
 * @index: 从站索引
 * @info: 节点信息输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_node_info(const struct mo_ecat_master *master,
				 size_t index, struct mo_ecat_node_info *info)
{
	const struct slave *slave;

	if (!master || !info) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->slave_table_mutex);
	if (index >= master->slave_table.slave_count || !master->slave_table.slaves) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->slave_table_mutex);
		return -1;
	}

	slave = &master->slave_table.slaves[index];
	memset(info, 0, sizeof(*info));
	info->position = slave->base_info.position;
	info->alias = slave->base_info.alias;
	info->vendor_id = slave->base_info.vendor_id;
	info->product_code = slave->base_info.product_code;
	info->revision_number = slave->base_info.revision_number;
	memcpy(info->name, slave->base_info.name, sizeof(info->name));
	info->dc_supported = slave->base_info.dc_supported;
	info->state = slave->state;
	pthread_mutex_unlock((pthread_mutex_t *)&master->slave_table_mutex);

	return 0;
}

/**
 * mo_ecat_master_get_slave_detail - 获取指定从站的配置详情
 * @master: 主站对象指针
 * @index: 从站索引
 * @detail: 详情输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_slave_detail(const struct mo_ecat_master *master, size_t index,
				    struct mo_ecat_slave_detail *detail)
{
	const struct slave *slave;

	if (!master || !detail) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->slave_table_mutex);
	if (index >= master->slave_table.slave_count || !master->slave_table.slaves) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->slave_table_mutex);
		return -1;
	}

	slave = &master->slave_table.slaves[index];
	memset(detail, 0, sizeof(*detail));

	for (size_t i = 0; i < SLAVE_MAX_SYNC_MANAGERS; ++i) {
		const struct slave_sync_manager *sm = &slave->base_info.sm[i];

		if (sm->length == 0) {
			continue;
		}
		if (detail->sm_count >= MO_ECAT_MAX_SLAVE_SM) {
			break;
		}
		detail->sm[detail->sm_count].start_address = sm->start_address;
		detail->sm[detail->sm_count].length = sm->length;
		detail->sm[detail->sm_count].type = sm->type;
		++detail->sm_count;
	}

	for (size_t i = 0; i < slave->pdo_entry_count; ++i) {
		if (detail->pdo_entry_count >= MO_ECAT_MAX_SLAVE_PDO_ENTRIES) {
			break;
		}
		detail->pdo_entries[detail->pdo_entry_count] = slave->pdo_entries[i];
		++detail->pdo_entry_count;
	}

	pthread_mutex_unlock((pthread_mutex_t *)&master->slave_table_mutex);
	return 0;
}
