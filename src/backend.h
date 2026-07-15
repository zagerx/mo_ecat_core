#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "pdo_image.h"

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

/**
 * @brief 读取所有从站的默认 PDO 映射条目。
 *
 * 后端通过 SDO 访问每个支持 CoE 的从站，读取 0x1C12/0x1C13 PDO 分配对象及
 * 对应映射对象，将解析后的条目填充到 slaves[].pdo_entries[] 中。
 *
 * @param slaves 核心层从站数组，由调用者根据从站数量预先分配。
 * @param slave_count 从站数量。
 * @return 0 成功，非 0 失败。
 */
int backend_read_pdo_entries(struct backend_instance *backend,
                             struct mo_ecat_slave *slaves,
                             size_t slave_count);

/**
 * @brief 配置后端的分布式时钟。
 *
 * 只能在从站扫描完成、建立 PDO 映射前调用。该函数只完成 DC 配置，
 * 不建立 PDO 数据区域，也不启动周期通信。
 */
int backend_configure_dc(struct backend_instance *backend);

/**
 * @brief 建立 PDO 映射并填写 PDO entry 的地址偏移。
 *
 * entries 由核心层根据扫描得到的 PDO 描述创建。后端建立 IOmap/domain 后，
 * 必须将每项对应的 byte_offset 与 bit_offset 写回 entries。
 * 成功后可通过 backend_get_pdo_image() 取得 PDO 数据区域。
 */
int backend_build_pdo_mapping(struct backend_instance *backend,
                              struct mo_ecat_pdo_entry_mapping *entries,
                              size_t entry_count);

/**
 * @brief 获取已建立 PDO 映射的数据区域。
 *
 * 仅能在 backend_build_pdo_mapping() 成功后调用。image 指向后端管理的
 * IOmap/domain 数据，不转移内存所有权。
 */
int backend_get_pdo_image(struct backend_instance *backend,
                          struct master_pdo_image *image);
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
