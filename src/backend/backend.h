/*
 * backend/backend.h - 后端适配层统一接口
 *
 * 定义后端实例结构及核心层与后端交互的统一入口函数。
 * 具体回调表定义见 backend/backend_ops.h。
 */

#ifndef BACKEND_BACKEND_H
#define BACKEND_BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_cyclic.h"
#include "mo_ecat/mo_ecat_topology.h"
#include "pdo_image_priv.h"
#include "pdo_mapping_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Backend 契约 ==================== */

struct backend_instance;	  /* 后端实例前向声明 */
struct backend_ops;		  /* 后端生命周期/运行时 ops 前向声明 */
struct backend_translation_ops;  /* 后端数据转换 ops 前向声明 */
struct master_slave;		  /* 核心层内部从站缓存 */

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

/**
 * backend_open - 打开后端
 * @backend: 后端实例指针
 * @config: 主站配置指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_open(struct backend_instance *backend,
                 const struct mo_ecat_master_config *config);

/**
 * backend_load_slave_info - 加载从站信息
 * @backend: 后端实例指针
 * @slave_count: 用于返回从站数量的指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_load_slave_info(struct backend_instance *backend, size_t *slave_count);

/**
 * backend_get_slave_count - 获取从站数量
 * @backend: 后端实例指针
 * @slave_count: 用于返回从站数量的指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_get_slave_count(struct backend_instance *backend, size_t *slave_count);

/**
 * backend_translate_slave_info - 转换从站信息到核心层结构
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int backend_translate_slave_info(struct backend_instance *backend,
                                   struct master_slave *slaves,
                                   size_t slave_count);

/**
 * backend_read_pdo_entries - 读取所有从站的默认 PDO 映射条目
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组，由调用者根据从站数量预先分配
 * @slave_count: 从站数量
 *
 * 后端通过 SDO 访问每个支持 CoE 的从站，读取 0x1C12/0x1C13 PDO 分配对象及
 * 对应映射对象，将解析后的条目填充到 slaves[].pdo_entries[] 中。
 *
 * Return: 0 成功，非 0 失败
 */
int backend_read_pdo_entries(struct backend_instance *backend,
                             struct master_slave *slaves,
                             size_t slave_count);

/**
 * backend_configure_dc - 配置后端的分布式时钟
 * @backend: 后端实例指针
 *
 * 只能在从站扫描完成、建立 PDO 映射前调用。该函数只完成 DC 配置，
 * 不建立 PDO 数据区域，也不启动周期通信。
 *
 * Return: 0 成功，非 0 失败
 */
int backend_configure_dc(struct backend_instance *backend);

/**
 * backend_build_pdo_mapping - 建立 PDO 映射
 * @backend: 后端实例指针
 * @entries: PDO entry 映射数组
 * @entry_count: PDO entry 数量
 *
 * entries 由核心层根据扫描得到的 PDO 描述创建。后端建立 IOmap/domain 后，
 * 必须将每项对应的 byte_offset 与 bit_offset 写回 entries。
 * 成功后可通过 backend_get_pdo_image() 取得 PDO 数据区域。
 *
 * Return: 0 成功，非 0 失败
 */
int backend_build_pdo_mapping(struct backend_instance *backend,
                              struct master_pdo_entry_mapping *entries,
                              size_t entry_count);

/**
 * backend_get_pdo_image - 获取已建立 PDO 映射的数据区域
 * @backend: 后端实例指针
 * @image: 用于返回 PDO 数据映像的指针
 *
 * 仅能在 backend_build_pdo_mapping() 成功后调用。image 指向后端管理的
 * IOmap/domain 数据，不转移内存所有权。
 *
 * Return: 0 成功，非 0 失败
 */
int backend_get_pdo_image(struct backend_instance *backend,
                          struct master_pdo_image *image);

/**
 * backend_activate - 激活后端周期通信
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_activate(struct backend_instance *backend);

/**
 * backend_cyclic_receive - 后端周期接收
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_cyclic_receive(struct backend_instance *backend,
                           struct mo_ecat_cyclic_result *result);

/**
 * backend_cyclic_send - 后端周期发送
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_cyclic_send(struct backend_instance *backend,
                        struct mo_ecat_cyclic_result *result);

/**
 * backend_read_all_slave_states - 读取所有从站状态
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int backend_read_all_slave_states(struct backend_instance *backend,
                                  struct master_slave *slaves,
                                  size_t slave_count);

/**
 * backend_read_single_slave_state - 读取单个从站状态
 * @backend: 后端实例指针
 * @slave_index: 从站索引
 * @state: 用于返回从站状态的指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_read_single_slave_state(struct backend_instance *backend,
                                    size_t slave_index,
                                    struct mo_ecat_node_state *state);

/**
 * backend_deactivate - 停用后端周期通信
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_deactivate(struct backend_instance *backend);

/**
 * backend_close - 关闭后端
 * @backend: 后端实例指针
 */
void backend_close(struct backend_instance *backend);

/* ==================== Backend 工厂 ==================== */

/**
 * backend_init - 初始化后端实例
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int backend_init(struct backend_instance *backend);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_BACKEND_H */
