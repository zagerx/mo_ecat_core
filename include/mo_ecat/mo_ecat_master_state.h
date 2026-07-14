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
    MO_ECAT_MASTER_STATE_DISCOVERED,
    MO_ECAT_MASTER_STATE_READY,
    MO_ECAT_MASTER_STATE_RUNNING,
    MO_ECAT_MASTER_STATE_FAULT
};

/** 最近一次周期通信结果。 */
struct mo_ecat_cycle_result {
    int link_up;
    uint32_t expected_wkc;
    uint32_t actual_wkc;
    int64_t dc_time_ns;
    int dc_time_valid;
    int diagnostics_required;
};

/**
 * @file mo_ecat_master_state.h
 * @brief 主站状态与周期结果查询
 */

/**
 * @brief 获取主站当前生命周期状态
 */
enum mo_ecat_master_state mo_ecat_master_get_state(
    const struct mo_ecat_master *master);

/**
 * @brief 获取最近一次周期结果
 *
 * 结果由 mo_ecat_master_cycle_end() 成功后更新。
 */
int mo_ecat_master_get_cycle_result(const struct mo_ecat_master *master,
                                    struct mo_ecat_cycle_result *result);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_STATE_H */
