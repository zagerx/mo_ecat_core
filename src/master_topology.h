#ifndef MASTER_TOPOLOGY_H
#define MASTER_TOPOLOGY_H

struct mo_ecat_master;

/** 根据后端扫描结果构建主站内部拓扑。 */
int master_topology_build(struct mo_ecat_master *master);

/** 刷新主站内部拓扑中全部节点的当前状态。 */
int master_topology_refresh_states(struct mo_ecat_master *master);

#endif /* MASTER_TOPOLOGY_H */
