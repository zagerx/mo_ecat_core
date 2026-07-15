#ifndef MO_ECAT_CYCLIC_H
#define MO_ECAT_CYCLIC_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/** 单次周期通信结果。 */
struct mo_ecat_cycle_result {
    int link_up;
    uint32_t expected_wkc;
    uint32_t actual_wkc;
    int64_t dc_time_ns;
    int dc_time_valid;
    int diagnostics_required;
};

/**
 * @brief RUNNING 状态中的周期控制回调。
 *
 * 回调执行在主站唯一周期调度线程中，位于接收输入周期数据与发送输出周期数据之间。
 */
typedef void (*mo_ecat_cycle_callback)(struct mo_ecat_master *master,
                                       const struct mo_ecat_cycle_result *result,
                                       void *user_data);

/**
 * 单个周期数据项的逻辑描述。
 *
 * 该结构标识一个节点 CoE 对象字典条目。其在周期数据区域内的物理偏移由
 * 核心层保存，应用层不直接访问。
 */
struct mo_ecat_cyclic_entry {
	uint32_t id;                 /**< 核心分配的周期数据项标识；不可由应用自行构造 */
	size_t node_index;           /**< 节点在主站拓扑表中的索引 */
    uint16_t object_index;       /**< CoE 对象字典索引 */
    uint8_t object_subindex;     /**< CoE 对象字典子索引 */
    uint8_t bit_length;          /**< 该 PDO entry 占用的位数 */
	enum mo_ecat_cyclic_direction direction; /**< 周期数据方向：输入或输出 */
};

/**
 * @file mo_ecat_cyclic.h
 * @brief 周期数据访问接口
 */

/**
 * @brief 获取已建立周期数据映射中的逻辑数据项总数
 */
size_t mo_ecat_master_get_cyclic_entry_count(const struct mo_ecat_master *master);

/**
 * @brief 获取指定周期数据项的逻辑描述
 *
 * 调用方提供 entry 缓冲区，核心复制数据。调用方不得在主站 RESET 或重新配置
 * 周期数据映射期间并发调用本函数。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_cyclic_entry(
    const struct mo_ecat_master *master,
    size_t index,
    struct mo_ecat_cyclic_entry *entry);

/**
 * @brief 获取输入周期数据在周期数据区域中的地址。
 *
 * 仅允许在周期控制回调中调用。
 */
const void *mo_ecat_cyclic_input(const struct mo_ecat_master *master,
                                 const struct mo_ecat_cyclic_entry *entry);

/**
 * @brief 获取输出周期数据在周期数据区域中的地址。
 *
 * 仅允许在周期控制回调中调用。
 */
void *mo_ecat_cyclic_output(struct mo_ecat_master *master,
                            const struct mo_ecat_cyclic_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_CYCLIC_H */
