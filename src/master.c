/**
 * @file mo_ecat_master.c
 * @brief 主站对象、生命周期与命令接口
 */

#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "master_priv.h"

static int s_master_instance_in_use;

enum mo_ecat_master_state
master_state_from_sm(const struct mo_ecat_master *master)
{
    sm_state_t current_state;

    if (!master) {
        return MO_ECAT_MASTER_STATE_INIT;
    }

    current_state = master->sm.current_state;
    if (current_state == master_state_idle) {
        return MO_ECAT_MASTER_STATE_IDLE;
    }
    if (current_state == master_state_discovered) {
        return MO_ECAT_MASTER_STATE_DISCOVERED;
    }
    if (current_state == master_state_ready) {
        return MO_ECAT_MASTER_STATE_READY;
    }
    if (current_state == master_state_running) {
        return MO_ECAT_MASTER_STATE_RUNNING;
    }
    if (current_state == master_state_degraded) {
        return MO_ECAT_MASTER_STATE_DEGRADED;
    }
    if (current_state == master_state_fault) {
        return MO_ECAT_MASTER_STATE_FAULT;
    }

    return MO_ECAT_MASTER_STATE_INIT;
}

int master_state_allows_io(const struct mo_ecat_master *master)
{
    enum mo_ecat_master_state state = master_state_from_sm(master);

    return state == MO_ECAT_MASTER_STATE_RUNNING ||
           state == MO_ECAT_MASTER_STATE_DEGRADED;
}

void master_request_command(struct mo_ecat_master *master,
                            enum mo_ecat_master_command command)
{
    if (!master) {
        return;
    }

    master->command = command;
}

int mo_ecat_master_init(struct mo_ecat_master *master,
                        const struct mo_ecat_master_options *options)
{
    if (!master || !options || options->interface_name[0] == '\0') {
        return -1;
    }

    memset(master, 0, sizeof(*master));
    if (pthread_mutex_init(&master->lock, NULL) != 0) {
        return -1;
    }

    master->options = *options;
    statemachine_init(&master->sm, master, master_state_init);
    return 0;
}

void mo_ecat_master_deinit(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    pthread_mutex_lock(&master->lock);
    if (master_state_allows_io(master)) {
        (void)master_backend_deactivate(master);
    }
    master_release_resources(master);
    pthread_mutex_unlock(&master->lock);

    pthread_mutex_destroy(&master->lock);
    memset(master, 0, sizeof(*master));
}

struct mo_ecat_master *mo_ecat_master_create(
    const struct mo_ecat_master_options *options)
{
    struct mo_ecat_master *master;

    if (s_master_instance_in_use) {
        return NULL;
    }

    master = calloc(1, sizeof(*master));
    if (!master) {
        return NULL;
    }

    if (mo_ecat_master_init(master, options) < 0) {
        free(master);
        return NULL;
    }

    s_master_instance_in_use = 1;
    return master;
}

void mo_ecat_master_destroy(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    mo_ecat_master_deinit(master);
    free(master);
    s_master_instance_in_use = 0;
}

int mo_ecat_master_reset(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);
    if (master_state_from_sm(master) == MO_ECAT_MASTER_STATE_IDLE) {
        pthread_mutex_unlock(&master->lock);
        return 0;
    }

    master_request_command(master, MO_ECAT_MASTER_CMD_RESET);
    pthread_mutex_unlock(&master->lock);
    return 0;
}

void mo_ecat_master_dispatch(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    pthread_mutex_lock(&master->lock);
    sm_dispatch(&master->sm);
    pthread_mutex_unlock(&master->lock);
}

int mo_ecat_master_start(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);
    master_request_command(master, MO_ECAT_MASTER_CMD_DISCOVER);
    pthread_mutex_unlock(&master->lock);
    return 0;
}

int mo_ecat_master_activate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);
    if (master_state_from_sm(master) != MO_ECAT_MASTER_STATE_READY) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    master_request_command(master, MO_ECAT_MASTER_CMD_ACTIVATE);
    pthread_mutex_unlock(&master->lock);
    return 0;
}

int mo_ecat_master_deactivate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);
    if (!master_state_allows_io(master)) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    master_request_command(master, MO_ECAT_MASTER_CMD_DEACTIVATE);
    pthread_mutex_unlock(&master->lock);
    return 0;
}

enum mo_ecat_master_state mo_ecat_master_get_state(
    const struct mo_ecat_master *master)
{
    enum mo_ecat_master_state state;

    if (!master) {
        return MO_ECAT_MASTER_STATE_INIT;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    state = master_state_from_sm(master);
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return state;
}

void mo_ecat_master_set_user_data(struct mo_ecat_master *master,
                                  void *user_data)
{
    if (!master) {
        return;
    }

    pthread_mutex_lock(&master->lock);
    master->user_data = user_data;
    pthread_mutex_unlock(&master->lock);
}

void *mo_ecat_master_get_user_data(const struct mo_ecat_master *master)
{
    void *user_data;

    if (!master) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    user_data = master->user_data;
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return user_data;
}
