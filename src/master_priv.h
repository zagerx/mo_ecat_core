/*
 * master_priv.h - 主站核心层内部定义
 *
 * 定义主站核心层内部使用的拓扑、PDO 映射及主站对象结构，
 * 并提供状态机与核心模块使用的内部辅助函数声明。
 */

#ifndef MASTER_PRIV_H
#define MASTER_PRIV_H

#include <stddef.h>
#include <stdatomic.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_cyclic.h"
#include "mo_ecat/mo_ecat_topology.h"
#include "backend/backend.h"
#include "mo_ecat/statemachine.h"
#include "master_states.h"
#include "master_resources.h"
#include "master_topology.h"
#include "master_pdo_mapping.h"
#include "master_error.h"
#include "pdo_image_priv.h"
#include "pdo_mapping_priv.h"
#include "topology_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct master_topology - 从站表
 * @slaves: 从站信息数组
 * @slave_count: 从站数量
 *
 * 保存已发现从站信息，运行时状态由后端直接更新到 slaves[].state。
 */
struct master_topology {
	struct master_slave *slaves;
	size_t slave_count;
};

/**
 * struct master_pdo_mapping - PDO 映射管理
 * @image: PDO 数据区域
 * @entries: PDO entry 内部映射数组
 * @entry_count: PDO entry 映射数量
 * @generation: 当前映射版本
 * @is_active: 是否允许周期读写
 *
 * 保存主站 PDO 数据区域、所有 PDO entry 的地址映射以及映射运行状态。
 */
struct master_pdo_mapping {
	struct master_pdo_image image;
	struct master_pdo_entry_mapping *entries;
	size_t entry_count;
	uint32_t generation;
	int is_active;
};

/**
 * struct mo_ecat_master - 主站对象（核心层内部定义）
 * @sm: 底层状态机
 * @command: 外部线程提交的命令槽
 * @state: 向外发布的主站状态
 * @error_code: 向外发布的故障码
 * @backend: 后端实例
 * @config: 主站内建配置指针，指向核心库的静态主站配置
 * @pdo_mapping: 主站 PDO 映射
 * @topology: 从站拓扑
 * @user_data: 用户私有数据
 * @cyclic_callback: 周期控制回调，仅在 RUNNING 调用
 */
struct mo_ecat_master {
	struct statemachine sm;

	_Atomic enum mo_ecat_master_cmd command;
	_Atomic enum mo_ecat_master_state state;
	_Atomic enum mo_ecat_master_error error_code;
	struct master_error_record last_error;
	struct backend_instance backend;
	const struct mo_ecat_master_config *config;
	struct master_pdo_mapping pdo_mapping;
	struct master_topology topology;

	void *user_data;
	mo_ecat_cyclic_callback cyclic_callback;
};

enum mo_ecat_master_cmd master_take_cmd(struct mo_ecat_master *master);

void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd);

enum master_error_detail master_cyclic_receive(struct mo_ecat_master *master,
							struct mo_ecat_cyclic_result *result);

enum master_error_detail master_cyclic_send(struct mo_ecat_master *master,
						 struct mo_ecat_cyclic_result *result);

#ifdef __cplusplus
}
#endif

#endif /* MASTER_PRIV_H */
