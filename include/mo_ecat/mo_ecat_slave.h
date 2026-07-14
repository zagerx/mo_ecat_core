#ifndef MO_ECAT_SLAVE_H
#define MO_ECAT_SLAVE_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

#define MO_ECAT_MAX_PDO_ENTRIES 32
#define MO_ECAT_MAX_SM          8
#define MO_ECAT_MAX_FMMU        4

enum mo_ecat_slave_al_state {
    MO_ECAT_AL_STATE_INIT,
    MO_ECAT_AL_STATE_PRE_OP,
    MO_ECAT_AL_STATE_SAFE_OP,
    MO_ECAT_AL_STATE_OP,
    MO_ECAT_AL_STATE_BOOTSTRAP,
    MO_ECAT_AL_STATE_UNKNOWN
};

struct mo_ecat_slave_state {
    enum mo_ecat_slave_al_state al_state;
    int error;
    uint16_t al_status_code;
    int online;
    int operational;
};

/** 从站默认 PDO 映射项。 */
struct mo_ecat_slave_pdo_entry {
    uint16_t pdo_index;
    uint16_t object_index;
    uint8_t object_subindex;
    uint8_t bit_length;
    enum mo_ecat_pdo_direction direction;
};

/** 从站邮箱参数，主要供诊断与维护功能查询。 */
struct mo_ecat_slave_mailbox {
    uint16_t protocol;
    uint16_t write_address;
    uint16_t write_size;
    uint16_t read_address;
    uint16_t read_size;
};

/** 从站 Sync Manager 信息。 */
struct mo_ecat_slave_sync_manager {
    uint16_t start_address;
    uint16_t length;
    uint32_t flags;
    uint8_t type;
};

/** 从站 FMMU 功能分配。 */
struct mo_ecat_slave_fmmu {
    uint8_t function;
};

/** 从站基础信息。
 *
 * 扫描阶段即可确定，运行期间基本不变。
 */
struct mo_ecat_slave_base_info {
    uint16_t position;
    uint16_t alias;
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    char name[MO_ECAT_MAX_NAME_LEN + 1];
    int has_dc;
    uint32_t propagation_delay_ns;
    struct mo_ecat_slave_mailbox mailbox;
    int has_coe;
    int has_foe;
    int has_eoe;
    int has_soe;
    struct mo_ecat_slave_sync_manager sm[MO_ECAT_MAX_SM];
    struct mo_ecat_slave_fmmu fmmu[MO_ECAT_MAX_FMMU];
};

/** 扫描得到的从站运行时信息。 */
struct mo_ecat_slave {
    struct mo_ecat_slave_base_info base_info;
    struct mo_ecat_slave_state state;
    struct mo_ecat_slave_pdo_entry pdo_entries[MO_ECAT_MAX_PDO_ENTRIES];
    size_t pdo_entry_count;
};

/**
 * @brief 从站信息的公开只读视图
 *
 * 由 mo_ecat_master_get_slave_info() 复制返回，避免向应用层暴露内部数组指针。
 */
struct mo_ecat_slave_info {
    struct mo_ecat_slave_base_info base_info;
    size_t pdo_entry_count;
    struct mo_ecat_slave_state state; /**< 从站运行时诊断状态 */
};

/**
 * @file mo_ecat_slave.h
 * @brief 从站信息与诊断接口
 */

/**
 * @brief 获取已发现从站数量
 */
size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master);

/**
 * @brief 获取指定从站信息的只读副本
 *
 * 调用方提供 info 缓冲区，核心在锁保护下复制数据。返回的指针不指向内部数组，
 * 因此在调用返回后即使发生状态迁移也不会悬空。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_slave_info(const struct mo_ecat_master *master,
                                  size_t index, struct mo_ecat_slave_info *info);

/**
 * @brief 读取从站诊断信息
 *
 * 后端会更新从站 AL 状态、online/operational/error 等字段。
 */
int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_SLAVE_H */
