/*
 * master_resources.h - 主站资源释放接口
 *
 * 提供主站关闭后端并释放拓扑与 PDO 映射资源的内部接口。
 */

#ifndef MASTER_RESOURCES_H
#define MASTER_RESOURCES_H

struct mo_ecat_master;

/**
 * master_resources_release - 释放主站持有的资源
 * @master: 主站对象指针
 *
 * 关闭后端并释放主站持有的拓扑与 PDO 映射资源。
 */
void master_resources_release(struct mo_ecat_master *master);

#endif /* MASTER_RESOURCES_H */
