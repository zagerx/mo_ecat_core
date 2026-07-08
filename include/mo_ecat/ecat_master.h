#ifndef MO_ECAT_ECAT_MASTER_H
#define MO_ECAT_ECAT_MASTER_H

#include <stdint.h>
#include <stddef.h>

#include "soem/soem.h"
#include "statemachine/statemachine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EC_MASTER_MAX_GROUPS    1
#define EC_MASTER_IFNAME_SIZE   32
#define EC_MASTER_IOMAP_SIZE    4096

/** @brief 主站应用层状态 */
typedef enum {
    EC_MASTER_STATE_INIT = 0,       /**< 初始：尚未扫描总线 */
    EC_MASTER_STATE_READY,          /**< 准备：已扫描/映射，等待启动 */
    EC_MASTER_STATE_RUNNING,        /**< 运行：周期过程数据运行中 */
    EC_MASTER_STATE_FAULT,          /**< 故障 */
    EC_MASTER_STATE_CONTROLLED,     /**< 受控：被外部控制器接管 */
    EC_MASTER_STATE_COUNT
} ec_master_state_id_t;

/** @brief 从站静态信息 */
struct slave_info {
    uint16_t position;
    uint16_t alias;
    uint32_t vendor_id;
    uint32_t product_code;
    uint16_t state;
    char     name[EC_MAXNAME + 1];
};

/** @brief 单个从站 */
struct slave {
    struct slave_info info;
};

/** @brief 从站组 */
struct slave_group {
    struct slave *slaves;   /**< 根据扫描信息动态确定大小 */
    int           slave_count;
    char          name[32];
};

/** @brief EtherCAT 主站对象 */
typedef struct ec_master {
    struct statemachine    sm;           /**< 内部状态机 */
    ec_master_state_id_t   state_id;     /**< 当前主站状态 */

    ecx_contextt           context;      /**< SOEM 上下文 */
    char                   ifname[EC_MASTER_IFNAME_SIZE]; /**< 网卡名 */

    struct slave_group     group[EC_MASTER_MAX_GROUPS];

    uint8_t               *iomap;        /**< 过程数据映像 */
    size_t                 iomap_size;   /**< IOmap 有效大小 */

    int                    dc_enabled;   /**< 是否启用 DC */
    int64_t                dc_time;      /**< DC 参考时间 */
    int                    dc_ref_slave; /**< 参考时钟从站位置 */

    void                  *user_data;    /**< 用户私有数据 */
} ec_master_t;

/**
 * @brief 创建主站对象
 * @param ifname 网卡接口名，如 "eth0"
 * @return 成功返回主站指针，失败返回 NULL
 */
ec_master_t *ec_master_create(const char *ifname);

/**
 * @brief 销毁主站对象
 * @param master 主站对象
 */
void ec_master_destroy(ec_master_t *master);

/**
 * @brief 启动主站状态机（触发 init 状态）
 * @param master 主站对象
 * @return 0 表示启动成功（异步），<0 表示失败
 */
int ec_master_start(ec_master_t *master);

/**
 * @brief 停止主站周期运行
 * @param master 主站对象
 */
void ec_master_stop(ec_master_t *master);

/**
 * @brief 调度一次主站状态机
 * @details 应在固定周期调用，例如 1ms。
 *          内部会处理状态切换、SOEM 收发等。
 * @param master 主站对象
 */
void ec_master_run_cycle(ec_master_t *master);

/**
 * @brief 获取当前主站状态
 * @param master 主站对象
 * @return 当前状态 ID
 */
ec_master_state_id_t ec_master_get_state(const ec_master_t *master);

/**
 * @brief 获取指定从站信息
 * @param master   主站对象
 * @param position 从站位置（SOEM 从 1 开始）
 * @param info     输出缓冲区
 * @return 0 成功，<0 失败
 */
int ec_master_get_slave_info(const ec_master_t *master, uint16_t position,
                             struct slave_info *info);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_ECAT_MASTER_H */
