/*
 * master_pdo_mapping.h - 主站 PDO 映射内部接口
 *
 * 提供主站 PDO 映射建立与周期交换激活/停用的内部接口。
 */

#ifndef MASTER_PDO_MAPPING_H
#define MASTER_PDO_MAPPING_H

#include "master_error.h"

struct mo_ecat_master;

enum master_error_detail master_pdo_mapping_build(struct mo_ecat_master *master);

enum master_error_detail master_pdo_mapping_activate(struct mo_ecat_master *master);

enum master_error_detail master_pdo_mapping_deactivate(struct mo_ecat_master *master);

#endif /* MASTER_PDO_MAPPING_H */
