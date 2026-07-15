/*
 * mo_ecat_cyclic.h - 周期数据访问接口
 */

#ifndef MO_ECAT_CYCLIC_H
#define MO_ECAT_CYCLIC_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * struct mo_ecat_cyclic_result - 单次周期通信结果
 * @link_up:              本周期链路是否可用
 * @expected_wkc:         期望工作计数器
 * @actual_wkc:           实际工作计数器
 * @dc_time_ns:           分布式时钟时间，单位纳秒
 * @dc_time_valid:        dc_time_ns 是否有效
 * @diagnostics_required: 是否需要刷新节点诊断状态
 */
struct mo_ecat_cyclic_result {
    int link_up;
    uint32_t expected_wkc;
    uint32_t actual_wkc;
    int64_t dc_time_ns;
    int dc_time_valid;
    int diagnostics_required;
};

/**
 * typedef mo_ecat_cyclic_callback - RUNNING 状态中的周期控制回调
 * @master:    主站对象
 * @result:    本周期接收结果
 * @user_data: 应用层私有数据
 *
 * 回调执行在主站唯一周期调度线程中，位于接收输入周期数据与发送输出周期数据之间。
 * 回调可通过 mo_ecat_cyclic_input() / mo_ecat_cyclic_output() 访问本周期数据。
 */
typedef void (*mo_ecat_cyclic_callback)(struct mo_ecat_master *master,
                                        const struct mo_ecat_cyclic_result *result,
                                        void *user_data);

/**
 * struct mo_ecat_cyclic_entry - 单个周期数据项的逻辑描述
 * @entry_id:       核心分配的周期数据项标识；不可由应用自行构造
 * @node_index:     节点在主站拓扑数组中的下标，不是 EtherCAT 站地址
 * @object_index:   CoE 对象字典索引
 * @object_subindex: CoE 对象字典子索引
 * @bit_length:     该周期数据项占用的位数
 * @direction:      周期数据方向：输入或输出
 *
 * 该结构标识一个节点 CoE 对象字典条目。其在周期数据区域内的物理偏移由
 * 核心层保存，应用层不直接访问。
 */
struct mo_ecat_cyclic_entry {
    uint32_t entry_id;
    size_t node_index;
    uint16_t object_index;
    uint8_t object_subindex;
    uint8_t bit_length;
    enum mo_ecat_cyclic_direction direction;
};

/**
 * mo_ecat_master_get_cyclic_entry_count - 获取已建立周期数据映射中的逻辑数据项总数
 * @master: 主站对象
 *
 * Return: 数据项总数
 */
size_t mo_ecat_master_get_cyclic_entry_count(const struct mo_ecat_master *master);

/**
 * mo_ecat_master_get_cyclic_entry - 获取指定周期数据项的逻辑描述
 * @master: 主站对象
 * @index:  数据项下标
 * @entry:  调用方提供的缓冲区，核心复制数据
 *
 * 调用方不得在主站 RESET 或重新配置周期数据映射期间并发调用本函数。
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_cyclic_entry(
    const struct mo_ecat_master *master,
    size_t index,
    struct mo_ecat_cyclic_entry *entry);

/**
 * mo_ecat_cyclic_input - 获取输入周期数据在周期数据区域中的地址
 * @master: 主站对象
 * @entry:  周期数据项描述
 *
 * 仅允许在周期控制回调中调用。
 *
 * Return: 输入数据指针，参数无效时返回 NULL
 */
const void *mo_ecat_cyclic_input(const struct mo_ecat_master *master,
                                 const struct mo_ecat_cyclic_entry *entry);

/**
 * mo_ecat_cyclic_output - 获取输出周期数据在周期数据区域中的地址
 * @master: 主站对象
 * @entry:  周期数据项描述
 *
 * 仅允许在周期控制回调中调用。
 *
 * Return: 输出数据指针，参数无效时返回 NULL
 */
void *mo_ecat_cyclic_output(struct mo_ecat_master *master,
                            const struct mo_ecat_cyclic_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_CYCLIC_H */
