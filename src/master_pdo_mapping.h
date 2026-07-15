#ifndef MASTER_PDO_MAPPING_H
#define MASTER_PDO_MAPPING_H

struct mo_ecat_master;

/** 建立主站 PDO 映射与 PDO 数据区域。 */
int master_pdo_mapping_build(struct mo_ecat_master *master);

/** 激活或停用主站 PDO 周期交换。 */
int master_pdo_mapping_activate(struct mo_ecat_master *master);
int master_pdo_mapping_deactivate(struct mo_ecat_master *master);

#endif /* MASTER_PDO_MAPPING_H */
