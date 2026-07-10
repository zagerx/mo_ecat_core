#ifndef MASTER_PRIV_H
#define MASTER_PRIV_H

#include <stddef.h>
#include <pthread.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_types.h"
#include "backend.h"
#include "common/statemachine/statemachine.h"
#include "master_states.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 周期运行状态
 *
 * 保存最近一次周期结果以及连续异常计数，用于核心层判断
 * 记录最近一次周期结果。
 */
struct mo_ecat_master_cycle {
    struct mo_ecat_cycle_result last; /**< 最近一次周期结果 */
};

/**
 * @brief 从站诊断信息
 *
 * 从站静态信息与最近一次诊断状态。
 */
struct mo_ecat_master_diagnostics {
    struct mo_ecat_slave *slaves;       /**< 从站信息数组 */
    struct mo_ecat_slave_state *states; /**< 从站状态数组 */
    size_t count;                       /**< 从站数量 */
};

/**
 * @brief PDO 引用表
 *
 * 由配置展开得到的扁平 PDO 引用数组，后端负责填写 offset。
 */
struct mo_ecat_master_pdo {
    struct mo_ecat_pdo_ref *refs; /**< PDO 引用数组 */
    size_t count;                 /**< PDO 引用数量 */
};

/**
 * @brief 配置期一次性分配的运行资源块
 */
struct mo_ecat_master_runtime_memory {
    void *memory;                 /**< 从站和诊断状态共用内存块 */
    size_t size;                  /**< 内存块大小 */
};

/**
 * @brief 主站对象（核心层内部定义）
 */
struct mo_ecat_master {
    struct statemachine sm;                /**< 底层状态机 */

    enum mo_ecat_master_cmd command;       /**< 当前请求命令 */
    struct mo_ecat_backend backend;        /**< 后端实例 */
    struct mo_ecat_master_options options; /**< 主站启动选项 */
    struct mo_ecat_process_image image;    /**< 过程数据映像 */
    struct mo_ecat_master_diagnostics diag;/**< 从站诊断 */
    struct mo_ecat_master_pdo pdo;         /**< PDO 引用 */
    struct mo_ecat_master_runtime_memory runtime_memory; /**< 运行资源 */
    struct mo_ecat_master_cycle cycle;     /**< 周期运行状态 */

    pthread_mutex_t lock;                  /**< 保护本对象的互斥锁 */
    void *user_data;                       /**< 用户私有数据 */
};

/* 内部辅助函数，供状态机与核心模块使用 */
enum mo_ecat_master_state
master_state_from_sm(const struct mo_ecat_master *master);
enum mo_ecat_master_cmd master_read_cmd(const struct mo_ecat_master *master);
void master_write_cmd(struct mo_ecat_master *master,
                      enum mo_ecat_master_cmd cmd);
int master_backend_open(struct mo_ecat_master *master);
int master_scan(struct mo_ecat_master *master, size_t *slave_count);
int master_build_topology(struct mo_ecat_master *master, size_t slave_count);
int master_read_pdo_entries(struct mo_ecat_master *master);
void master_release_resources(struct mo_ecat_master *master);
void master_clear_cmd(struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MASTER_PRIV_H */
