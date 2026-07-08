/**
 * @file ecat_master.c
 * @brief 基于 SOEM 的 C 主站对象公共 API 实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecat_master_priv.h"

/* ==================== 公共 API ==================== */

struct ec_master *ec_master_create(const char *ifname)
{
    if (!ifname) {
        return NULL;
    }

    struct ec_master *master = calloc(1, sizeof(struct ec_master));
    if (!master) {
        return NULL;
    }

    strncpy(master->ifname, ifname, EC_MASTER_IFNAME_SIZE - 1);
    master->ifname[EC_MASTER_IFNAME_SIZE - 1] = '\0';
    master->dc_enabled = 0;

    statemachine_init(&master->sm, master, master_state_init);

    return master;
}

void ec_master_destroy(struct ec_master *master)
{
    if (!master) {
        return;
    }

    ecx_close(&master->context);
    ecat_slave_group_cleanup(&master->group[0]);

    if (master->iomap) {
        free(master->iomap);
        master->iomap = NULL;
        master->iomap_size = 0;
    }

    free(master);
}

int ec_master_start(struct ec_master *master)
{
    if (!master) {
        return -1;
    }

    /* 触发一次状态机调度，执行 init 状态 */
    ec_master_run_cycle(master);
    return 0;
}

void ec_master_stop(struct ec_master *master)
{
    if (!master) {
        return;
    }

    /* 从运行态回到准备态，并请求 SAFE_OP */
    master->context.slavelist[0].state = EC_STATE_SAFE_OP;
    ecx_writestate(&master->context, 0);
    sm_transition(&master->sm, master_state_ready);
}

void ec_master_run_cycle(struct ec_master *master)
{
    if (!master) {
        return;
    }

    sm_dispatch(&master->sm);
}

enum ec_master_state ec_master_get_state(const struct ec_master *master)
{
    if (!master) {
        return EC_MASTER_STATE_FAULT;
    }
    return master->state_id;
}

int ec_master_get_slave_count(const struct ec_master *master)
{
    if (!master) {
        return -1;
    }
    return master->group[0].slave_count;
}

int ec_master_get_slave_info(const struct ec_master *master, uint16_t position,
                             struct ec_slave_info *info)
{
    if (!master || !info) {
        return -1;
    }
    if (position == 0 || position > (uint16_t)master->context.slavecount) {
        return -1;
    }

    *info = master->group[0].slaves[position].info;
    /* 实时刷新状态 */
    info->state = master->context.slavelist[position].state;

    return 0;
}

int64_t ec_master_get_dc_time(const struct ec_master *master)
{
    if (!master) {
        return 0;
    }
    return master->dc_time;
}
