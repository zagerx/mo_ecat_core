/*
 * master_topology.h - 主站拓扑内部接口
 *
 * 提供根据后端扫描结果构建与刷新主站内部从站拓扑的内部接口。
 */

#ifndef MASTER_TOPOLOGY_H
#define MASTER_TOPOLOGY_H

struct mo_ecat_master;

int master_topology_build(struct mo_ecat_master *master, size_t slave_count);

int master_topology_refresh_states(struct mo_ecat_master *master);

#endif /* MASTER_TOPOLOGY_H */
