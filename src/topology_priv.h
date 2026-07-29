/*
 * topology_priv.h - 主站从站拓扑内部结构
 *
 * 定义核心层保存的从站扫描缓存结构，包括基本信息、邮箱、同步管理器、
 * FMMU 以及 PDO entry 等内部数据。
 */

#ifndef TOPOLOGY_PRIV_H
#define TOPOLOGY_PRIV_H

#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_topology.h"

/**
 * SLAVE_MAX_PDO_ENTRIES - 单个从站最大 PDO entry 数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define SLAVE_MAX_PDO_ENTRIES 32

/**
 * SLAVE_MAX_SYNC_MANAGERS - 单个从站最大同步管理器（Sync Manager）数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define SLAVE_MAX_SYNC_MANAGERS 8

/**
 * SLAVE_MAX_FMMUS - 单个从站最大 FMMU 数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define SLAVE_MAX_FMMUS 4

/**
 * struct pdo_entry - PDO 映射条目的固有描述
 * @object_index: 被映射对象的 CoE 对象字典索引，例如 0x6040
 * @object_subindex: 被映射对象的子索引，例如 0x04 表示 0x6040:04
 * @bit_length: 被映射字段占用的位数，例如 0x60400420 低 8 位
 *              0x20 表示 32 bit
 * @direction: 主站视角的数据方向；0x1C12 下的 RxPDO（如 0x1600）
 *             为 MO_ECAT_PDO_OUTPUT，0x1C13 下的 TxPDO（如
 *             0x1A00）为 MO_ECAT_PDO_INPUT
 *
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
 * struct slave_pdo_entry - 单个从站扫描得到的 PDO entry
 * @pdo_index: entry 所属的 PDO Mapping 对象索引，例如 RxPDO 0x1600
 *             或 TxPDO 0x1A00
 * @pdo_subindex: entry 在 PDO Mapping 对象中的子索引，例如 3 表示
 *                该映射值读取自 0x1600:03
 * @entry: 从映射值解析出的字段描述；例如 0x1600:03 的值
 *         0x60400420 会解析为 0x6040:04、32 bit、主站输出方向
 *
 * 该结构保留“条目来自哪个 PDO”的扫描上下文。例如：
 *
 * 0x1C12:01 = 0x1600
 * 0x1600:03 = 0x60400420
 *
 * 对应 pdo_index=0x1600、pdo_subindex=3，
 * entry={object_index=0x6040, object_subindex=4, bit_length=32,
 * direction=MO_ECAT_PDO_OUTPUT}。
 */
struct slave_pdo_entry {
	uint16_t pdo_index;
	uint8_t pdo_subindex;
	struct pdo_entry entry;
};

/**
 * struct slave_mailbox - 从站邮箱信息
 * @protocol: 支持的邮箱协议
 * @write_address: 写邮箱起始地址
 * @write_size: 写邮箱大小
 * @read_address: 读邮箱起始地址
 * @read_size: 读邮箱大小
 */
struct slave_mailbox {
	uint16_t protocol;
	uint16_t write_address;
	uint16_t write_size;
	uint16_t read_address;
	uint16_t read_size;
};

/**
 * struct slave_sync_manager - 从站同步管理器信息
 * @start_address: 起始地址
 * @length: 长度
 * @flags: 标志
 * @type: 类型
 */
struct slave_sync_manager {
	uint16_t start_address;
	uint16_t length;
	uint32_t flags;
	uint8_t type;
};

/**
 * struct slave_fmmu - 从站 FMMU 信息
 * @function: FMMU 功能
 */
struct slave_fmmu {
	uint8_t function;
};

/**
 * struct slave_base_info - 从站基本扫描信息
 * @position: 从站在总线上的位置
 * @alias: 从站别名
 * @vendor_id: 厂商 ID
 * @product_code: 产品码
 * @revision_number: 修订号
 * @name: 从站名称字符串
 * @dc_supported: 是否支持分布式时钟
 * @propagation_delay_ns: 传播延迟（纳秒）
 * @mailbox: 邮箱信息
 * @has_coe: 是否支持 CoE
 * @has_foe: 是否支持 FoE
 * @has_eoe: 是否支持 EoE
 * @has_soe: 是否支持 SoE
 * @sm: 同步管理器数组
 * @fmmu: FMMU 数组
 */
struct slave_base_info {
	uint16_t position;
	uint16_t alias;
	uint32_t vendor_id;
	uint32_t product_code;
	uint32_t revision_number;
	char name[MO_ECAT_MAX_NAME_LEN + 1];
	int dc_supported;
	uint32_t propagation_delay_ns;
	struct slave_mailbox mailbox;
	int has_coe;
	int has_foe;
	int has_eoe;
	int has_soe;
	struct slave_sync_manager sm[SLAVE_MAX_SYNC_MANAGERS];
	struct slave_fmmu fmmu[SLAVE_MAX_FMMUS];
};

/**
 * struct slave - 核心层保存的完整从站扫描结果
 * @base_info: 从站基本扫描信息
 * @state: 从站当前运行状态
 * @pdo_entries: PDO entry 扫描缓存数组
 * @pdo_entry_count: 实际 PDO entry 数量
 *
 * 核心层保存的完整从站扫描结果；应用层只能取得摘要副本。
 */
struct slave {
	struct slave_base_info base_info;
	struct mo_ecat_node_state state;
	struct slave_pdo_entry pdo_entries[SLAVE_MAX_PDO_ENTRIES];
	size_t pdo_entry_count;
};

#endif /* TOPOLOGY_PRIV_H */
