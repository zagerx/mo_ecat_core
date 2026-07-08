/**
 * @file ecat_master_states.c
 * @brief 主站状态机各状态处理函数
 */

#include <stdio.h>
#include <stdlib.h>

#include "ecat_master_priv.h"

/* 同一文件内的静态状态函数前向声明 */
static void master_state_running(struct statemachine *sm);
#ifdef __GNUC__
static void master_state_controlled(struct statemachine *sm) __attribute__((unused));
#endif

static const char *master_state_name(enum ec_master_state id)
{
    switch (id) {
        case EC_MASTER_STATE_INIT:       return "INIT";
        case EC_MASTER_STATE_READY:      return "READY";
        case EC_MASTER_STATE_RUNNING:    return "RUNNING";
        case EC_MASTER_STATE_FAULT:      return "FAULT";
        case EC_MASTER_STATE_CONTROLLED: return "CONTROLLED";
        default:                         return "UNKNOWN";
    }
}

static void master_set_state(struct ec_master *master, enum ec_master_state id)
{
    if (!master) {
        return;
    }
    master->state_id = id;
    printf("[EC Master] enter state: %s\n", master_state_name(id));
}

static void master_enter_fault(struct ec_master *master, const char *reason)
{
    fprintf(stderr, "[EC Master] FAULT: %s\n", reason);
    sm_transition(&master->sm, master_state_fault);
}

void master_state_init(struct statemachine *sm)
{
    struct ec_master *master = (struct ec_master *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_INIT);

        printf("[EC Master] opening interface: %s\n", master->ifname);
        if (!ecx_init(&master->context, master->ifname)) {
            master_enter_fault(master, "ecx_init failed");
            return;
        }

        if (ecx_config_init(&master->context) <= 0) {
            master_enter_fault(master, "ecx_config_init failed or no slave found");
            return;
        }

        if (ecat_slave_group_init(&master->group[0], &master->context) < 0) {
            master_enter_fault(master, "failed to initialize slave group");
            return;
        }

        printf("[EC Master] %d slave(s) found\n", master->context.slavecount);

        if (ecat_slave_group_map(&master->group[0], &master->context,
                                 &master->iomap, &master->iomap_size) < 0) {
            master_enter_fault(master, "ecx_config_map_group failed");
            return;
        }

        /* 配置 DC（可选） */
        if (master->dc_enabled) {
            ecx_configdc(&master->context);
        }

        /* 初始化完成，进入准备状态 */
        sm_transition(&master->sm, master_state_ready);
    }
}

void master_state_ready(struct statemachine *sm)
{
    struct ec_master *master = (struct ec_master *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_READY);

        /* 请求所有从站进入 OP */
        master->context.slavelist[0].state = EC_STATE_OPERATIONAL;
        ecx_writestate(&master->context, 0);

        sm->phase = USER_STATUS;
    }
    else if (sm->phase == USER_STATUS) {
        uint16_t state = ecx_statecheck(&master->context, 0,
                                        EC_STATE_OPERATIONAL, 10000);
        if (state == EC_STATE_OPERATIONAL) {
            sm_transition(&master->sm, master_state_running);
        }
    }
}

static void master_state_running(struct statemachine *sm)
{
    struct ec_master *master = (struct ec_master *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_RUNNING);
        sm->phase = USER_STATUS;
    }
    else if (sm->phase == USER_STATUS) {
        ecx_send_processdata(&master->context);
        ecx_receive_processdata(&master->context, EC_TIMEOUTRET);

        master->dc_time = master->context.DCtime;

        /* 简单故障检测：示例中可扩展为 WKC 检查 */
        if (master->context.ecaterror) {
            master_enter_fault(master, "SOEM error detected");
        }
    }
}

void master_state_fault(struct statemachine *sm)
{
    struct ec_master *master = (struct ec_master *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_FAULT);

        /* 尝试让从站回到 INIT */
        master->context.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&master->context, 0);

        sm->phase = USER_STATUS;
    }
}

static void master_state_controlled(struct statemachine *sm)
{
    struct ec_master *master = (struct ec_master *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_CONTROLLED);
        sm->phase = USER_STATUS;
    }
    /* 受控状态下的具体行为由外部控制器决定，占位 */
}
