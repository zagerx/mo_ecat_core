#ifndef MO_ECAT_PDO_H
#define MO_ECAT_PDO_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * 单个 PDO entry 在主站 PDO 数据区域中的映射描述。
 *
 * 该结构连接从站的 CoE 对象字典条目与过程数据映像中的实际位置。
 * slave_index / object_index / object_subindex / bit_length / direction
 * 在映射建立时由核心层从从站描述填充；byte_offset / bit_offset 由
 * 后端在建立 IOmap/domain 后回填；generation 用于标识该映射所属版本。
 */
struct mo_ecat_pdo_entry_mapping {
    size_t slave_index;          /**< 从站在主站从站表中的索引 */
    uint16_t object_index;       /**< CoE 对象字典索引 */
    uint8_t object_subindex;     /**< CoE 对象字典子索引 */
    uint8_t bit_length;          /**< 该 PDO entry 占用的位数 */
    uint32_t byte_offset;        /**< 在 PDO 数据区域中的字节偏移 */
    uint8_t bit_offset;          /**< 在字节内的起始位偏移 */
    enum mo_ecat_pdo_direction direction; /**< PDO 方向：输入或输出 */
    uint32_t generation;         /**< 所属 PDO 映射版本号 */
};

/**
 * @file mo_ecat_pdo.h
 * @brief PDO entry 映射与 PDO 数据访问接口
 */

/**
 * @brief 获取 PDO entry 映射总数
 */
size_t mo_ecat_master_get_pdo_entry_mapping_count(const struct mo_ecat_master *master);

/**
 * @brief PDO 数据区域的公开只读视图
 */
struct mo_ecat_pdo_image_view {
    const uint8_t *memory; /**< PDO 数据区域基地址 */
    size_t         size;   /**< PDO 数据区域字节数 */
    uint32_t       generation; /**< 所属 PDO 映射版本 */
};

/**
 * @brief 获取指定 PDO entry 映射的副本
 *
 * 调用方提供 mapping 缓冲区，核心在锁保护下复制数据。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_pdo_entry_mapping(
    const struct mo_ecat_master *master,
    size_t index,
    struct mo_ecat_pdo_entry_mapping *mapping);

/**
 * @brief 获取 PDO 数据区域的公开只读视图
 *
 * 返回的 view->memory 指向后端 IOmap 内部缓冲区，仅在 READY/RUNNING 且未发生
 * RESET/重新配置前有效。应用层必须在提交 RESET/重新配置前停止访问。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_pdo_image_view(
    const struct mo_ecat_master *master,
    struct mo_ecat_pdo_image_view *view);

/**
 * @brief 周期开始（接收过程数据）
 */
int mo_ecat_master_cycle_begin(struct mo_ecat_master *master,
                               struct mo_ecat_cycle_result *result);

/**
 * @brief 周期结束（发送过程数据）
 */
int mo_ecat_master_cycle_end(struct mo_ecat_master *master,
                             struct mo_ecat_cycle_result *result);

/**
 * @brief 获取输入 PDO 在过程数据中的地址
 */
const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                              const struct mo_ecat_pdo_entry_mapping *mapping);

/**
 * @brief 获取输出 PDO 在过程数据中的地址
 */
void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_entry_mapping *mapping);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_PDO_H */
