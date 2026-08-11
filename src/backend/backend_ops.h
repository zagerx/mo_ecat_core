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
	enum backend_error (*translate_slave_info)(struct backend_instance *backend,
						   struct slave *slaves, size_t slave_count);

	enum backend_error (*read_pdo_entries)(struct backend_instance *backend,
					       struct slave *slaves, size_t slave_count);

	enum backend_error (*get_pdo_image)(struct backend_instance *backend,
					    struct pdo_image *image);
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
	enum backend_error (*open)(struct backend_instance *backend,
				   const struct mo_ecat_master_config *config);

	enum backend_error (*load_slave_info)(struct backend_instance *backend,
					      size_t *slave_count);

	enum backend_error (*configure_dc)(struct backend_instance *backend);

	enum backend_error (*build_pdo_mapping)(struct backend_instance *backend,
						struct pdo_image_entry *entries,
						size_t entry_count);

	enum backend_error (*activate)(struct backend_instance *backend);

	enum backend_error (*cyclic_receive)(struct backend_instance *backend,
					     struct mo_ecat_cyclic_result *result);

	enum backend_error (*cyclic_send)(struct backend_instance *backend,
					  struct mo_ecat_cyclic_result *result);

	enum backend_error (*read_all_slave_states)(struct backend_instance *backend,
						    struct slave *slaves, size_t slave_count);

	enum backend_error (*read_single_slave_state)(struct backend_instance *backend,
						      size_t slave_index,
						      struct mo_ecat_node_state *state);

	enum backend_error (*deactivate)(struct backend_instance *backend);
	void (*close)(struct backend_instance *backend);

	/* 从站 DC Sync0 周期同步输出控制（仅配置阶段调用）。 */
	enum backend_error (*sync0_configure)(struct backend_instance *backend, size_t slave_index,
					      int enable, uint32_t cycle_time_ns,
					      int32_t shift_time_ns);

	/* 读回从站 Sync0 当前激活状态（供诊断）。 */
	enum backend_error (*sync0_read_status)(struct backend_instance *backend,
						size_t slave_index,
						struct mo_ecat_sync0_status *status);

	/* 将 SAFE-OP 的单个从站恢复到 OP（清错误码并请求状态迁移）。 */
	enum backend_error (*recover_slave)(struct backend_instance *backend, size_t slave_index);

	/* 设置单个从站 AL 状态（调试用途，直接写 AL Control 寄存器）。 */
	enum backend_error (*set_slave_al_state)(struct backend_instance *backend,
						 size_t slave_index,
						 enum mo_ecat_node_al_state target_state);
};

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_OPS_H */
