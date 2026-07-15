/**
 * @file master_resources.c
 * @brief 主站运行资源释放
 */

#include <stdlib.h>
#include <string.h>

#include "master_priv.h"
#include "master_resources.h"

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
