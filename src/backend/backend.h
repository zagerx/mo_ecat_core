/*
 * backend/backend.h - 后端适配层统一接口
 *
 * 定义后端实例结构及核心层与后端交互的统一入口函数。
 * 具体回调表定义见 backend/backend_ops.h。
 */

#ifndef BACKEND_BACKEND_H
#define BACKEND_BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_master_config.h"
#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_topology.h"
#include "backend_error.h"
#include "pdo_image_priv.h"
#include "pdo_mapping_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Backend 契约 ==================== */

struct backend_instance;	  /* 后端实例前向声明 */
struct backend_ops;		  /* 后端生命周期/运行时 ops 前向声明 */
struct backend_translation_ops;  /* 后端数据转换 ops 前向声明 */
struct slave;		  /* 核心层内部从站缓存 */

/**
 * struct backend_instance - 后端实例
 * @name: 后端名称
 * @ops: 后端生命周期/运行时回调表
 * @translation_ops: 后端数据转换回调表
 * @ctx: 后端私有上下文指针
 *
 * 对核心层而言，ops 和 translation_ops 是不透明指针，
 * 具体回调表定义见 backend/backend_ops.h，仅供 backend.c 和后端实现使用。
 */
struct backend_instance {
    const char *name;
    const struct backend_ops *ops;
    const struct backend_translation_ops *translation_ops;
    void *ctx;
};

/* ==================== Backend 统一入口 ==================== */

enum backend_error backend_open(struct backend_instance *backend,
				const struct mo_ecat_master_config *config);

enum backend_error backend_load_slave_info(struct backend_instance *backend, size_t *slave_count);

enum backend_error backend_translate_slave_info(struct backend_instance *backend,
					  struct slave *slaves,
					  size_t slave_count);

enum backend_error backend_read_pdo_entries(struct backend_instance *backend,
				    struct slave *slaves,
				    size_t slave_count);

enum backend_error backend_configure_dc(struct backend_instance *backend);

enum backend_error backend_build_pdo_mapping(struct backend_instance *backend,
				     struct pdo_entry_mapping *entries,
				     size_t entry_count);

enum backend_error backend_get_pdo_image(struct backend_instance *backend,
				 struct master_pdo_image *image);

enum backend_error backend_activate(struct backend_instance *backend);

enum backend_error backend_cyclic_receive(struct backend_instance *backend,
				  struct mo_ecat_cyclic_result *result);

enum backend_error backend_cyclic_send(struct backend_instance *backend,
			       struct mo_ecat_cyclic_result *result);

enum backend_error backend_read_all_slave_states(struct backend_instance *backend,
					 struct slave *slaves,
					 size_t slave_count);

enum backend_error backend_read_single_slave_state(struct backend_instance *backend,
					   size_t slave_index,
					   struct mo_ecat_node_state *state);

enum backend_error backend_deactivate(struct backend_instance *backend);

void backend_close(struct backend_instance *backend);

/* ==================== Backend 工厂 ==================== */

enum backend_error backend_init(struct backend_instance *backend);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_BACKEND_H */
