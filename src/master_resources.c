/*
 * master_resources.c - 主站运行资源释放
 *
 * 关闭后端、释放 PDO 映射与拓扑内存，同时保留映射代际计数器。
 */

#include <stdlib.h>
#include <string.h>

#include "master_priv.h"
#include "master_resources.h"

/**
 * master_resources_release - 释放主站持有的资源
 * @master: 主站对象指针
 *
 * 关闭后端并释放主站持有的拓扑与 PDO 映射资源。
 * 映射代际计数器会保留，用于识别旧的 PDO entry 引用。
 */
void master_resources_release(struct mo_ecat_master *master)
{
	uint32_t generation;

	if (!master) {
		return;
	}

	generation = master->pdo_mapping.generation;
	backend_close(&master->backend);

	free(master->pdo_mapping.entry_mappings);
	free(master->topology.slaves);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->pdo_mapping, 0, sizeof(master->pdo_mapping));
	master->pdo_mapping.generation = generation;
	master->topology.slaves = NULL;
	master->topology.slave_count = 0;
}
