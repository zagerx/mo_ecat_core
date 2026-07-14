#ifndef MASTER_PRIV_H
#define MASTER_PRIV_H

#include <stddef.h>
#include <pthread.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "backend.h"
#include "common/statemachine/statemachine.h"
#include "master_states.h"
#include "process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 最近一次周期结果
 */
struct mo_ecat_master_cycle_result {
	struct mo_ecat_cycle_result last; /**< 最近一次周期结果 */
};

/**
 * @brief 从站表
 *
 * 保存已发现从站信息，运行时状态由后端直接更新到 slaves[].state。
 */
struct mo_ecat_master_slave_table {
	struct mo_ecat_slave *slaves; /**< 从站信息数组 */
	size_t count;                 /**< 从站数量 */
};

/**
 * @brief PDO 引用表
 *
 * 由配置展开得到的扁平 PDO 引用数组，后端负责填写 offset。
 */
struct mo_ecat_master_pdo_refs {
	struct mo_ecat_slave_pdo_ref *refs; /**< PDO 引用数组 */
	size_t count;                 /**< PDO 引用数量 */
};

/**
 * @brief 过程数据管理单元
 *
 * 把过程映像缓冲区与 PDO 引用表统一放在一起管理，二者在 CONFIGURE 阶段同时建立，
 * 在 RESET/重新配置时同时失效。
 */
struct mo_ecat_master_process_data {
	struct mo_ecat_process_image image;      /**< 过程数据映像 */
	struct mo_ecat_master_pdo_refs pdo_refs; /**< PDO 引用表 */
};

/**
 * @brief 主站对象（核心层内部定义）
 */
struct mo_ecat_master {
	struct statemachine sm; /**< 底层状态机 */

	enum mo_ecat_master_cmd command;            /**< 当前请求命令 */
	enum mo_ecat_master_error error_code;       /**< 当前故障码 */
	struct backend_instance backend;             /**< 后端实例 */
	const struct mo_ecat_master_config *config; /**< 主站配置指针，指向外部配置，生命周期由调用者保证 */
	struct mo_ecat_master_process_data process; /**< 过程数据（映像 + PDO 引用） */
	struct mo_ecat_master_slave_table slave_table; /**< 从站表 */
	struct mo_ecat_master_cycle_result cycle_result; /**< 最近一次周期结果 */

	pthread_mutex_t lock; /**< 保护本对象的互斥锁 */
	void *user_data;      /**< 用户私有数据 */
};

/* 内部辅助函数，供状态机与核心模块使用 */
enum mo_ecat_master_state master_state_from_sm(const struct mo_ecat_master *master);
enum mo_ecat_master_cmd master_read_cmd(const struct mo_ecat_master *master);
void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd);
int master_build_slave_table(struct mo_ecat_master *master, size_t slave_count);
int master_read_pdo_entries(struct mo_ecat_master *master);
int master_configure(struct mo_ecat_master *master);
int master_activate(struct mo_ecat_master *master);
int master_deactivate(struct mo_ecat_master *master);
void master_release_resources(struct mo_ecat_master *master);
void master_clear_cmd(struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MASTER_PRIV_H */
