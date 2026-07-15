#ifndef MO_ECAT_MASTER_STATE_H
#define MO_ECAT_MASTER_STATE_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/** 主站生命周期状态。 */
enum mo_ecat_master_state {
    MO_ECAT_MASTER_STATE_INIT,
    MO_ECAT_MASTER_STATE_IDLE,
    MO_ECAT_MASTER_STATE_READY,
    MO_ECAT_MASTER_STATE_RUNNING,
    MO_ECAT_MASTER_STATE_FAULT
};

/**
 * @file mo_ecat_master_state.h
 * @brief 主站生命周期状态查询
 */

/**
 * @brief 获取主站当前生命周期状态
 */
enum mo_ecat_master_state mo_ecat_master_get_state(
    const struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_STATE_H */
