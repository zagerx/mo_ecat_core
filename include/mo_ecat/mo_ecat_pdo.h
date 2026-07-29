/*
 * mo_ecat_pdo.h - PDO 布局与周期数据访问接口
 */

#ifndef MO_ECAT_PDO_H
#define MO_ECAT_PDO_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * enum mo_ecat_pdo_direction - 主站视角的 PDO 数据方向
 * @MO_ECAT_PDO_INPUT:  输入方向，数据从节点流向主站（TxPDO，如 0x1A00）
 * @MO_ECAT_PDO_OUTPUT: 输出方向，数据从主站流向节点（RxPDO，如 0x1600）
 */
enum mo_ecat_pdo_direction {
    MO_ECAT_PDO_INPUT,
    MO_ECAT_PDO_OUTPUT
};

/**
 * struct pdo_entry - PDO 条目的最小规格
 * @object_index: 被映射对象的 CoE 对象字典索引，例如 0x6040
 * @object_subindex: 被映射对象的子索引，例如 0x04 表示 0x6040:04
 * @bit_length: 被映射字段占用的位数，例如 0x60400420 低 8 位
 *              0x20 表示 32 bit
 * @direction: 主站视角的数据方向；0x1C12 下的 RxPDO（如 0x1600）
 *             为 MO_ECAT_PDO_OUTPUT，0x1C13 下的 TxPDO（如
 *             0x1A00）为 MO_ECAT_PDO_INPUT
 *
 * 该公共值对象描述 Core 与 Adapter 共同识别 PDO entry 所需的最小信息。
 * 例如从 0x1600:03 读到映射值 0x60400420，将得到：
 * object_index=0x6040、object_subindex=0x04、bit_length=32、
 * direction=MO_ECAT_PDO_OUTPUT。
 */
struct pdo_entry {
    uint16_t object_index;
    uint8_t object_subindex;
    uint8_t bit_length;
    enum mo_ecat_pdo_direction direction;
};

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
 * struct pdo_entry_record - Master 发现的单个 PDO entry 记录
 * @entry_id: Master 将所有从站的 PDO entry 扁平化后分配的全局标识；
 *            仅用于当前 PDO 映射代际，不是 0x1600 等对象字典索引，
 *            应用不得自行构造
 * @slave_index: entry 所属从站在 Master 拓扑 slaves[] 数组中的下标，
 *               从 0 开始；不是 EtherCAT 配置站地址，也不是 SOEM
 *               从 1 开始的 slave number
 * @spec: PDO entry 的最小规格
 *
 * 例如，slave_index=2 的从站在 0x1600:03 中映射了
 * 0x6040:04/32 bit，Master 可能将其公开为：
 *
 * entry_id=17、slave_index=2、
 * spec={object_index=0x6040、object_subindex=0x04、bit_length=32、
 * direction=MO_ECAT_PDO_OUTPUT}。
 *
 * pdo_index=0x1600 和 entry 子索引 3 只在后端 SDO 扫描过程中使用，
 * 不保存在该公共结构中；字段在周期数据区域内的物理偏移也由 Core
 * 保存，应用层不直接访问。
 */
struct pdo_entry_record {
    uint32_t entry_id;
    size_t slave_index;
    struct pdo_entry spec;
};

size_t mo_ecat_master_get_pdo_entry_count(const struct mo_ecat_master *master);

int mo_ecat_master_get_pdo_entry(
    const struct mo_ecat_master *master,
    size_t index,
    struct pdo_entry_record *record);

const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                             const struct pdo_entry_record *record);

void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct pdo_entry_record *record);

/**
 * struct mo_ecat_pdo_handle - 已绑定的 PDO 数据访问句柄
 * @data: PDO 映像内的数据地址，由 Core 解析，调用方不得直接解引用
 * @generation: 绑定时的布局代际，0 表示无效句柄
 * @bit_length: 该周期数据项占用的位数
 * @bit_offset: 在 PDO 数据区域中的位偏移
 * @direction: PDO 数据方向，取 enum mo_ecat_pdo_direction 的值
 *
 * 非拥有值对象，可复制，无需显式销毁。句柄与其数据地址只允许在
 * 调度线程（dispatch/周期回调所在线程）使用；PDO 布局重建后，
 * generation 不再匹配，旧句柄自动失效。
 */
struct mo_ecat_pdo_handle {
    void *data;
    uint32_t generation;
    uint16_t bit_length;
    uint8_t bit_offset;
    uint8_t direction;
};

/**
 * mo_ecat_pdo_bind - 绑定 PDO entry 并解析访问句柄
 * @master: 主站对象指针
 * @record: Master 发现的 PDO entry 记录
 * @handle: 输出访问句柄
 *
 * 仅在管理阶段调用：完成 entry 全量校验与地址解析。
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_pdo_bind(struct mo_ecat_master *master,
                     const struct pdo_entry_record *record,
                     struct mo_ecat_pdo_handle *handle);

/**
 * mo_ecat_pdo_read - 经句柄获取输入 PDO 数据指针
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 *
 * 仅常数时间校验：句柄有效、代际匹配、映射存在且周期活动、方向为输入。
 *
 * Return: 成功返回数据指针，失败返回 NULL
 */
const void *mo_ecat_pdo_read(const struct mo_ecat_master *master,
                            const struct mo_ecat_pdo_handle *handle);

/**
 * mo_ecat_pdo_write - 经句柄获取输出 PDO 数据可写指针
 * @master: 主站对象指针
 * @handle: 已绑定的访问句柄
 *
 * 校验同 mo_ecat_pdo_read，方向为输出。
 *
 * Return: 成功返回可写数据指针，失败返回 NULL
 */
void *mo_ecat_pdo_write(struct mo_ecat_master *master,
                        const struct mo_ecat_pdo_handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_PDO_H */
