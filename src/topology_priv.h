/*
 * topology_priv.h - 主站从站拓扑内部结构
 *
 * 定义核心层保存的从站扫描缓存结构，包括基本信息、邮箱、同步管理器、
 * FMMU 以及 PDO entry 等内部数据。
 */

#ifndef TOPOLOGY_PRIV_H
#define TOPOLOGY_PRIV_H

#include "mo_ecat/mo_ecat_topology.h"

/**
 * MASTER_MAX_PDO_ENTRIES - 单个从站最大 PDO entry 数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define MASTER_MAX_PDO_ENTRIES 32

/**
 * MASTER_MAX_SM - 单个从站最大同步管理器（Sync Manager）数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define MASTER_MAX_SM          8

/**
 * MASTER_MAX_FMMU - 单个从站最大 FMMU 数量
 *
 * 后端扫描缓存的固定容量，不属于公开 API 约束。
 */
#define MASTER_MAX_FMMU        4

/**
 * struct master_slave_pdo_entry - 从站 PDO entry 扫描缓存
 * @pdo_index: PDO 索引
 * @object_index: 映射对象索引
 * @object_subindex: 映射对象子索引
 * @bit_length: 位长度
 * @direction: 方向（输入/输出）
 */
struct master_slave_pdo_entry {
	uint16_t pdo_index;
	uint16_t object_index;
	uint8_t object_subindex;
	uint8_t bit_length;
	enum mo_ecat_cyclic_direction direction;
};

/**
 * struct master_slave_mailbox - 从站邮箱信息
 * @protocol: 支持的邮箱协议
 * @write_address: 写邮箱起始地址
 * @write_size: 写邮箱大小
 * @read_address: 读邮箱起始地址
 * @read_size: 读邮箱大小
 */
struct master_slave_mailbox {
	uint16_t protocol;
	uint16_t write_address;
	uint16_t write_size;
	uint16_t read_address;
	uint16_t read_size;
};

/**
 * struct master_slave_sync_manager - 从站同步管理器信息
 * @start_address: 起始地址
 * @length: 长度
 * @flags: 标志
 * @type: 类型
 */
struct master_slave_sync_manager {
	uint16_t start_address;
	uint16_t length;
	uint32_t flags;
	uint8_t type;
};

/**
 * struct master_slave_fmmu - 从站 FMMU 信息
 * @function: FMMU 功能
 */
struct master_slave_fmmu {
	uint8_t function;
};

/**
 * struct master_slave_base_info - 从站基本扫描信息
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
struct master_slave_base_info {
	uint16_t position;
	uint16_t alias;
	uint32_t vendor_id;
	uint32_t product_code;
	uint32_t revision_number;
	char name[MO_ECAT_MAX_NAME_LEN + 1];
	int dc_supported;
	uint32_t propagation_delay_ns;
	struct master_slave_mailbox mailbox;
	int has_coe;
	int has_foe;
	int has_eoe;
	int has_soe;
	struct master_slave_sync_manager sm[MASTER_MAX_SM];
	struct master_slave_fmmu fmmu[MASTER_MAX_FMMU];
};

/**
 * struct master_slave - 核心层保存的完整从站扫描结果
 * @base_info: 从站基本扫描信息
 * @state: 从站当前运行状态
 * @pdo_entries: PDO entry 扫描缓存数组
 * @pdo_entry_count: 实际 PDO entry 数量
 *
 * 核心层保存的完整从站扫描结果；应用层只能取得摘要副本。
 */
struct master_slave {
	struct master_slave_base_info base_info;
	struct mo_ecat_node_state state;
	struct master_slave_pdo_entry pdo_entries[MASTER_MAX_PDO_ENTRIES];
	size_t pdo_entry_count;
};

#endif /* TOPOLOGY_PRIV_H */
