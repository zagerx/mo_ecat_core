/**
 * @file ecat_master.c
 * @brief 基于 SOEM 的 C 主站对象实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mo_ecat/ecat_master.h"
#include "statemachine/statemachine.h"

/* SOEM 相关常量已包含在 soem.h 中 */

#ifdef __GNUC__
#define EC_MASTER_UNUSED __attribute__((unused))
#else
#define EC_MASTER_UNUSED
#endif

/* 前向声明 */
static void master_state_init(struct statemachine *sm);
static void master_state_ready(struct statemachine *sm);
static void master_state_running(struct statemachine *sm);
static void master_state_fault(struct statemachine *sm);
static void EC_MASTER_UNUSED master_state_controlled(struct statemachine *sm);

static const char *state_name(ec_master_state_id_t id)
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

static void master_set_state(ec_master_t *master, ec_master_state_id_t id)
{
    if (!master) {
        return;
    }
    master->state_id = id;
    printf("[EC Master] enter state: %s\n", state_name(id));
}

static void master_enter_fault(ec_master_t *master, const char *reason)
{
    fprintf(stderr, "[EC Master] FAULT: %s\n", reason);
    sm_transition(&master->sm, master_state_fault);
}

static int master_alloc_slave_array(ec_master_t *master)
{
    int count = master->context.slavecount;

    master->group[0].slave_count = count;
    if (count <= 0) {
        return 0;
    }

    /* SOEM 从站编号从 1 开始，数组长度 +1 */
    master->group[0].slaves = (struct slave *)calloc((size_t)(count + 1),
                                                     sizeof(struct slave));
    if (!master->group[0].slaves) {
        return -1;
    }

    for (int i = 1; i <= count; ++i) {
        struct slave_info *info = &master->group[0].slaves[i].info;
        info->position    = (uint16_t)i;
        info->alias       = master->context.slavelist[i].aliasadr;
        info->vendor_id   = master->context.slavelist[i].eep_man;
        info->product_code= master->context.slavelist[i].eep_id;
        info->state       = master->context.slavelist[i].state;
        strncpy(info->name, (const char *)master->context.slavelist[i].name,
                EC_MAXNAME);
        info->name[EC_MAXNAME] = '\0';
    }

    return 0;
}

static void master_free_resources(ec_master_t *master)
{
    if (!master) {
        return;
    }

    ecx_close(&master->context);

    if (master->iomap) {
        free(master->iomap);
        master->iomap = NULL;
        master->iomap_size = 0;
    }

    if (master->group[0].slaves) {
        free(master->group[0].slaves);
        master->group[0].slaves = NULL;
        master->group[0].slave_count = 0;
    }
}

/* ==================== 状态处理函数 ==================== */

static void master_state_init(struct statemachine *sm)
{
    ec_master_t *master = (ec_master_t *)sm->data;

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

        if (master_alloc_slave_array(master) < 0) {
            master_enter_fault(master, "failed to allocate slave array");
            return;
        }

        printf("[EC Master] %d slave(s) found\n", master->context.slavecount);

        master->iomap = (uint8_t *)malloc(EC_MASTER_IOMAP_SIZE);
        if (!master->iomap) {
            master_enter_fault(master, "failed to allocate IOmap");
            return;
        }

        int iomap_size = ecx_config_map_group(&master->context,
                                              master->iomap, 0);
        if (iomap_size <= 0) {
            master_enter_fault(master, "ecx_config_map_group failed");
            return;
        }
        master->iomap_size = (size_t)iomap_size;

        printf("[EC Master] IOmap size: %zu bytes\n", master->iomap_size);

        /* 配置 DC（可选） */
        if (master->dc_enabled) {
            ecx_configdc(&master->context);
        }

        /* 初始化完成，进入准备状态 */
        sm_transition(&master->sm, master_state_ready);
    }
}

static void master_state_ready(struct statemachine *sm)
{
    ec_master_t *master = (ec_master_t *)sm->data;

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
    ec_master_t *master = (ec_master_t *)sm->data;

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

static void master_state_fault(struct statemachine *sm)
{
    ec_master_t *master = (ec_master_t *)sm->data;

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
    ec_master_t *master = (ec_master_t *)sm->data;

    if (sm->phase == ENTER) {
        master_set_state(master, EC_MASTER_STATE_CONTROLLED);
        sm->phase = USER_STATUS;
    }
    /* 受控状态下的具体行为由外部控制器决定，占位 */
}

/* ==================== 公共 API ==================== */

ec_master_t *ec_master_create(const char *ifname)
{
    if (!ifname) {
        return NULL;
    }

    ec_master_t *master = (ec_master_t *)calloc(1, sizeof(ec_master_t));
    if (!master) {
        return NULL;
    }

    strncpy(master->ifname, ifname, EC_MASTER_IFNAME_SIZE - 1);
    master->ifname[EC_MASTER_IFNAME_SIZE - 1] = '\0';
    master->dc_enabled = 0;

    statemachine_init(&master->sm, master, master_state_init);

    return master;
}

void ec_master_destroy(ec_master_t *master)
{
    if (!master) {
        return;
    }

    master_free_resources(master);
    free(master);
}

int ec_master_start(ec_master_t *master)
{
    if (!master) {
        return -1;
    }

    /* 触发一次状态机调度，执行 init 状态 */
    ec_master_run_cycle(master);
    return 0;
}

void ec_master_stop(ec_master_t *master)
{
    if (!master) {
        return;
    }

    /* 从运行态回到准备态，并请求 SAFE_OP */
    master->context.slavelist[0].state = EC_STATE_SAFE_OP;
    ecx_writestate(&master->context, 0);
    sm_transition(&master->sm, master_state_ready);
}

void ec_master_run_cycle(ec_master_t *master)
{
    if (!master) {
        return;
    }

    sm_dispatch(&master->sm);
}

ec_master_state_id_t ec_master_get_state(const ec_master_t *master)
{
    if (!master) {
        return EC_MASTER_STATE_FAULT;
    }
    return master->state_id;
}

int ec_master_get_slave_info(const ec_master_t *master, uint16_t position,
                             struct slave_info *info)
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
