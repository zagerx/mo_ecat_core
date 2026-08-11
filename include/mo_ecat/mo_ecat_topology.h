/*
 * mo_ecat_topology.h - 总线拓扑节点信息接口
 */

#ifndef MO_ECAT_TOPOLOGY_H
#define MO_ECAT_TOPOLOGY_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_pdo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * enum mo_ecat_node_al_state - 节点应用层状态
 * @MO_ECAT_NODE_AL_STATE_INIT:       Init
 * @MO_ECAT_NODE_AL_STATE_PRE_OP:     Pre-Operational
 * @MO_ECAT_NODE_AL_STATE_SAFE_OP:    Safe-Operational
 * @MO_ECAT_NODE_AL_STATE_OP:         Operational
 * @MO_ECAT_NODE_AL_STATE_BOOTSTRAP:  Bootstrap
 * @MO_ECAT_NODE_AL_STATE_UNKNOWN:    未知或无法识别
 */
enum mo_ecat_node_al_state {
	MO_ECAT_NODE_AL_STATE_INIT,
	MO_ECAT_NODE_AL_STATE_PRE_OP,
	MO_ECAT_NODE_AL_STATE_SAFE_OP,
	MO_ECAT_NODE_AL_STATE_OP,
	MO_ECAT_NODE_AL_STATE_BOOTSTRAP,
	MO_ECAT_NODE_AL_STATE_UNKNOWN
};

/**
 * struct mo_ecat_node_state - 节点运行时诊断状态
 * @al_state:        应用层状态
 * @has_error:       是否存在 AL 错误
 * @al_status_code:  AL 状态码
 * @is_online:       是否在线
 * @is_operational:  是否处于 OP 状态
 */
struct mo_ecat_node_state {
	enum mo_ecat_node_al_state al_state;
	int has_error;
	uint16_t al_status_code;
	int is_online;
	int is_operational;
};

/**
 * struct mo_ecat_node_info - 拓扑节点的公开只读视图
 * @position:        节点位置
 * @alias:           节点别名
 * @vendor_id:       厂商 ID
 * @product_code:    产品码
 * @revision_number: 修订号
 * @name:            节点名称字符串
 * @dc_supported:    是否支持分布式时钟
 * @state:           节点运行时诊断状态
 *
 * 由 mo_ecat_master_get_node_info() 复制返回，避免向应用层暴露内部数组指针。
 */
struct mo_ecat_node_info {
	uint16_t position;
	uint16_t alias;
	uint32_t vendor_id;
	uint32_t product_code;
	uint32_t revision_number;
	char name[MO_ECAT_MAX_NAME_LEN + 1];
	int dc_supported;
	struct mo_ecat_node_state state;
};

size_t mo_ecat_master_get_node_count(const struct mo_ecat_master *master);

int mo_ecat_master_get_node_info(const struct mo_ecat_master *master, size_t index,
				 struct mo_ecat_node_info *info);

#define MO_ECAT_MAX_SLAVE_SM 8
#define MO_ECAT_MAX_SLAVE_PDO_ENTRIES 32

/**
 * struct mo_ecat_slave_sm - 从站同步管理器摘要
 * @start_address: SM 起始地址
 * @length: SM 长度
 * @type: SM 类型（1=邮箱收 2=邮箱发 3=过程数据输出 4=过程数据输入）
 */
struct mo_ecat_slave_sm {
	uint16_t start_address;
	uint16_t length;
	uint8_t type;
};

/**
 * struct mo_ecat_slave_detail - 从站配置详情（SM 与 PDO 映射明细）
 * @sm: 有效 SM 摘要数组
 * @sm_count: 有效 SM 数量
 * @pdo_entries: 该从站扫描到的 PDO 条目规格数组
 * @pdo_entry_count: PDO 条目数量
 *
 * 由 mo_ecat_master_get_slave_detail() 按值复制返回，
 * 供调试界面展示单个从站的配置细节。
 */
struct mo_ecat_slave_detail {
	struct mo_ecat_slave_sm sm[MO_ECAT_MAX_SLAVE_SM];
	size_t sm_count;
	struct pdo_entry pdo_entries[MO_ECAT_MAX_SLAVE_PDO_ENTRIES];
	size_t pdo_entry_count;
};

/**
 * mo_ecat_master_get_slave_detail - 获取指定从站的配置详情
 * @master: 主站对象指针
 * @index: 从站索引
 * @detail: 详情输出缓冲区
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_get_slave_detail(const struct mo_ecat_master *master, size_t index,
				    struct mo_ecat_slave_detail *detail);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_TOPOLOGY_H */
