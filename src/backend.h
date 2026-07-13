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

/* ==================== Backend 契约 ==================== */

struct mo_ecat_backend;          /* 前向声明，用于 ops 函数指针 */

/**
 * 适配层到核心层的数据绑定/转换回调。
 *
 * 这些回调负责在发现、扫描、配置阶段把后端私有的从站/PDO/过程映像数据
 * 暴露或翻译成核心层能理解的结构。它们与后端生命周期回调分离，以便清晰
 * 标识依赖核心层类型的接口边界。
 */
struct mo_ecat_backend_translation_ops {
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

    /**
     * 根据适配层映射结果，填充核心层 PDO 引用数组的 offset 等字段。
     * refs 由核心层根据 slaves[].pdo_entries 预先构造好基础信息。
     */
    int  (*fill_pdo_refs)(struct mo_ecat_backend *backend,
                          struct mo_ecat_pdo_ref *refs,
                          size_t ref_count,
                          uint32_t generation);

    /** 将适配层已映射好的过程映像信息绑定到核心层对象。 */
    int  (*get_process_image)(struct mo_ecat_backend *backend,
                              struct mo_ecat_process_image *image);

    /** 将适配层最新从站静态信息回填到核心层从站数组。 */
    int  (*fill_slave_info)(struct mo_ecat_backend *backend,
                            struct mo_ecat_slave *slaves,
                            size_t slave_count);
};

struct mo_ecat_backend_ops {
    int  (*open)(struct mo_ecat_backend *backend,
                 const struct mo_ecat_master_config *config);

    /** 扫描总线并返回实际从站数量。open() 成功后调用。 */
    int  (*scan)(struct mo_ecat_backend *backend,
                 size_t *slave_count);

    /** 完成 PDO 映射，只更新适配层内部状态，不直接填充核心层对象。 */
    int  (*configure)(struct mo_ecat_backend *backend);

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
    const char *name;
    const struct mo_ecat_backend_ops *ops;
    const struct mo_ecat_backend_translation_ops *translation_ops;
    void *ctx;
};

/* ==================== Backend 工厂 ==================== */

int backend_init(struct mo_ecat_backend *backend);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
