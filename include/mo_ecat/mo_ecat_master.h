#ifndef MO_ECAT_MASTER_H
#define MO_ECAT_MASTER_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @brief 创建主站对象
 */
struct mo_ecat_master *mo_ecat_master_create(void);

/**
 * @brief 销毁主站对象
 *
 * 处于 ACTIVE / CONFIGURED 状态时会先关闭后端并释放资源。
 */
void mo_ecat_master_destroy(struct mo_ecat_master *master);

/**
 * @brief 配置主站
 *
 * 该函数会打开后端、扫描从站并建立 PDO 映射。
 *
 * @param master  主站对象
 * @param config  顶层配置
 * @param backend 后端实例，由调用者分配并初始化
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_configure(struct mo_ecat_master *master,
                             const struct mo_ecat_config *config,
                             struct mo_ecat_backend *backend);

/**
 * @brief 激活过程数据周期
 */
int mo_ecat_master_activate(struct mo_ecat_master *master);

/**
 * @brief 停用过程数据周期
 */
int mo_ecat_master_deactivate(struct mo_ecat_master *master);

/**
 * @brief 周期开始（接收）
 */
int mo_ecat_master_cycle_begin(struct mo_ecat_master *master,
                               struct mo_ecat_cycle_result *result);

/**
 * @brief 周期结束（发送）
 */
int mo_ecat_master_cycle_end(struct mo_ecat_master *master,
                             struct mo_ecat_cycle_result *result);

/**
 * @brief 读取从站诊断信息
 */
int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master);

/**
 * @brief 获取主站状态
 */
enum mo_ecat_master_state mo_ecat_master_get_state(
    const struct mo_ecat_master *master);

/**
 * @brief 获取从站数量
 */
size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master);

/**
 * @brief 获取指定从站信息
 */
const struct mo_ecat_slave *mo_ecat_master_get_slave(
    const struct mo_ecat_master *master, size_t index);

/**
 * @brief 获取 PDO 引用总数
 */
size_t mo_ecat_master_get_pdo_ref_count(const struct mo_ecat_master *master);

/**
 * @brief 获取 PDO 引用
 */
const struct mo_ecat_pdo_ref *mo_ecat_master_get_pdo_ref(
    const struct mo_ecat_master *master, size_t index);

/**
 * @brief 获取过程数据区指针与大小
 */
int mo_ecat_master_get_process_image(const struct mo_ecat_master *master,
                                     const uint8_t **memory,
                                     size_t *size);

/**
 * @brief 获取输入 PDO 在过程数据中的地址
 */
const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                              const struct mo_ecat_pdo_ref *ref);

/**
 * @brief 获取输出 PDO 在过程数据中的地址
 */
void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_ref *ref);

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
