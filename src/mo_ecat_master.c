/**
 * @file mo_ecat_master.c
 * @brief 核心层主站实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "mo_ecat_backend.h"
#include "mo_ecat_master_priv.h"
#include "mo_ecat_master_states.h"

static void config_free(struct mo_ecat_config *config);

/**
 * @brief 根据状态机当前函数指针推断主站生命周期状态
 */
static enum mo_ecat_master_state master_state_from_sm(const struct mo_ecat_master *master)
{
    if (!master) {
        return MO_ECAT_MASTER_STATE_INIT;
    }

    sm_state_t cur = master->sm.current_state;

    if (cur == mo_ecat_master_state_idle) {
        return MO_ECAT_MASTER_STATE_IDLE;
    }
    if (cur == mo_ecat_master_state_ready) {
        return MO_ECAT_MASTER_STATE_READY;
    }
    if (cur == mo_ecat_master_state_running) {
        return MO_ECAT_MASTER_STATE_RUNNING;
    }
    if (cur == mo_ecat_master_state_degraded) {
        return MO_ECAT_MASTER_STATE_DEGRADED;
    }
    if (cur == mo_ecat_master_state_fault) {
        return MO_ECAT_MASTER_STATE_FAULT;
    }

    /* 默认视为 INIT（包含初始状态和未知状态） */
    return MO_ECAT_MASTER_STATE_INIT;
}

static int config_copy(struct mo_ecat_config *dst,
                       const struct mo_ecat_config *src)
{
    if (!dst || !src) {
        return -1;
    }

    memset(dst, 0, sizeof(*dst));

    dst->interface_name = strdup(src->interface_name ? src->interface_name : "");
    if (src->interface_name && !dst->interface_name) {
        return -1;
    }

    dst->slave_count = src->slave_count;
    if (dst->slave_count > 0) {
        dst->slaves = (struct mo_ecat_slave_config *)calloc(
            dst->slave_count, sizeof(struct mo_ecat_slave_config));
        if (!dst->slaves) {
            free((void *)dst->interface_name);
            dst->interface_name = NULL;
            return -1;
        }

        for (size_t i = 0; i < dst->slave_count; ++i) {
            const struct mo_ecat_slave_config *s = &src->slaves[i];
            struct mo_ecat_slave_config *d =
                (struct mo_ecat_slave_config *)&dst->slaves[i];

            d->position      = s->position;
            d->alias         = s->alias;
            d->vendor_id     = s->vendor_id;
            d->product_code  = s->product_code;
            d->revision_number = s->revision_number;
            d->dc_active     = s->dc_active;

            if (s->name) {
                d->name = strdup(s->name);
                if (!d->name) {
                    config_free(dst);
                    return -1;
                }
            }

            d->pdo_entry_count = s->pdo_entry_count;
            if (d->pdo_entry_count > 0) {
                d->pdo_entries = (struct mo_ecat_pdo_entry_config *)calloc(
                    d->pdo_entry_count, sizeof(struct mo_ecat_pdo_entry_config));
                if (!d->pdo_entries) {
                    config_free(dst);
                    return -1;
                }
                memcpy((void *)d->pdo_entries, s->pdo_entries,
                       d->pdo_entry_count * sizeof(struct mo_ecat_pdo_entry_config));
            }
        }
    }

    return 0;
}

static void config_free(struct mo_ecat_config *config)
{
    if (!config) {
        return;
    }

    if (config->slaves) {
        for (size_t i = 0; i < config->slave_count; ++i) {
            free((void *)config->slaves[i].name);
            free((void *)config->slaves[i].pdo_entries);
        }
        free((void *)config->slaves);
        config->slaves = NULL;
    }

    free((void *)config->interface_name);
    config->interface_name = NULL;
}

static int backend_create(const struct mo_ecat_backend_config *config,
                          struct mo_ecat_backend *backend)
{
    if (!config || !backend) {
        return -1;
    }

    memset(backend, 0, sizeof(*backend));
    return mo_ecat_backend_init(backend, config);
}

static void backend_destroy(struct mo_ecat_backend *backend)
{
    if (!backend) {
        return;
    }

    if (backend->ops && backend->ops->close) {
        backend->ops->close(backend);
    }

    memset(backend, 0, sizeof(*backend));
}

static size_t count_pdo_refs(const struct mo_ecat_config *config)
{
    size_t count = 0;
    for (size_t i = 0; i < config->slave_count; ++i) {
        count += config->slaves[i].pdo_entry_count;
    }
    return count;
}

static void build_pdo_refs(struct mo_ecat_master *master,
                           const struct mo_ecat_config *config)
{
    size_t idx = 0;
    for (size_t i = 0; i < config->slave_count; ++i) {
        const struct mo_ecat_slave_config *slave = &config->slaves[i];
        for (size_t j = 0; j < slave->pdo_entry_count; ++j) {
            struct mo_ecat_pdo_ref *ref = &master->pdo.refs[idx++];
            const struct mo_ecat_pdo_entry_config *entry = &slave->pdo_entries[j];

            ref->slave_index  = i;
            ref->pdo_index    = entry->index;
            ref->entry_index  = entry->subindex;
            ref->direction    = entry->direction;
            ref->bit_length   = entry->bit_length;
            ref->byte_offset  = 0;
            ref->bit_offset   = 0;
            ref->generation   = 0; /* backend 完成映射后填充 */
        }
    }
}

static int master_cycle_state_allows_io(const struct mo_ecat_master *master)
{
    enum mo_ecat_master_state state = master_state_from_sm(master);
    return state == MO_ECAT_MASTER_STATE_RUNNING ||
           state == MO_ECAT_MASTER_STATE_DEGRADED;
}

static int master_submit_command(struct mo_ecat_master *master,
                                 enum mo_ecat_master_command command)
{
    if (!master || command == MO_ECAT_MASTER_CMD_NONE ||
        master->cmd.pending) {
        return -1;
    }

    master->cmd.id = command;
    master->cmd.pending = 1;
    master->cmd.result = 0;
    return 0;
}

void mo_ecat_master_clear_command(struct mo_ecat_master *master, int result)
{
    if (!master) {
        return;
    }

    if (master->cmd.id == MO_ECAT_MASTER_CMD_CONFIGURE) {
        master->cmd.pending_config = NULL;
        /* 如果后端实例尚未转移到运行时资源，在这里释放，避免泄漏 */
        backend_destroy(&master->cmd.pending_backend_value);
    }

    master->cmd.id = MO_ECAT_MASTER_CMD_NONE;
    master->cmd.pending = 0;
    master->cmd.result = result;
}

void mo_ecat_master_release_resources(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    if (master->rt.backend.ops && master->rt.backend.ops->close) {
        master->rt.backend.ops->close(&master->rt.backend);
    }

    config_free(&master->rt.config);
    free(master->diag.slaves);
    free(master->diag.states);
    free(master->pdo.refs);

    memset(&master->rt.backend, 0, sizeof(master->rt.backend));
    memset(&master->rt.image, 0, sizeof(master->rt.image));
    master->diag.slaves = NULL;
    master->diag.states = NULL;
    master->diag.count = 0;
    master->pdo.refs = NULL;
    master->pdo.count = 0;
    master->cycle.result_pending = 0;
    master->cycle.abnormal = 0;
    master->cycle.consecutive_errors = 0;
}

static int pdo_ref_in_bounds(const struct mo_ecat_process_image *image,
                             const struct mo_ecat_pdo_ref *ref)
{
    if (!image || !ref || !image->memory || ref->bit_length == 0) {
        return 0;
    }

    if (ref->byte_offset >= image->size) {
        return 0;
    }

    size_t image_bits = image->size * 8U;
    size_t start_bit = (size_t)ref->byte_offset * 8U + ref->bit_offset;
    size_t end_bit = start_bit + ref->bit_length;

    return end_bit >= start_bit && end_bit <= image_bits;
}

static void master_handle_cycle_result(struct mo_ecat_master *master,
                                       int rc,
                                       struct mo_ecat_cycle_result *result)
{
    int abnormal = (rc < 0);

    if (result) {
        if (!result->link_up) {
            abnormal = 1;
        }
        if (result->expected_wkc > 0 &&
            result->actual_wkc != result->expected_wkc) {
            abnormal = 1;
        }
        if (abnormal) {
            result->diagnostics_required = 1;
        }
    }

    master->cycle.result_pending = 1;
    master->cycle.abnormal = master->cycle.abnormal || abnormal;
}

int mo_ecat_master_take_cycle_result(struct mo_ecat_master *master,
                                     int *abnormal)
{
    if (!master || !abnormal || !master->cycle.result_pending) {
        return 0;
    }

    *abnormal = master->cycle.abnormal;
    master->cycle.result_pending = 0;
    master->cycle.abnormal = 0;
    return 1;
}

struct mo_ecat_master *mo_ecat_master_create(void)
{
    struct mo_ecat_master *master =
        (struct mo_ecat_master *)calloc(1, sizeof(struct mo_ecat_master));
    if (!master) {
        return NULL;
    }

    if (pthread_mutex_init(&master->lock, NULL) != 0) {
        free(master);
        return NULL;
    }

    statemachine_init(&master->sm, master, mo_ecat_master_state_init);

    return master;
}

void mo_ecat_master_destroy(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    pthread_mutex_lock(&master->lock);

    if (master_cycle_state_allows_io(master)) {
        (void)mo_ecat_master_backend_deactivate(master);
    }

    if (master_state_from_sm(master) != MO_ECAT_MASTER_STATE_IDLE) {
        mo_ecat_master_release_resources(master);
    }

    pthread_mutex_unlock(&master->lock);
    pthread_mutex_destroy(&master->lock);

    free(master);
}

int mo_ecat_master_reset(struct mo_ecat_master *master)
{
    int rc;

    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (master_state_from_sm(master) == MO_ECAT_MASTER_STATE_IDLE) {
        pthread_mutex_unlock(&master->lock);
        return 0;
    }

    rc = master_submit_command(master, MO_ECAT_MASTER_CMD_RESET);
    pthread_mutex_unlock(&master->lock);
    return rc;
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

int mo_ecat_master_configure(struct mo_ecat_master *master,
                             const struct mo_ecat_config *config,
                             const struct mo_ecat_backend_config *backend_config)
{
    int rc;
    struct mo_ecat_backend backend;

    if (!master || !config || !backend_config) {
        return -1;
    }

    if (backend_create(backend_config, &backend) < 0) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (master->cmd.pending) {
        pthread_mutex_unlock(&master->lock);
        backend_destroy(&backend);
        return -1;
    }

    master->cmd.pending_config = config;
    master->cmd.pending_backend_value = backend;

    rc = master_submit_command(master, MO_ECAT_MASTER_CMD_CONFIGURE);
    if (rc < 0) {
        master->cmd.pending_config = NULL;
        backend_destroy(&master->cmd.pending_backend_value);
        memset(&master->cmd.pending_backend_value, 0,
               sizeof(master->cmd.pending_backend_value));
    }

    pthread_mutex_unlock(&master->lock);
    return rc;
}

int mo_ecat_master_prepare_config(struct mo_ecat_master *master,
                                  const struct mo_ecat_config *config,
                                  struct mo_ecat_backend *backend)
{
    if (!master || !config || !backend) {
        return -1;
    }

    if (!backend->ops || !backend->ops->open || !backend->ops->configure) {
        return -1;
    }

    mo_ecat_master_release_resources(master);

    if (config_copy(&master->rt.config, config) < 0) {
        goto fail;
    }

    master->diag.count = config->slave_count;
    master->pdo.count = count_pdo_refs(config);
    if (master->pdo.count > 0) {
        master->pdo.refs = (struct mo_ecat_pdo_ref *)calloc(
            master->pdo.count, sizeof(struct mo_ecat_pdo_ref));
        if (!master->pdo.refs) {
            goto fail;
        }
        build_pdo_refs(master, config);
    }

    master->diag.slaves = (struct mo_ecat_slave *)calloc(
        master->diag.count, sizeof(struct mo_ecat_slave));
    master->diag.states = (struct mo_ecat_slave_state *)calloc(
        master->diag.count, sizeof(struct mo_ecat_slave_state));
    if (master->diag.count > 0 && (!master->diag.slaves || !master->diag.states)) {
        goto fail;
    }

    /* 复制后端实例，所有权从命令槽转移到运行时资源 */
    memcpy(&master->rt.backend, backend, sizeof(struct mo_ecat_backend));
    master->cmd.pending_backend_value.ctx = NULL;
    master->cmd.pending_backend_value.ops = NULL;

    return 0;

fail:
    mo_ecat_master_release_resources(master);
    return -1;
}

int mo_ecat_master_backend_configure(struct mo_ecat_master *master)
{
    if (!master || !master->rt.backend.ops || !master->rt.backend.ops->open ||
        !master->rt.backend.ops->configure) {
        return -1;
    }

    int rc = master->rt.backend.ops->open(&master->rt.backend, &master->rt.config);
    if (rc < 0) {
        return rc;
    }

    rc = master->rt.backend.ops->configure(&master->rt.backend,
                                        &master->rt.config,
                                        &master->rt.image,
                                        master->pdo.refs,
                                        master->pdo.count,
                                        master->diag.slaves,
                                        master->diag.count);
    if (rc < 0) {
        return rc;
    }

    /* 后端已填充 PDO refs 的 generation，未填充的统一刷一次 */
    for (size_t i = 0; i < master->pdo.count; ++i) {
        if (master->pdo.refs[i].generation == 0) {
            master->pdo.refs[i].generation = master->rt.image.generation;
        }
    }

    /* 刷新从站诊断状态 */
    if (master->rt.backend.ops->read_diagnostics) {
        master->rt.backend.ops->read_diagnostics(&master->rt.backend,
                                              master->diag.states,
                                              master->diag.count);
        for (size_t i = 0; i < master->diag.count; ++i) {
            master->diag.slaves[i].state = master->diag.states[i];
        }
    }

    return 0;
}

int mo_ecat_master_activate(struct mo_ecat_master *master)
{
    int rc;

    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (master_state_from_sm(master) != MO_ECAT_MASTER_STATE_READY) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    rc = master_submit_command(master, MO_ECAT_MASTER_CMD_ACTIVATE);
    pthread_mutex_unlock(&master->lock);
    return rc;
}

int mo_ecat_master_backend_activate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->rt.backend.ops && master->rt.backend.ops->activate) {
        return master->rt.backend.ops->activate(&master->rt.backend);
    }

    return 0;
}

int mo_ecat_master_deactivate(struct mo_ecat_master *master)
{
    int rc;

    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (!master_cycle_state_allows_io(master)) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    rc = master_submit_command(master, MO_ECAT_MASTER_CMD_DEACTIVATE);
    pthread_mutex_unlock(&master->lock);
    return rc;
}

int mo_ecat_master_backend_deactivate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->rt.backend.ops && master->rt.backend.ops->deactivate) {
        return master->rt.backend.ops->deactivate(&master->rt.backend);
    }

    return 0;
}

int mo_ecat_master_cycle_begin(struct mo_ecat_master *master,
                               struct mo_ecat_cycle_result *result)
{
    int rc;

    if (!master || !result) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (!master_cycle_state_allows_io(master)) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    memset(result, 0, sizeof(*result));

    if (!master->rt.backend.ops || !master->rt.backend.ops->cycle_begin) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    rc = master->rt.backend.ops->cycle_begin(&master->rt.backend, result);
    master_handle_cycle_result(master, rc, result);
    pthread_mutex_unlock(&master->lock);
    return rc;
}

int mo_ecat_master_cycle_end(struct mo_ecat_master *master,
                             struct mo_ecat_cycle_result *result)
{
    int rc;

    if (!master || !result) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (!master_cycle_state_allows_io(master)) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    if (!master->rt.backend.ops || !master->rt.backend.ops->cycle_end) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    rc = master->rt.backend.ops->cycle_end(&master->rt.backend, result);
    master_handle_cycle_result(master, rc, result);
    if (rc == 0) {
        master->cycle.last = *result;
    }
    pthread_mutex_unlock(&master->lock);
    return rc;
}

int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master)
{
    int rc;

    if (!master) {
        return -1;
    }

    pthread_mutex_lock(&master->lock);

    if (master_state_from_sm(master) != MO_ECAT_MASTER_STATE_READY &&
        master_state_from_sm(master) != MO_ECAT_MASTER_STATE_RUNNING &&
        master_state_from_sm(master) != MO_ECAT_MASTER_STATE_DEGRADED &&
        master_state_from_sm(master) != MO_ECAT_MASTER_STATE_FAULT) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    if (!master->rt.backend.ops || !master->rt.backend.ops->read_diagnostics) {
        pthread_mutex_unlock(&master->lock);
        return -1;
    }

    rc = master->rt.backend.ops->read_diagnostics(&master->rt.backend,
                                               master->diag.states,
                                               master->diag.count);
    if (rc < 0) {
        pthread_mutex_unlock(&master->lock);
        return rc;
    }

    for (size_t i = 0; i < master->diag.count; ++i) {
        master->diag.slaves[i].state = master->diag.states[i];
    }

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

size_t mo_ecat_master_get_pdo_ref_count(const struct mo_ecat_master *master)
{
    size_t count;

    if (!master) {
        return 0;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    count = master->pdo.count;
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return count;
}

const struct mo_ecat_pdo_ref *mo_ecat_master_get_pdo_ref(
    const struct mo_ecat_master *master, size_t index)
{
    const struct mo_ecat_pdo_ref *ref;

    if (!master) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    if (index >= master->pdo.count) {
        pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
        return NULL;
    }
    ref = &master->pdo.refs[index];
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return ref;
}

int mo_ecat_master_get_cycle_result(const struct mo_ecat_master *master,
                                    struct mo_ecat_cycle_result *result)
{
    if (!master || !result) {
        return -1;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);
    *result = master->cycle.last;
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return 0;
}

int mo_ecat_master_get_process_image(const struct mo_ecat_master *master,
                                     const uint8_t **memory,
                                     size_t *size)
{
    if (!master || !memory || !size) {
        return -1;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);

    if (master_state_from_sm(master) != MO_ECAT_MASTER_STATE_READY &&
        master_state_from_sm(master) != MO_ECAT_MASTER_STATE_RUNNING &&
        master_state_from_sm(master) != MO_ECAT_MASTER_STATE_DEGRADED) {
        pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
        return -1;
    }

    *memory = master->rt.image.memory;
    *size   = master->rt.image.size;
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return 0;
}

const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                              const struct mo_ecat_pdo_ref *ref)
{
    const void *p;

    if (!master || !ref) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t *)&master->lock);

    if (ref->direction != MO_ECAT_PDO_INPUT ||
        ref->generation != master->rt.image.generation ||
        !pdo_ref_in_bounds(&master->rt.image, ref)) {
        pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
        return NULL;
    }

    p = &master->rt.image.memory[ref->byte_offset];
    pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
    return p;
}

void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_ref *ref)
{
    void *p;

    if (!master || !ref) {
        return NULL;
    }

    pthread_mutex_lock(&master->lock);

    if (ref->direction != MO_ECAT_PDO_OUTPUT ||
        ref->generation != master->rt.image.generation ||
        !pdo_ref_in_bounds(&master->rt.image, ref)) {
        pthread_mutex_unlock(&master->lock);
        return NULL;
    }

    p = &master->rt.image.memory[ref->byte_offset];
    pthread_mutex_unlock(&master->lock);
    return p;
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
