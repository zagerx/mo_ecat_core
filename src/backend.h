#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "process_image.h"

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
                 const struct mo_ecat_master_options *options);

    /** 扫描总线并返回实际从站数量。open() 成功后调用。 */
    int  (*scan)(struct mo_ecat_backend *backend,
                 size_t *slave_count);

    /** 将 scan() 获得的从站基本信息写入核心层运行时数组。 */
    int  (*read_discovered_slaves)(struct mo_ecat_backend *backend,
                                   struct mo_ecat_slave *slaves,
                                   size_t slave_count);

    /**
     * 读取扫描到的从站默认 PDO 映射描述。
     * 该操作只读取 PDO 分配与映射对象，不建立过程数据映像。
     */
    int  (*read_pdo_entries)(struct mo_ecat_backend *backend,
                             struct mo_ecat_slave *slaves,
                             size_t slave_count);

    /** 完成 PDO 映射并回填运行时对象。scan() 成功后调用。 */
    int  (*configure)(struct mo_ecat_backend *backend,
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

int backend_init(struct mo_ecat_backend *backend);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
