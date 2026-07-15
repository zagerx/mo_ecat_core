#ifndef MASTER_RESOURCES_H
#define MASTER_RESOURCES_H

struct mo_ecat_master;

/** 关闭后端并释放主站持有的拓扑与 PDO 映射资源。 */
void master_resources_release(struct mo_ecat_master *master);

#endif /* MASTER_RESOURCES_H */
