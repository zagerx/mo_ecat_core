/*
 * master_resources.h - 主站资源释放接口
 *
 * 提供主站关闭后端并释放拓扑与 PDO 映射资源的内部接口。
 */

#ifndef MASTER_RESOURCES_H
#define MASTER_RESOURCES_H

struct mo_ecat_master;

void master_runtime_release(struct mo_ecat_master *master);

void master_resources_release(struct mo_ecat_master *master);

#endif /* MASTER_RESOURCES_H */
