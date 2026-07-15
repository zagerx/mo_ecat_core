#ifndef MASTER_PRIV_H
#define MASTER_PRIV_H

#include <stddef.h>
#include <stdatomic.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_cyclic.h"
#include "mo_ecat/mo_ecat_topology.h"
#include "backend.h"
#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "pdo_image.h"
#include "pdo_mapping_priv.h"
#include "slave_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从站表
 *
 * 保存已发现从站信息，运行时状态由后端直接更新到 slaves[].state。
 */
struct master_topology {
	struct master_slave *slaves; /**< 从站信息数组 */
	size_t slave_count;          /**< 从站数量 */
};

/**
 * @brief PDO 映射管理
 *
 * 保存主站 PDO 数据区域、所有 PDO entry 的地址映射以及映射运行状态。
 */
struct master_pdo_mapping {
	struct master_pdo_image image;                   /**< PDO 数据区域 */
	struct master_pdo_entry_mapping *entries;        /**< PDO entry 内部映射数组 */
	size_t entry_count;                              /**< PDO entry 映射数量 */
	uint32_t generation;                             /**< 当前映射版本 */
	int is_active;                                   /**< 是否允许周期读写 */
};

/**
 * @brief 主站对象（核心层内部定义）
 */
struct mo_ecat_master {
	struct statemachine sm; /**< 底层状态机 */

	_Atomic enum mo_ecat_master_cmd command;    /**< 外部线程提交的命令槽 */
	_Atomic enum mo_ecat_master_state state;    /**< 向外发布的主站状态 */
	_Atomic enum mo_ecat_master_error error_code; /**< 向外发布的故障码 */
	struct backend_instance backend;             /**< 后端实例 */
	const struct mo_ecat_master_config *config; /**< 主站配置指针，指向外部配置，生命周期由调用者保证 */
	struct master_pdo_mapping pdo_mapping;      /**< 主站 PDO 映射 */
	struct master_topology topology; /**< 从站拓扑 */

	void *user_data;      /**< 用户私有数据 */
	mo_ecat_cyclic_callback cyclic_callback; /**< 周期控制回调，仅在 RUNNING 调用 */
};

/* 内部辅助函数，供状态机与核心模块使用 */
enum mo_ecat_master_cmd master_take_cmd(struct mo_ecat_master *master);
void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd);
int master_topology_build(struct mo_ecat_master *master);
int master_topology_refresh_states(struct mo_ecat_master *master);
int master_pdo_mapping_build(struct mo_ecat_master *master);
int master_pdo_mapping_activate(struct mo_ecat_master *master);
int master_pdo_mapping_deactivate(struct mo_ecat_master *master);
void master_resources_release(struct mo_ecat_master *master);
int master_cyclic_receive(struct mo_ecat_master *master,
			  struct mo_ecat_cyclic_result *result);
int master_cyclic_send(struct mo_ecat_master *master,
		       struct mo_ecat_cyclic_result *result);

#ifdef __cplusplus
}
#endif

#endif /* MASTER_PRIV_H */
