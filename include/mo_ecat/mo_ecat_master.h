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
 * @brief 初始化主站对象
 *
 * 该函数只初始化对象字段、绑定配置指针并初始化内部状态机，
 * 不打开后端、不配置总线。config 由调用者保证生命周期。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_init(struct mo_ecat_master *master,
                        const struct mo_ecat_config *config);

/**
 * @brief 反初始化主站对象
 *
 * 释放运行期资源并关闭后端，不释放 master 对象本身。
 */
void mo_ecat_master_deinit(struct mo_ecat_master *master);

/**
 * @brief 获取并初始化单主站对象
 *
 * 单主站场景下，该函数返回核心库内部的静态主站实例，
 * 并调用 mo_ecat_master_init() 完成字段初始化和配置指针绑定。
 * 重复调用会失败并返回 NULL。
 */
struct mo_ecat_master *mo_ecat_master_create(
    const struct mo_ecat_config *config);

/**
 * @brief 销毁主站对象
 *
 * 销毁前会自动释放后端资源并关闭后端。该函数不会释放 master 存储本身。
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
 * @return 0 表示命令已接受，非 0 表示拒绝
 */
int mo_ecat_master_configure(struct mo_ecat_master *master);

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
