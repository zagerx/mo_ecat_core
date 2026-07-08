/**
 * @file ecat_slave.c
 * @brief 从站模块：从站信息分配、填充、释放
 */

#include <stdlib.h>
#include <string.h>

#include "ecat_master_priv.h"

int ecat_slave_array_alloc(struct slave_group *group, int slave_count)
{
    if (!group || slave_count < 0) {
        return -1;
    }

    group->slave_count = slave_count;
    if (slave_count == 0) {
        group->slaves = NULL;
        return 0;
    }

    /* SOEM 从站编号从 1 开始，数组长度 +1 */
    group->slaves = (struct slave *)calloc((size_t)(slave_count + 1),
                                           sizeof(struct slave));
    if (!group->slaves) {
        group->slave_count = 0;
        return -1;
    }

    return 0;
}

void ecat_slave_array_free(struct slave_group *group)
{
    if (!group) {
        return;
    }

    if (group->slaves) {
        free(group->slaves);
        group->slaves = NULL;
    }
    group->slave_count = 0;
}

void ecat_slave_fill_info(struct ec_slave_info *info, const ec_slavet *slave)
{
    if (!info || !slave) {
        return;
    }

    info->alias        = slave->aliasadr;
    info->vendor_id    = slave->eep_man;
    info->product_code = slave->eep_id;
    info->state        = slave->state;
    strncpy(info->name, (const char *)slave->name, EC_MASTER_MAX_NAME_LEN);
    info->name[EC_MASTER_MAX_NAME_LEN] = '\0';
}
