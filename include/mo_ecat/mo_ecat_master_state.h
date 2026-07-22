/*
 * mo_ecat_master_state.h - 主站生命周期状态查询
 */

#ifndef MO_ECAT_MASTER_STATE_H
#define MO_ECAT_MASTER_STATE_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * enum mo_ecat_master_state - 主站生命周期状态
 * @MO_ECAT_MASTER_STATE_INIT:    初始状态，尚未完成任何初始化
 * @MO_ECAT_MASTER_STATE_IDLE:    空闲状态，可接受 SCAN 命令
 * @MO_ECAT_MASTER_STATE_READY:   已配置完成，可接受 ACTIVATE 命令
 * @MO_ECAT_MASTER_STATE_RUNNING: 周期运行中
 * @MO_ECAT_MASTER_STATE_FAULT:   故障状态，必须先 RESET
 */
enum mo_ecat_master_state {
	MO_ECAT_MASTER_STATE_INIT,
	MO_ECAT_MASTER_STATE_IDLE,
	MO_ECAT_MASTER_STATE_READY,
	MO_ECAT_MASTER_STATE_RUNNING,
	MO_ECAT_MASTER_STATE_FAULT
};

enum mo_ecat_master_state mo_ecat_master_get_state(const struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_STATE_H */
