/**
 * @file topology.c
 * @brief 主站拓扑节点查询接口
 */

#include <string.h>

#include "mo_ecat/mo_ecat_topology.h"
#include "master_priv.h"

size_t mo_ecat_master_get_node_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	count = master->slave_table.count;
	return count;
}

int mo_ecat_master_get_node_info(const struct mo_ecat_master *master,
				 size_t index, struct mo_ecat_node_info *info)
{
	const struct mo_ecat_slave *slave;

	if (!master || !info) {
		return -1;
	}

	if (index >= master->slave_table.count) {
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
	info->has_dc = slave->base_info.has_dc;
	info->state = slave->state;

	return 0;
}
