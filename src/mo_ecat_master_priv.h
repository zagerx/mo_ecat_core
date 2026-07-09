#ifndef MO_ECAT_MASTER_PRIV_H
#define MO_ECAT_MASTER_PRIV_H

#include <stddef.h>
#include <pthread.h>

#include "mo_ecat/mo_ecat_types.h"
#include "common/statemachine/statemachine.h"
#include "mo_ecat_master_states.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 主站支持的命令类型
 */
enum mo_ecat_master_command {
    MO_ECAT_MASTER_CMD_NONE,       /**< 无命令 */
    MO_ECAT_MASTER_CMD_CONFIGURE,  /**< 配置总线 */
    MO_ECAT_MASTER_CMD_ACTIVATE,   /**< 激活周期 */
    MO_ECAT_MASTER_CMD_DEACTIVATE, /**< 停用周期 */
    MO_ECAT_MASTER_CMD_RESET       /**< 复位到空闲 */
};

/**
 * @brief 单条命令槽
 *
 * 核心层一次只处理一条命令。调用者提交命令后，由后台 dispatch 线程
 * （或应用显式调用 dispatch）在合适的状态下执行。
 */
struct mo_ecat_master_cmd {
    enum mo_ecat_master_command id; /**< 当前命令 */
    int pending;                    /**< 是否有未处理命令 */
    int result;                     /**< 上条命令的处理结果 */
    const struct mo_ecat_config *pending_config;   /**< CONFIGURE 携带的配置 */
    struct mo_ecat_backend pending_backend_value;  /**< CONFIGURE 携带的后端副本 */
};

/**
 * @brief 周期运行状态
 *
 * 保存最近一次周期结果以及连续异常计数，用于核心层判断
 * 是否需要进入 DEGRADED 或 FAULT。
 */
struct mo_ecat_master_cycle {
    int result_pending;            /**< 是否有待状态机消费的周期结果 */
    int abnormal;                  /**< 最近一次周期是否异常 */
    unsigned int consecutive_errors; /**< 连续异常次数 */
    struct mo_ecat_cycle_result last; /**< 最近一次周期结果 */
};

/**
 * @brief 运行时资源
 *
 * 包含当前挂载的后端、已深拷贝的配置以及过程数据映像。
 */
struct mo_ecat_master_runtime {
    struct mo_ecat_backend backend;     /**< 后端实例 */
    struct mo_ecat_config config;       /**< 配置副本 */
    struct mo_ecat_process_image image; /**< 过程数据映像 */
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
 * @brief 主站对象（核心层内部定义）
 */
struct mo_ecat_master {
    struct statemachine sm;                /**< 底层状态机 */

    struct mo_ecat_master_cmd cmd;         /**< 命令槽 */
    struct mo_ecat_master_runtime rt;      /**< 运行时资源 */
    struct mo_ecat_master_diagnostics diag;/**< 从站诊断 */
    struct mo_ecat_master_pdo pdo;         /**< PDO 引用 */
    struct mo_ecat_master_cycle cycle;     /**< 周期运行状态 */

    pthread_mutex_t lock;                  /**< 保护本对象的互斥锁 */
    void *user_data;                       /**< 用户私有数据 */
};

/* 内部辅助函数，供状态机与核心模块使用 */
int mo_ecat_master_prepare_config(struct mo_ecat_master *master,
                                  const struct mo_ecat_config *config,
                                  struct mo_ecat_backend *backend);
int mo_ecat_master_backend_configure(struct mo_ecat_master *master);
int mo_ecat_master_backend_activate(struct mo_ecat_master *master);
int mo_ecat_master_backend_deactivate(struct mo_ecat_master *master);
void mo_ecat_master_release_resources(struct mo_ecat_master *master);
void mo_ecat_master_clear_command(struct mo_ecat_master *master, int result);
int mo_ecat_master_take_cycle_result(struct mo_ecat_master *master,
                                     int *abnormal);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_PRIV_H */
