/*
 * mo_ecat_master.h - 主站生命周期与调度接口
 *
 * 本头文件只包含主站最核心的生命周期函数。
 * 状态查询、节点信息、周期数据访问分别位于独立头文件。
 */

#ifndef MO_ECAT_MASTER_H
#define MO_ECAT_MASTER_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_config.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat/mo_ecat_sync0.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * enum mo_ecat_master_cmd - 主站状态机请求命令
 * @MO_ECAT_MASTER_CMD_NONE:       无命令
 * @MO_ECAT_MASTER_CMD_SCAN:       扫描总线
 * @MO_ECAT_MASTER_CMD_CONFIGURE:  配置 DC 并建立 PDO 映射
 * @MO_ECAT_MASTER_CMD_ACTIVATE:   激活周期运行
 * @MO_ECAT_MASTER_CMD_DEACTIVATE: 停止周期运行
 * @MO_ECAT_MASTER_CMD_RESET:      复位到空闲
 */
enum mo_ecat_master_cmd {
	MO_ECAT_MASTER_CMD_NONE,
	MO_ECAT_MASTER_CMD_SCAN,
	MO_ECAT_MASTER_CMD_CONFIGURE,
	MO_ECAT_MASTER_CMD_ACTIVATE,
	MO_ECAT_MASTER_CMD_DEACTIVATE,
	MO_ECAT_MASTER_CMD_RESET
};

/**
 * enum mo_ecat_master_error - 主站最近一次进入 FAULT 的原因
 * @MO_ECAT_MASTER_ERROR_NONE:                         无错误
 * @MO_ECAT_MASTER_ERROR_DISCOVER_FAILED:              扫描总线失败
 * @MO_ECAT_MASTER_ERROR_READ_PDO_DESCRIPTION_FAILED:     读取 PDO 描述失败
 * @MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED:          配置 DC 失败
 * @MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_MAPPING_FAILED:    建立 PDO 映射失败
 * @MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED:              激活周期运行失败
 * @MO_ECAT_MASTER_ERROR_BUS_FAULT:                    总线故障
 * @MO_ECAT_MASTER_ERROR_SYNC0_ACTIVATE_FAILED:        激活 Sync0 失败
 */
enum mo_ecat_master_error {
	MO_ECAT_MASTER_ERROR_NONE,
	MO_ECAT_MASTER_ERROR_DISCOVER_FAILED,
	MO_ECAT_MASTER_ERROR_READ_PDO_DESCRIPTION_FAILED,
	MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED,
	MO_ECAT_MASTER_ERROR_CONFIGURE_PDO_MAPPING_FAILED,
	MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED,
	MO_ECAT_MASTER_ERROR_BUS_FAULT,
	MO_ECAT_MASTER_ERROR_SYNC0_ACTIVATE_FAILED,
};

/**
 * mo_ecat_master_create - 创建主站对象
 * @callback: 周期控制回调，仅在 RUNNING 状态下每个周期调用
 * @user_data: 用户私有数据，随周期回调传回
 *
 * 实例数量不受限制；每个实例持有独立的状态与后端上下文。
 * 创建后必须通过 mo_ecat_master_binding() 绑定主站配置，
 * 未绑定配置的主站无法接受 SCAN 命令。
 *
 * Return: 成功返回主站对象指针，失败返回 NULL
 */
struct mo_ecat_master *mo_ecat_master_create(mo_ecat_cyclic_callback callback, void *user_data);

/**
 * mo_ecat_master_binding - 绑定主站配置
 * @master: 主站对象指针
 * @config: 主站配置指针（含EtherCAT网口），由调用方持有并保证唯一；
 *          主站不复制内容，配置对象必须比主站存活更久
 *
 * Return: 0 成功，非 0 失败
 */
int mo_ecat_master_binding(struct mo_ecat_master *master,
			   const struct mo_ecat_master_config *config);

void mo_ecat_master_destroy(struct mo_ecat_master *master);

void mo_ecat_master_dispatch(struct mo_ecat_master *master);

int mo_ecat_master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd);

enum mo_ecat_master_error mo_ecat_master_get_error_code(const struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_H */
