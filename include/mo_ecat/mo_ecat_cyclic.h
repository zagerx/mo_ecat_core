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

typedef void (*mo_ecat_cyclic_callback)(struct mo_ecat_master *master,
                                        const struct mo_ecat_cyclic_result *result,
                                        void *user_data);

/**
 * struct mo_ecat_pdo_entry - Master 公开的单个 PDO entry 逻辑描述
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
struct mo_ecat_pdo_entry {
    uint32_t entry_id;
    size_t node_index;
    uint16_t object_index;
    uint8_t object_subindex;
    uint8_t bit_length;
    enum mo_ecat_cyclic_direction direction;
};

size_t mo_ecat_master_get_cyclic_entry_count(const struct mo_ecat_master *master);

int mo_ecat_master_get_cyclic_entry(
    const struct mo_ecat_master *master,
    size_t index,
    struct mo_ecat_pdo_entry *entry);

const void *mo_ecat_cyclic_input(const struct mo_ecat_master *master,
                                 const struct mo_ecat_pdo_entry *entry);

void *mo_ecat_cyclic_output(struct mo_ecat_master *master,
                            const struct mo_ecat_pdo_entry *entry);

/**
 * struct mo_ecat_cyclic_handle - 已绑定的周期数据访问句柄
 * @data: PDO 映像内的数据地址，由 Core 解析，调用方不得直接解引用
 * @generation: 绑定时的布局代际，0 表示无效句柄
 * @bit_length: 该周期数据项占用的位数
 * @bit_offset: 在 PDO 数据区域中的位偏移
 * @direction: 周期数据方向，取 enum mo_ecat_cyclic_direction 的值
 *
 * 非拥有值对象，可复制，无需显式销毁。句柄与其数据地址只允许在
 * 调度线程（dispatch/周期回调所在线程）使用，且仅在当前周期有效。
 */
struct mo_ecat_cyclic_handle {
    void *data;
    uint32_t generation;
    uint16_t bit_length;
    uint8_t bit_offset;
    uint8_t direction;
};

/**
 * mo_ecat_cyclic_bind - 绑定周期数据项并解析访问句柄
 * @master: 主站对象指针
 * @entry: PDO entry 逻辑描述
 * @handle: 输出访问句柄
 *
 * 仅在管理阶段调用：完成 entry 全量校验与地址解析。
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_cyclic_bind(struct mo_ecat_master *master,
                        const struct mo_ecat_pdo_entry *entry,
                        struct mo_ecat_cyclic_handle *handle);

/**
 * mo_ecat_cyclic_read - 经句柄获取输入 PDO 数据指针
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 *
 * 仅常数时间校验：句柄有效、代际匹配、映射存在且周期活动、方向为输入。
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
const void *mo_ecat_cyclic_read(const struct mo_ecat_master *master,
                                const struct mo_ecat_cyclic_handle *handle);

/**
 * mo_ecat_cyclic_write - 经句柄获取输出 PDO 数据可写指针
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 *
 * 校验同 mo_ecat_cyclic_read，方向为输出。
 *
 * Return: 成功返回可写数据指针，失败返回 NULL
 */
void *mo_ecat_cyclic_write(struct mo_ecat_master *master,
                           const struct mo_ecat_cyclic_handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_CYCLIC_H */
