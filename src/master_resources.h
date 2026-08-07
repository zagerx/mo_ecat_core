/*
 * master_resources.h - 主站资源释放接口
 *
 * 提供主站关闭后端并释放拓扑与 PDO 映射资源的内部接口。
 */

#ifndef MASTER_RESOURCES_H
#define MASTER_RESOURCES_H

struct mo_ecat_master;

/* 仅释放 PDO 运行资源，保留后端与拓扑（FAULT 后继续刷新状态用）。 */
void master_runtime_pdo_release(struct mo_ecat_master *master);

/* 关闭后端并释放 PDO 运行资源。 */
void master_runtime_release(struct mo_ecat_master *master);

/* 释放主站持有的全部资源（含后端与拓扑）。 */
void master_resources_release(struct mo_ecat_master *master);

#endif /* MASTER_RESOURCES_H */
