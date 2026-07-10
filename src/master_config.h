#ifndef MASTER_CONFIG_H
#define MASTER_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "mo_ecat/mo_ecat_common.h"

#define MO_ECAT_MAX_SLAVES       16
#define MO_ECAT_MAX_PDO_ENTRIES  32

/** 后端 PDO 配置阶段使用的单项配置。 */
struct mo_ecat_pdo_entry_config {
    uint16_t index;
    uint8_t  subindex;
    uint8_t  bit_length;
    enum mo_ecat_pdo_direction direction;
};

/** 后端 PDO 配置阶段使用的从站配置。 */
struct mo_ecat_slave_config {
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    char name[MO_ECAT_MAX_NAME_LEN + 1];
    struct mo_ecat_pdo_entry_config pdo_entries[MO_ECAT_MAX_PDO_ENTRIES];
    size_t pdo_entry_count;
    int dc_active;
};

/**
 * 后端内部配置模型。
 *
 * 当前尚无公共接口接收该结构，后续配置 API 稳定后再以独立公共头发布。
 */
struct mo_ecat_user_config {
    struct mo_ecat_slave_config slaves[MO_ECAT_MAX_SLAVES];
    size_t slave_count;
};

#endif /* MASTER_CONFIG_H */
