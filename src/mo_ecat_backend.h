#ifndef MO_ECAT_BACKEND_H
#define MO_ECAT_BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* ==================== Backend 工厂 ==================== */

int mo_ecat_backend_init(struct mo_ecat_backend *backend);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_BACKEND_H */
