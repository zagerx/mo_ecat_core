/*
 * master_pdo_layout.h - 主站 PDO 数据映像布局内部接口
 *
 * 提供主站 PDO 映射建立与周期交换激活/停用的内部接口。
 */

#ifndef MASTER_PDO_LAYOUT_H
#define MASTER_PDO_LAYOUT_H

#include "master_error.h"

struct mo_ecat_master;

enum master_error_detail master_pdo_layout_build(struct mo_ecat_master *master);

enum master_error_detail master_pdo_layout_activate(struct mo_ecat_master *master);

enum master_error_detail master_pdo_layout_deactivate(struct mo_ecat_master *master);

#endif /* MASTER_PDO_LAYOUT_H */
