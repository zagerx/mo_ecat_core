#ifndef MO_ECAT_PDO_H
#define MO_ECAT_PDO_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @brief RUNNING 状态中的周期控制回调。
 *
 * 回调执行在主站唯一周期调度线程中，位于接收输入 PDO 与发送输出 PDO 之间。
 * 回调可通过 mo_ecat_pdo_input() / mo_ecat_pdo_output() 访问本周期 PDO 数据。
 */
typedef void (*mo_ecat_cycle_callback)(struct mo_ecat_master *master,
                                       const struct mo_ecat_cycle_result *result,
                                       void *user_data);

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
 * @brief 获取指定 PDO entry 映射的副本
 *
 * 调用方提供 mapping 缓冲区，核心复制数据。调用方不得在主站 RESET 或重新配置
 * PDO 映射期间并发调用本函数。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_pdo_entry_mapping(
    const struct mo_ecat_master *master,
    size_t index,
    struct mo_ecat_pdo_entry_mapping *mapping);

/**
 * @brief 设置 RUNNING 状态中的周期控制回调。
 *
 * 回调函数和 user_data 必须在主站开始 RUNNING 前设置；运行期间不得修改。
 */
int mo_ecat_master_set_cycle_callback(struct mo_ecat_master *master,
                                      mo_ecat_cycle_callback callback,
                                      void *user_data);

/**
 * @brief 获取输入 PDO 在 PDO 数据区域中的地址。
 *
 * 仅允许在周期控制回调中调用。
 */
const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                              const struct mo_ecat_pdo_entry_mapping *mapping);

/**
 * @brief 获取输出 PDO 在 PDO 数据区域中的地址。
 *
 * 仅允许在周期控制回调中调用。
 */
void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_entry_mapping *mapping);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_PDO_H */
