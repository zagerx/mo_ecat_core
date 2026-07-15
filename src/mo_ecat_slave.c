/**
 * @file mo_ecat_slave.c
 * @brief 主站从站诊断接口
 */

#include <string.h>

#include "mo_ecat/mo_ecat_slave.h"
#include "master_priv.h"

size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master)
{
	size_t count;

	if (!master) {
		return 0;
	}

	count = master->slave_table.count;
	return count;
}

int mo_ecat_master_get_slave_info(const struct mo_ecat_master *master,
				  size_t index, struct mo_ecat_slave_info *info)
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
	info->base_info = slave->base_info;
	info->pdo_entry_count = slave->pdo_entry_count;
	info->state = slave->state;

	return 0;
}

int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master)
{
	if (!master) {
		return -1;
	}

	if (atomic_load(&master->state) == MO_ECAT_MASTER_STATE_INIT ||
	    (atomic_load(&master->state) == MO_ECAT_MASTER_STATE_IDLE &&
	     master->slave_table.count == 0)) {
		return -1;
	}

	return 0;
}
