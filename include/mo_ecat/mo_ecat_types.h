#ifndef MO_ECAT_TYPES_H
#define MO_ECAT_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MO_ECAT_MAX_NAME_LEN 80

/* ==================== 主站生命周期状态 ==================== */

enum mo_ecat_master_state {
    MO_ECAT_MASTER_STATE_CLOSED,
    MO_ECAT_MASTER_STATE_OPENED,
    MO_ECAT_MASTER_STATE_CONFIGURED,
    MO_ECAT_MASTER_STATE_ACTIVE,
    MO_ECAT_MASTER_STATE_DEGRADED,
    MO_ECAT_MASTER_STATE_FAULT
};

/* ==================== AL 状态 ==================== */

enum mo_ecat_al_state {
    MO_ECAT_AL_STATE_INIT,
    MO_ECAT_AL_STATE_PRE_OP,
    MO_ECAT_AL_STATE_SAFE_OP,
    MO_ECAT_AL_STATE_OP,
    MO_ECAT_AL_STATE_BOOTSTRAP,
    MO_ECAT_AL_STATE_UNKNOWN
};

struct mo_ecat_slave_state {
    enum mo_ecat_al_state al_state;
    int                   error;
    uint16_t              al_status_code;
    int                   online;
    int                   operational;
};

/* ==================== 配置模型 ==================== */

enum mo_ecat_pdo_direction {
    MO_ECAT_PDO_INPUT,
    MO_ECAT_PDO_OUTPUT
};

struct mo_ecat_pdo_entry_config {
    uint16_t index;
    uint8_t  subindex;
    uint8_t  bit_length;
    enum mo_ecat_pdo_direction direction;
};

struct mo_ecat_dc_config {
    int      enabled;
    uint32_t assign_activate;
    uint32_t sync0_cycle_ns;
    int32_t  sync0_shift_ns;
};

struct mo_ecat_slave_config {
    uint16_t alias;
    uint16_t position;

    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;

    const char *name;

    const struct mo_ecat_pdo_entry_config *pdo_entries;
    size_t pdo_entry_count;

    int dc_active;
};

struct mo_ecat_config {
    const char *interface_name;

    const struct mo_ecat_slave_config *slaves;
    size_t slave_count;
};

/* ==================== 运行时类型 ==================== */

struct mo_ecat_process_image {
    uint8_t *memory;
    size_t   size;
    uint32_t generation;
    int      active;
};

struct mo_ecat_pdo_ref {
    size_t   slave_index;
    uint16_t pdo_index;
    uint16_t entry_index;

    uint32_t byte_offset;
    uint8_t  bit_offset;
    uint8_t  bit_length;
    enum mo_ecat_pdo_direction direction;
    uint32_t generation;
};

struct mo_ecat_slave {
    uint16_t position;
    uint16_t alias;

    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;

    char     name[MO_ECAT_MAX_NAME_LEN + 1];
    int      has_dc;
    uint32_t propagation_delay_ns;

    struct mo_ecat_slave_state state;
};

struct mo_ecat_cycle_result {
    int      backend_error;
    int      link_up;

    uint32_t expected_wkc;
    uint32_t actual_wkc;

    int64_t  dc_time_ns;
    int      dc_time_valid;

    int      diagnostics_required;
};

/* ==================== Backend 能力 ==================== */

struct mo_ecat_backend_caps {
    int online_discovery;
    int dynamic_pdo_mapping;
    int dc_support;
    int redundancy;
    int manual_state_control;
};

/* ==================== Backend 契约 ==================== */

struct mo_ecat_backend;          /* 前向声明，用于 ops 函数指针 */
struct mo_ecat_discovery_ops;
struct mo_ecat_manual_state_ops;

struct mo_ecat_backend_ops {
    const char *name;

    int  (*open)(struct mo_ecat_backend *backend,
                 const struct mo_ecat_config *config);

    int  (*configure)(struct mo_ecat_backend *backend,
                      const struct mo_ecat_config *config,
                      struct mo_ecat_process_image *image,
                      struct mo_ecat_pdo_ref *pdo_refs,
                      size_t pdo_ref_count,
                      struct mo_ecat_slave *slaves,
                      size_t slave_count);

    int  (*activate)(struct mo_ecat_backend *backend);

    int  (*cycle_begin)(struct mo_ecat_backend *backend,
                        struct mo_ecat_cycle_result *result);

    int  (*cycle_end)(struct mo_ecat_backend *backend,
                      struct mo_ecat_cycle_result *result);

    int  (*read_diagnostics)(struct mo_ecat_backend *backend,
                             struct mo_ecat_slave_state *states,
                             size_t state_count);

    int  (*deactivate)(struct mo_ecat_backend *backend);
    void (*close)(struct mo_ecat_backend *backend);
};

struct mo_ecat_backend {
    const struct mo_ecat_backend_ops *ops;
    const struct mo_ecat_discovery_ops *discovery_ops;
    const struct mo_ecat_manual_state_ops *manual_state_ops;
    struct mo_ecat_backend_caps caps;
    void *ctx;
};

/* 前向声明 */
struct mo_ecat_master;

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_TYPES_H */
