/*
 * pdo_layout.h - PDO 数据映像布局内部接口
 *
 * 提供 PDO 映射建立与周期交换激活/停用的内部接口。
 */

#ifndef PDO_LAYOUT_H
#define PDO_LAYOUT_H

#include "master_error.h"

struct mo_ecat_master;

enum master_error_detail pdo_layout_build(struct mo_ecat_master *master);

enum master_error_detail pdo_layout_activate(struct mo_ecat_master *master);

enum master_error_detail pdo_layout_deactivate(struct mo_ecat_master *master);

#endif /* PDO_LAYOUT_H */
