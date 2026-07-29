#ifndef CLI_PRINT_H
#define CLI_PRINT_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_topology.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;
struct robot;

/**
 * @file print.h
 * @brief CLI 输出辅助函数
 */

/**
 * @brief 把主站状态枚举转成可读字符串
 */
const char *state_name(enum mo_ecat_master_state state);

/**
 * @brief 打印当前主站状态和最近一次周期结果
 */
void print_state(struct mo_ecat_master *master);

/**
 * @brief 打印从站诊断信息
 */
void print_diagnostics(struct mo_ecat_master *master);

/**
 * @brief 打印机器人实例
 */
void print_robot(const struct robot *robot);

/**
 * @brief 打印单个 PDO entry 信息
 */
void print_pdo_entry(struct mo_ecat_master *master, size_t idx);

/**
 * @brief 打印命令帮助
 */
void print_help(void);

#ifdef __cplusplus
}
#endif

#endif /* CLI_PRINT_H */
