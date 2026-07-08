#ifndef ECAT_MASTER_PRIV_H
#define ECAT_MASTER_PRIV_H

#include <stdint.h>

#include "soem/soem.h"
#include "statemachine/statemachine.h"
#include "mo_ecat/ecat_master.h"

#define EC_MASTER_MAX_GROUPS    1
#define EC_MASTER_IFNAME_SIZE   32
#define EC_MASTER_IOMAP_SIZE    4096

/** @brief 单个从站（内部类型） */
struct slave {
    struct ec_slave_info info;
};

/** @brief 从站组（内部类型） */
struct slave_group {
    struct slave *slaves;   /**< 根据扫描信息动态确定大小 */
    int           slave_count;
    char          name[32];
};

/** @brief EtherCAT 主站对象完整定义（仅内部使用） */
struct ec_master {
    struct statemachine       sm;           /**< 内部状态机 */
    enum ec_master_state      state_id;     /**< 当前主站状态 */

    ecx_contextt              context;      /**< SOEM 上下文 */
    char                      ifname[EC_MASTER_IFNAME_SIZE]; /**< 网卡名 */

    struct slave_group        group[EC_MASTER_MAX_GROUPS];

    uint8_t                  *iomap;        /**< 过程数据映像 */
    size_t                    iomap_size;   /**< IOmap 有效大小 */

    int                       dc_enabled;   /**< 是否启用 DC */
    int64_t                   dc_time;      /**< DC 参考时间 */
    int                       dc_ref_slave; /**< 参考时钟从站位置 */

    void                     *user_data;    /**< 用户私有数据 */
};

#endif /* ECAT_MASTER_PRIV_H */
