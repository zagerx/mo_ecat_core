/*
 * slave_priv.h - 从站内部数据结构
 *
 * 定义核心层保存的从站扫描缓存结构，包括基本信息、邮箱、同步管理器、
 * FMMU 以及 PDO entry 等内部数据。
 */

#ifndef SLAVE_PRIV_H
#define SLAVE_PRIV_H

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
 * @pdo_entries: PDO entry 最小规格扫描缓存数组，保持从站实际映射顺序
 * @pdo_entry_count: 实际 PDO entry 数量
 *
 * 核心层保存的完整从站扫描结果；应用层只能取得摘要副本。
 */
struct slave {
	struct slave_base_info base_info;
	struct mo_ecat_node_state state;
	struct pdo_entry pdo_entries[SLAVE_MAX_PDO_ENTRIES];
	size_t pdo_entry_count;
};

#endif /* SLAVE_PRIV_H */
