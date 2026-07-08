/**
 * @file ecat_slave_group.c
 * @brief 从站组模块：从站组管理、PDO IOmap 映射
 */

#include <stdio.h>
#include <stdlib.h>

#include "ecat_master_priv.h"

int ecat_slave_group_init(struct slave_group *group, ecx_contextt *context)
{
    if (!group || !context) {
        return -1;
    }

    int count = context->slavecount;

    if (ecat_slave_array_alloc(group, count) < 0) {
        return -1;
    }

    for (int i = 1; i <= count; ++i) {
        struct ec_slave_info *info = &group->slaves[i].info;
        info->position = (uint16_t)i;
        ecat_slave_fill_info(info, &context->slavelist[i]);
    }

    return 0;
}

void ecat_slave_group_cleanup(struct slave_group *group)
{
    if (!group) {
        return;
    }

    ecat_slave_array_free(group);
}

int ecat_slave_group_map(struct slave_group *group, ecx_contextt *context,
                         uint8_t **iomap, size_t *iomap_size)
{
    if (!group || !context || !iomap || !iomap_size) {
        return -1;
    }

    *iomap = (uint8_t *)malloc(EC_MASTER_IOMAP_SIZE);
    if (!*iomap) {
        return -1;
    }

    int size = ecx_config_map_group(context, *iomap, 0);
    if (size <= 0) {
        free(*iomap);
        *iomap = NULL;
        return -1;
    }

    *iomap_size = (size_t)size;
    printf("[EC Master] IOmap size: %zu bytes\n", *iomap_size);

    return 0;
}
