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

struct backend_instance;          /* 后端实例前向声明 */
struct backend_ops;              /* 后端生命周期/运行时 ops 前向声明 */
struct backend_translation_ops;  /* 后端数据转换 ops 前向声明 */

/**
 * @brief 后端实例
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

int backend_open(struct backend_instance *backend,
                 const struct mo_ecat_master_config *config);
int backend_load_slave_info(struct backend_instance *backend, size_t *slave_count);
int backend_get_slave_count(struct backend_instance *backend, size_t *slave_count);
int backend_translate_slave_info(struct backend_instance *backend,
                                   struct mo_ecat_slave *slaves,
                                   size_t slave_count);
int backend_read_pdo_entries(struct backend_instance *backend,
                             struct mo_ecat_slave *slaves,
                             size_t slave_count);
int backend_configure(struct backend_instance *backend);
int backend_get_process_image(struct backend_instance *backend,
                              struct mo_ecat_process_image *image);
int backend_fill_pdo_refs(struct backend_instance *backend,
                          struct mo_ecat_slave_pdo_ref *refs,
                          size_t ref_count,
                          uint32_t generation);
int backend_activate(struct backend_instance *backend);
int backend_cycle_begin(struct backend_instance *backend,
                        struct mo_ecat_cycle_result *result);
int backend_cycle_end(struct backend_instance *backend,
                      struct mo_ecat_cycle_result *result);
int backend_read_all_slave_states(struct backend_instance *backend,
                                  struct mo_ecat_slave *slaves,
                                  size_t slave_count);
int backend_read_single_slave_state(struct backend_instance *backend,
                                    size_t slave_index,
                                    struct mo_ecat_slave_state *state);
int backend_deactivate(struct backend_instance *backend);
void backend_close(struct backend_instance *backend);

/* ==================== Backend 工厂 ==================== */

int backend_init(struct backend_instance *backend);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
