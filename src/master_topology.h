/*
 * master_topology.h - 主站拓扑内部接口
 *
 * 提供根据后端扫描结果构建与刷新主站内部从站拓扑的内部接口。
 */

#ifndef MASTER_TOPOLOGY_H
#define MASTER_TOPOLOGY_H

struct mo_ecat_master;

/**
 * master_topology_build - 构建主站内部拓扑
 * @master: 主站对象指针
 *
 * 根据后端扫描结果构建主站内部拓扑。
 *
 * Return: 0 成功，非 0 失败
 */
int master_topology_build(struct mo_ecat_master *master);

/**
 * master_topology_refresh_states - 刷新从站拓扑状态
 * @master: 主站对象指针
 *
 * 刷新主站内部拓扑中全部节点的当前状态。
 *
 * Return: 0 成功，非 0 失败
 */
int master_topology_refresh_states(struct mo_ecat_master *master);

#endif /* MASTER_TOPOLOGY_H */
