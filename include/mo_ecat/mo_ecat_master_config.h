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
 *
 * 当前只包含网口名，后续所有产品级可变主站参数都应加在这里。
 */
struct mo_ecat_master_config {
    char interface_name[MO_ECAT_MAX_IFNAME_LEN + 1];
};

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_CONFIG_H */
