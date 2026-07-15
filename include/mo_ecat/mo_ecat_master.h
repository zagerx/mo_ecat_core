#ifndef MO_ECAT_MASTER_H
#define MO_ECAT_MASTER_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @brief 主站状态机请求命令
 */
enum mo_ecat_master_cmd {
    MO_ECAT_MASTER_CMD_NONE,         /**< 无命令 */
    MO_ECAT_MASTER_CMD_SCAN,         /**< 扫描总线 */
    MO_ECAT_MASTER_CMD_CONFIGURE,    /**< 配置 DC 并建立 PDO 映射 */
    MO_ECAT_MASTER_CMD_ACTIVATE,     /**< 激活周期运行 */
    MO_ECAT_MASTER_CMD_DEACTIVATE,   /**< 停止周期运行 */
    MO_ECAT_MASTER_CMD_RESET          /**< 复位到空闲 */
};

/**
 * @brief 主站最近一次进入 FAULT 的原因
 */
enum mo_ecat_master_error {
    MO_ECAT_MASTER_ERROR_NONE,
    MO_ECAT_MASTER_ERROR_DISCOVER_FAILED,
    MO_ECAT_MASTER_ERROR_READ_DEFAULT_PDO_FAILED,
    MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED,
    MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_MAPPING_FAILED,
    MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED,
    MO_ECAT_MASTER_ERROR_BUS_FAULT,
};

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
 * 该函数只初始化对象字段、绑定配置并初始化内部状态机，
 * 不打开后端、不配置总线。config 由调用者保证生命周期。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_init(struct mo_ecat_master *master,
                        const struct mo_ecat_master_config *config);

/**
 * @brief 反初始化主站对象
 *
 * 释放运行期资源并关闭后端，不释放 master 对象本身。
 */
void mo_ecat_master_deinit(struct mo_ecat_master *master);

/**
 * @brief 创建并初始化单主站对象
 *
 * 该函数为主站对象分配内存，并调用 mo_ecat_master_init() 完成
 * 字段初始化和配置指针绑定。单主站场景下，重复调用会失败并返回 NULL。
 */
struct mo_ecat_master *mo_ecat_master_create(
    const struct mo_ecat_master_config *config);

/**
 * @brief 销毁主站对象
 *
 * 销毁前会自动释放后端资源并关闭后端，并释放由
 * mo_ecat_master_create() 分配的主站对象内存。
 */
void mo_ecat_master_destroy(struct mo_ecat_master *master);

/**
 * @brief 调度主站状态机
 *
 * 应由后台线程以固定周期调用。该函数会处理状态迁移和命令执行。
 */
void mo_ecat_master_dispatch(struct mo_ecat_master *master);

/**
 * @brief 写入主站状态机命令
 *
 * 该函数只写入一条状态机请求，不直接执行后端动作。新命令可以覆盖
 * 尚未消费的旧命令；核心库不保证命令可靠投递，也不保存异步执行结果。
 *
 * @param master 主站对象
 * @param cmd 请求命令；不允许写入 MO_ECAT_MASTER_CMD_NONE
 * @return 0 表示命令已写入，非 0 表示参数无效
 */
int mo_ecat_master_write_cmd(struct mo_ecat_master *master,
                             enum mo_ecat_master_cmd cmd);

/**
 * @brief 获取主站最近一次进入 FAULT 的原因
 *
 * 该返回值只在状态为 FAULT 时具有诊断意义；命令未被接受时不会更新。
 */
enum mo_ecat_master_error mo_ecat_master_get_error_code(
    const struct mo_ecat_master *master);

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
