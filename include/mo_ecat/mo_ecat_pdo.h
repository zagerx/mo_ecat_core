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
    uint16_t pdo_index;
    uint16_t entry_index;
    uint32_t byte_offset;
    uint8_t bit_offset;
    uint8_t bit_length;
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
 * @brief 获取指定 PDO 引用
 */
const struct mo_ecat_pdo_ref *mo_ecat_master_get_pdo_ref(
    const struct mo_ecat_master *master, size_t index);

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
 * @brief 获取过程数据区指针与大小
 */
int mo_ecat_master_get_process_image(const struct mo_ecat_master *master,
                                     const uint8_t **memory,
                                     size_t *size);

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
