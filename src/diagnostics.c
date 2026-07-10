/**
 * @file diagnostics.c
 * @brief 主站从站诊断接口
 */

#include "mo_ecat/mo_ecat_slave.h"
#include "master_priv.h"

size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master)
{
    size_t count;

    if (!master) {
        return 0;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    count = master->diag.count;
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return count;
}

const struct mo_ecat_slave *mo_ecat_master_get_slave(
    const struct mo_ecat_master *master, size_t index)
{
    const struct mo_ecat_slave *slave;

    if (!master) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    if (index >= master->diag.count) {
        pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
        return NULL;
    }
    slave = &master->diag.slaves[index];
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return slave;
}

int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master)
{
    enum mo_ecat_master_state state;
    int result;

    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);
    state = master_state_from_sm(master);
    if (state != MO_ECAT_MASTER_STATE_DISCOVERED &&
        state != MO_ECAT_MASTER_STATE_READY &&
        state != MO_ECAT_MASTER_STATE_RUNNING &&
        state != MO_ECAT_MASTER_STATE_DEGRADED &&
        state != MO_ECAT_MASTER_STATE_FAULT) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    if (!master->backend.ops || !master->backend.ops->read_diagnostics) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    result = master->backend.ops->read_diagnostics(&master->backend,
                                                   master->diag.states,
                                                   master->diag.count);
    if (result == 0) {
        for (size_t i = 0; i < master->diag.count; ++i) {
            master->diag.slaves[i].state = master->diag.states[i];
        }
    }

    pthread_mutex_unlock(&master->lock);
    return result;
}
