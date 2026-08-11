/*
 * mo_ecat_master_config.h - 主站配置
 *
 * 配置结构由核心库公开，配置实例由应用层持有并保证唯一；
 * 主站只在创建时记录指针，不复制内容。
 */

#ifndef MO_ECAT_MASTER_CONFIG_H
#define MO_ECAT_MASTER_CONFIG_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct mo_ecat_master_config - 主站配置
 * @interface_name: EtherCAT 网口名
 * @sync0_cycle_ns: Sync0 周期（ns），0 表示不激活 Sync0
 * @sync0_shift_ns: Sync0 相位偏移（ns）
 *
 * 主站进入 RUNNING 后对所有支持 DC 的从站统一激活 Sync0；
 * DEACTIVATE 时统一关闭。
 */
struct mo_ecat_master_config {
    char interface_name[MO_ECAT_MAX_IFNAME_LEN + 1];
    uint32_t sync0_cycle_ns;
    int32_t sync0_shift_ns;
};

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_CONFIG_H */
