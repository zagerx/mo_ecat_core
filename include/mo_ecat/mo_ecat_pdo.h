#ifndef MO_ECAT_PDO_H
#define MO_ECAT_PDO_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/** PDO 在过程数据映像中的引用。 */
struct mo_ecat_pdo_ref {
    size_t slave_index;
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    uint32_t byte_offset;
    uint8_t bit_offset;
    enum mo_ecat_pdo_direction direction;
    uint32_t generation;
};

/**
 * @file mo_ecat_pdo.h
 * @brief PDO 引用与过程数据访问接口
 */

/**
 * @brief 获取 PDO 引用总数
 */
size_t mo_ecat_master_get_pdo_ref_count(const struct mo_ecat_master *master);

/**
 * @brief 过程数据映像的公开只读视图
 */
struct mo_ecat_process_image_view {
    const uint8_t *memory; /**< 过程映像基地址 */
    size_t         size;   /**< 过程映像字节数 */
    uint32_t       generation; /**< 所属过程映像代际 */
};

/**
 * @brief 获取指定 PDO 引用的副本
 *
 * 调用方提供 ref 缓冲区，核心在锁保护下复制数据。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_pdo_ref(const struct mo_ecat_master *master,
                               size_t index, struct mo_ecat_pdo_ref *ref);

/**
 * @brief 获取过程数据映像的公开只读视图
 *
 * 返回的 view->memory 指向后端 IOmap 内部缓冲区，仅在 READY/RUNNING 且未发生
 * RESET/重新配置前有效。应用层必须在提交 RESET/重新配置前停止访问。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_process_image_view(
    const struct mo_ecat_master *master,
    struct mo_ecat_process_image_view *view);

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
                              const struct mo_ecat_pdo_ref *ref);

/**
 * @brief 获取输出 PDO 在过程数据中的地址
 */
void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_ref *ref);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_PDO_H */
