#ifndef BACKEND_OPS_H
#define BACKEND_OPS_H

#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file backend/backend_ops.h
 * @brief 后端 ops 表私有定义
 *
 * 本头文件只供 backend.c 和具体后端实现（如 soem.c）使用，
 * 核心层不应直接 include，也不应直接访问 ops 表成员。
 */

/**
 * 适配层到核心层的数据绑定/转换回调。
 *
 * 这些回调负责在发现、扫描、配置阶段把后端私有的从站/PDO/过程映像数据
 * 暴露或翻译成核心层能理解的结构。它们与后端生命周期回调分离，以便清晰
 * 标识依赖核心层类型的接口边界。
 */
struct backend_translation_ops {
    /** 将 scan() 获得的从站基本信息写入核心层运行时数组。 */
    int  (*translate_slave_info)(struct backend_instance *backend,
                                   struct mo_ecat_slave *slaves,
                                   size_t slave_count);

    /**
     * 读取扫描到的从站默认 PDO 映射描述。
     * 该操作只读取 PDO 分配与映射对象，不建立过程数据映像。
     */
    int  (*read_pdo_entries)(struct backend_instance *backend,
                             struct mo_ecat_slave *slaves,
                             size_t slave_count);

    /**
     * 根据适配层映射结果，填充核心层 PDO 引用数组的 offset 等字段。
     * refs 由核心层根据 slaves[].pdo_entries 预先构造好基础信息。
     */
    int  (*fill_pdo_refs)(struct backend_instance *backend,
                          struct mo_ecat_slave_pdo_ref *refs,
                          size_t ref_count,
                          uint32_t generation);

    /** 将适配层已映射好的过程映像信息绑定到核心层对象。 */
    int  (*get_process_image)(struct backend_instance *backend,
                              struct mo_ecat_process_image *image);

};

/**
 * 后端生命周期/运行时回调。
 */
struct backend_ops {
    int  (*open)(struct backend_instance *backend,
                 const struct mo_ecat_master_config *config);

    /** 扫描总线、加载从站基本信息到适配层，并返回实际从站数量。open() 成功后调用。 */
    int  (*load_slave_info)(struct backend_instance *backend,
                            size_t *slave_count);

    /** 完成 PDO 映射，只更新适配层内部状态，不直接填充核心层对象。 */
    int  (*configure)(struct backend_instance *backend);

    int  (*activate)(struct backend_instance *backend);

    int  (*cycle_begin)(struct backend_instance *backend,
                        struct mo_ecat_cycle_result *result);

    int  (*cycle_end)(struct backend_instance *backend,
                      struct mo_ecat_cycle_result *result);

    int  (*read_all_slave_states)(struct backend_instance *backend,
                                  struct mo_ecat_slave *slaves,
                                  size_t slave_count);

    int  (*read_single_slave_state)(struct backend_instance *backend,
                                    size_t slave_index,
                                    struct mo_ecat_slave_state *state);

    int  (*deactivate)(struct backend_instance *backend);
    void (*close)(struct backend_instance *backend);
};

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_OPS_H */
