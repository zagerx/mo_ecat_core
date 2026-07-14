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

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	count = master->slave_table.count;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return count;
}

int mo_ecat_master_get_slave_info(const struct mo_ecat_master *master,
				  size_t index, struct mo_ecat_slave_info *info)
{
	const struct mo_ecat_slave *slave;

	if (!master || !info) {
		return -1;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	if (index >= master->slave_table.count) {
		pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
		return -1;
	}

	slave = &master->slave_table.slaves[index];
	memset(info, 0, sizeof(*info));
	info->position = slave->position;
	info->alias = slave->alias;
	info->vendor_id = slave->vendor_id;
	info->product_code = slave->product_code;
	info->revision_number = slave->revision_number;
	memcpy(info->name, slave->name, sizeof(info->name) - 1);
	info->name[sizeof(info->name) - 1] = '\0';
	info->has_dc = slave->has_dc;
	info->pdo_entry_count = slave->pdo_entry_count;
	info->state = slave->state;

	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return 0;
}

int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state;
	int result;

	if (!master) {
		return -1;
	}

	pthread_mutex_lock(&master->lock);
	state = master_state_from_sm(master);
	if (state == MO_ECAT_MASTER_STATE_INIT ||
	    (state == MO_ECAT_MASTER_STATE_IDLE && master->slave_table.count == 0)) {
		pthread_mutex_unlock(&master->lock);
		return -1;
	}

	result = backend_read_slave_states(&master->backend,
					   master->slave_table.slaves,
					   master->slave_table.count);

	pthread_mutex_unlock(&master->lock);
	return result;
}
