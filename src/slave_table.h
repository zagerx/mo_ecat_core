/*
 * slave_table.h - 从站表内部接口
 *
 * 提供根据后端扫描结果构建与刷新主站内部从站表的内部接口。
 */

#ifndef SLAVE_TABLE_H
#define SLAVE_TABLE_H

#include "master_error.h"

struct mo_ecat_master;

enum master_error_detail slave_table_build(struct mo_ecat_master *master,
					   size_t slave_count);

enum master_error_detail slave_table_refresh_states(struct mo_ecat_master *master);

#endif /* SLAVE_TABLE_H */
