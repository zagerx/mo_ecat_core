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
 * master_runtime_release - 关闭后端并释放 PDO 运行资源
 * @master: 主站对象指针
 *
 * 故障时保留最后一次拓扑快照，供应用层读取节点 AL 状态与状态码。
 */
void master_runtime_release(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	backend_close(&master->backend);

	free(master->pdo_layout.entries);
	memset(&master->backend, 0, sizeof(master->backend));
	memset(&master->pdo_layout, 0, sizeof(master->pdo_layout));
}

/**
 * master_resources_release - 释放主站持有的全部资源
 * @master: 主站对象指针
 */
void master_resources_release(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	master_runtime_release(master);
	pthread_mutex_lock(&master->topology_mutex);
	free(master->topology.slaves);
	master->topology.slaves = NULL;
	master->topology.slave_count = 0;
	pthread_mutex_unlock(&master->topology_mutex);
}
