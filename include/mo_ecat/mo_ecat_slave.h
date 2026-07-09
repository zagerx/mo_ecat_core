#ifndef MO_ECAT_SLAVE_H
#define MO_ECAT_SLAVE_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @file mo_ecat_slave.h
 * @brief 从站信息与诊断接口
 */

/**
 * @brief 获取已配置从站数量
 */
size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master);

/**
 * @brief 获取指定从站信息
 */
const struct mo_ecat_slave *mo_ecat_master_get_slave(
    const struct mo_ecat_master *master, size_t index);

/**
 * @brief 读取从站诊断信息
 *
 * 后端会更新从站 AL 状态、online/operational/error 等字段。
 */
int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_SLAVE_H */
