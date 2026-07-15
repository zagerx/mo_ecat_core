/*
 * backend/backend_ops.h - 后端 ops 表私有定义
 *
 * 本头文件只供 backend.c 和具体后端实现（如 soem.c）使用，
 * 核心层不应直接 include，也不应直接访问 ops 表成员。
 */

#ifndef BACKEND_OPS_H
#define BACKEND_OPS_H

#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct backend_translation_ops - 适配层到核心层的数据绑定/转换回调
 * @translate_slave_info: 将 scan() 获得的从站基本信息写入核心层运行时数组
 * @read_pdo_entries: 读取扫描到的从站默认 PDO 映射描述
 * @get_pdo_image: 将适配层已映射好的 PDO 数据区域绑定到核心层对象
 *
 * 这些回调负责在发现、扫描、配置阶段把后端私有的从站/PDO/PDO 数据区域
 * 暴露或翻译成核心层能理解的结构。它们与后端生命周期回调分离，以便清晰
 * 标识依赖核心层类型的接口边界。
 */
struct backend_translation_ops {
    int  (*translate_slave_info)(struct backend_instance *backend,
                                   struct master_slave *slaves,
                                   size_t slave_count);

    /**
     * read_pdo_entries - 读取扫描到的从站默认 PDO 映射描述
     * @backend: 后端实例指针
     * @slaves: 核心层从站数组
     * @slave_count: 从站数量
     *
     * 该操作只读取 PDO 分配与映射对象，不建立过程数据映像。
     *
     * Return: 0 成功，非 0 失败
     */
    int  (*read_pdo_entries)(struct backend_instance *backend,
                             struct master_slave *slaves,
                             size_t slave_count);

    /**
     * get_pdo_image - 将适配层已映射好的 PDO 数据区域绑定到核心层对象
     * @backend: 后端实例指针
     * @image: 用于返回 PDO 数据映像的指针
     *
     * Return: 0 成功，非 0 失败
     */
    int  (*get_pdo_image)(struct backend_instance *backend,
                          struct master_pdo_image *image);

};

/**
 * struct backend_ops - 后端生命周期/运行时回调
 * @open: 打开后端
 * @load_slave_info: 扫描总线、加载从站基本信息到适配层，并返回实际从站数量
 * @configure_dc: 配置分布式时钟，只更新适配层内部状态
 * @build_pdo_mapping: 建立 PDO 映射并填充每个 PDO entry 的 byte/bit offset
 * @activate: 激活后端周期通信
 * @cyclic_receive: 后端周期接收
 * @cyclic_send: 后端周期发送
 * @read_all_slave_states: 读取所有从站状态
 * @read_single_slave_state: 读取单个从站状态
 * @deactivate: 停用后端周期通信
 * @close: 关闭后端
 */
struct backend_ops {
    int  (*open)(struct backend_instance *backend,
                 const struct mo_ecat_master_config *config);

    int  (*load_slave_info)(struct backend_instance *backend,
                            size_t *slave_count);

    int  (*configure_dc)(struct backend_instance *backend);

    int  (*build_pdo_mapping)(struct backend_instance *backend,
                              struct master_pdo_entry_mapping *entries,
                              size_t entry_count);

    int  (*activate)(struct backend_instance *backend);

    int  (*cyclic_receive)(struct backend_instance *backend,
                           struct mo_ecat_cyclic_result *result);

    int  (*cyclic_send)(struct backend_instance *backend,
                        struct mo_ecat_cyclic_result *result);

    int  (*read_all_slave_states)(struct backend_instance *backend,
                                  struct master_slave *slaves,
                                  size_t slave_count);

    int  (*read_single_slave_state)(struct backend_instance *backend,
                                    size_t slave_index,
                                    struct mo_ecat_node_state *state);

    int  (*deactivate)(struct backend_instance *backend);
    void (*close)(struct backend_instance *backend);
};

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_OPS_H */
