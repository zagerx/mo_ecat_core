#ifndef MO_ECAT_MASTER_H
#define MO_ECAT_MASTER_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @file mo_ecat_master.h
 * @brief 主站生命周期与调度接口
 *
 * 本头文件只包含主站最核心的生命周期函数。
 * 状态查询、从站信息、PDO 访问分别位于独立头文件。
 */

/**
 * @brief 创建并配置主站对象
 *
 * 创建主站对象的同时完成后端初始化与总线配置。成功后主站处于 READY
 * 状态，调用者可直接提交激活命令。
 *
 * @param config 顶层配置
 * @return 成功返回主站对象，失败返回 NULL
 */
struct mo_ecat_master *mo_ecat_master_create(
    const struct mo_ecat_config *config);

/**
 * @brief 销毁主站对象
 *
 * 销毁前会自动释放后端资源并关闭后端。
 */
void mo_ecat_master_destroy(struct mo_ecat_master *master);

/**
 * @brief 调度主站状态机
 *
 * 应由后台线程以固定周期调用。该函数会处理 pending 的状态迁移、
 * 命令执行以及周期结果消费。
 */
void mo_ecat_master_dispatch(struct mo_ecat_master *master);

/**
 * @brief 提交配置命令
 *
 * 该函数只把 CONFIGURE 命令放入命令槽，实际配置工作由后续
 * mo_ecat_master_dispatch() 在 IDLE 状态执行。
 *
 * 核心层会创建并持有后端实例，调用者无需手动管理 backend 生命周期。
 *
 * @param master 主站对象
 * @param config 顶层配置，命令执行前必须保持有效
 * @return 0 表示命令已接受，非 0 表示拒绝
 */
int mo_ecat_master_configure(struct mo_ecat_master *master,
                             const struct mo_ecat_config *config);

/**
 * @brief 提交激活命令
 *
 * 仅在 READY 状态下有效。
 */
int mo_ecat_master_activate(struct mo_ecat_master *master);

/**
 * @brief 提交停用命令
 *
 * 仅在 RUNNING / DEGRADED 状态下有效。
 */
int mo_ecat_master_deactivate(struct mo_ecat_master *master);

/**
 * @brief 提交复位命令
 *
 * 释放后端资源并回到 IDLE 状态。
 */
int mo_ecat_master_reset(struct mo_ecat_master *master);

/**
 * @brief 设置用户数据
 */
void mo_ecat_master_set_user_data(struct mo_ecat_master *master,
                                  void *user_data);

/**
 * @brief 获取用户数据
 */
void *mo_ecat_master_get_user_data(const struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_H */
