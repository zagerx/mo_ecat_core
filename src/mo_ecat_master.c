/**
 * @file mo_ecat_master.c
 * @brief 核心层主站实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat_master_priv.h"
#include "mo_ecat_master_states.h"

static void config_free(struct mo_ecat_config *config);

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
            struct mo_ecat_pdo_ref *ref = &master->pdo_refs[idx++];
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

static int master_cycle_state_allows_io(enum mo_ecat_master_state state)
{
    return state == MO_ECAT_MASTER_STATE_RUNNING ||
           state == MO_ECAT_MASTER_STATE_DEGRADED;
}

static int master_submit_command(struct mo_ecat_master *master,
                                 enum mo_ecat_master_command command)
{
    if (!master || command == MO_ECAT_MASTER_CMD_NONE ||
        master->command_pending) {
        return -1;
    }

    master->command = command;
    master->command_pending = 1;
    master->command_result = 0;
    return 0;
}

void mo_ecat_master_clear_command(struct mo_ecat_master *master, int result)
{
    if (!master) {
        return;
    }

    if (master->command == MO_ECAT_MASTER_CMD_CONFIGURE) {
        master->pending_config = NULL;
        master->pending_backend = NULL;
    }

    master->command = MO_ECAT_MASTER_CMD_NONE;
    master->command_pending = 0;
    master->command_result = result;
}

void mo_ecat_master_release_resources(struct mo_ecat_master *master)
{
    if (!master) {
        return;
    }

    if (master->backend.ops && master->backend.ops->close) {
        master->backend.ops->close(&master->backend);
    }

    config_free(&master->config);
    free(master->slaves);
    free(master->diagnostics);
    free(master->pdo_refs);

    memset(&master->backend, 0, sizeof(master->backend));
    memset(&master->image, 0, sizeof(master->image));
    master->slaves = NULL;
    master->diagnostics = NULL;
    master->pdo_refs = NULL;
    master->pdo_ref_count = 0;
    master->cycle_result_pending = 0;
    master->cycle_abnormal = 0;
    master->consecutive_cycle_errors = 0;
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

    master->cycle_result_pending = 1;
    master->cycle_abnormal = master->cycle_abnormal || abnormal;
}

int mo_ecat_master_take_cycle_result(struct mo_ecat_master *master,
                                     int *abnormal)
{
    if (!master || !abnormal || !master->cycle_result_pending) {
        return 0;
    }

    *abnormal = master->cycle_abnormal;
    master->cycle_result_pending = 0;
    master->cycle_abnormal = 0;
    return 1;
}

struct mo_ecat_master *mo_ecat_master_create(void)
{
    struct mo_ecat_master *master =
        (struct mo_ecat_master *)calloc(1, sizeof(struct mo_ecat_master));
    if (!master) {
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

    if (master_cycle_state_allows_io(master->state)) {
        (void)mo_ecat_master_backend_deactivate(master);
    }

    if (master->state != MO_ECAT_MASTER_STATE_IDLE) {
        mo_ecat_master_release_resources(master);
    }

    free(master);
}

int mo_ecat_master_reset(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->state == MO_ECAT_MASTER_STATE_IDLE) {
        return 0;
    }

    return master_submit_command(master, MO_ECAT_MASTER_CMD_RESET);
}

void mo_ecat_master_dispatch(struct mo_ecat_master *master)
{
    if (master) {
        sm_dispatch(&master->sm);
    }
}

int mo_ecat_master_configure(struct mo_ecat_master *master,
                             const struct mo_ecat_config *config,
                             struct mo_ecat_backend *backend)
{
    if (!master || !config || !backend) {
        return -1;
    }

    if (master->command_pending) {
        return -1;
    }

    master->pending_config = config;
    master->pending_backend = backend;

    if (master_submit_command(master, MO_ECAT_MASTER_CMD_CONFIGURE) < 0) {
        master->pending_config = NULL;
        master->pending_backend = NULL;
        return -1;
    }

    return 0;
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

    if (config_copy(&master->config, config) < 0) {
        goto fail;
    }

    master->pdo_ref_count = count_pdo_refs(config);
    if (master->pdo_ref_count > 0) {
        master->pdo_refs = (struct mo_ecat_pdo_ref *)calloc(
            master->pdo_ref_count, sizeof(struct mo_ecat_pdo_ref));
        if (!master->pdo_refs) {
            goto fail;
        }
        build_pdo_refs(master, config);
    }

    master->slaves = (struct mo_ecat_slave *)calloc(
        config->slave_count, sizeof(struct mo_ecat_slave));
    master->diagnostics = (struct mo_ecat_slave_state *)calloc(
        config->slave_count, sizeof(struct mo_ecat_slave_state));
    if (config->slave_count > 0 && (!master->slaves || !master->diagnostics)) {
        goto fail;
    }

    /* 复制后端实例 */
    memcpy(&master->backend, backend, sizeof(struct mo_ecat_backend));

    return 0;

fail:
    mo_ecat_master_release_resources(master);
    return -1;
}

int mo_ecat_master_backend_configure(struct mo_ecat_master *master)
{
    if (!master || !master->backend.ops || !master->backend.ops->open ||
        !master->backend.ops->configure) {
        return -1;
    }

    int rc = master->backend.ops->open(&master->backend, &master->config);
    if (rc < 0) {
        return rc;
    }

    rc = master->backend.ops->configure(&master->backend,
                                        &master->config,
                                        &master->image,
                                        master->pdo_refs,
                                        master->pdo_ref_count,
                                        master->slaves,
                                        master->config.slave_count);
    if (rc < 0) {
        return rc;
    }

    /* 后端已填充 PDO refs 的 generation，未填充的统一刷一次 */
    for (size_t i = 0; i < master->pdo_ref_count; ++i) {
        if (master->pdo_refs[i].generation == 0) {
            master->pdo_refs[i].generation = master->image.generation;
        }
    }

    /* 刷新从站诊断状态 */
    if (master->backend.ops->read_diagnostics) {
        master->backend.ops->read_diagnostics(&master->backend,
                                              master->diagnostics,
                                              master->config.slave_count);
        for (size_t i = 0; i < master->config.slave_count; ++i) {
            master->slaves[i].state = master->diagnostics[i];
        }
    }

    return 0;
}

int mo_ecat_master_activate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->state != MO_ECAT_MASTER_STATE_READY) {
        return -1;
    }

    return master_submit_command(master, MO_ECAT_MASTER_CMD_ACTIVATE);
}

int mo_ecat_master_backend_activate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->backend.ops && master->backend.ops->activate) {
        return master->backend.ops->activate(&master->backend);
    }

    return 0;
}

int mo_ecat_master_deactivate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (!master_cycle_state_allows_io(master->state)) {
        return -1;
    }

    return master_submit_command(master, MO_ECAT_MASTER_CMD_DEACTIVATE);
}

int mo_ecat_master_backend_deactivate(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->backend.ops && master->backend.ops->deactivate) {
        return master->backend.ops->deactivate(&master->backend);
    }

    return 0;
}

int mo_ecat_master_cycle_begin(struct mo_ecat_master *master,
                               struct mo_ecat_cycle_result *result)
{
    if (!master || !result) {
        return -1;
    }

    if (!master_cycle_state_allows_io(master->state)) {
        return -1;
    }

    memset(result, 0, sizeof(*result));

    if (!master->backend.ops || !master->backend.ops->cycle_begin) {
        return -1;
    }

    int rc = master->backend.ops->cycle_begin(&master->backend, result);
    master_handle_cycle_result(master, rc, result);
    if (rc < 0) {
        return rc;
    }

    return 0;
}

int mo_ecat_master_cycle_end(struct mo_ecat_master *master,
                             struct mo_ecat_cycle_result *result)
{
    if (!master || !result) {
        return -1;
    }

    if (!master_cycle_state_allows_io(master->state)) {
        return -1;
    }

    if (!master->backend.ops || !master->backend.ops->cycle_end) {
        return -1;
    }

    int rc = master->backend.ops->cycle_end(&master->backend, result);
    master_handle_cycle_result(master, rc, result);
    return rc;
}

int mo_ecat_master_read_diagnostics(struct mo_ecat_master *master)
{
    if (!master) {
        return -1;
    }

    if (master->state != MO_ECAT_MASTER_STATE_READY &&
        master->state != MO_ECAT_MASTER_STATE_RUNNING &&
        master->state != MO_ECAT_MASTER_STATE_DEGRADED &&
        master->state != MO_ECAT_MASTER_STATE_FAULT) {
        return -1;
    }

    if (!master->backend.ops || !master->backend.ops->read_diagnostics) {
        return -1;
    }

    int rc = master->backend.ops->read_diagnostics(&master->backend,
                                                   master->diagnostics,
                                                   master->config.slave_count);
    if (rc < 0) {
        return rc;
    }

    for (size_t i = 0; i < master->config.slave_count; ++i) {
        master->slaves[i].state = master->diagnostics[i];
    }

    return 0;
}

enum mo_ecat_master_state mo_ecat_master_get_state(
    const struct mo_ecat_master *master)
{
    return master ? master->state : MO_ECAT_MASTER_STATE_INIT;
}

size_t mo_ecat_master_get_slave_count(const struct mo_ecat_master *master)
{
    return master ? master->config.slave_count : 0;
}

const struct mo_ecat_slave *mo_ecat_master_get_slave(
    const struct mo_ecat_master *master, size_t index)
{
    if (!master || index >= master->config.slave_count) {
        return NULL;
    }
    return &master->slaves[index];
}

size_t mo_ecat_master_get_pdo_ref_count(const struct mo_ecat_master *master)
{
    return master ? master->pdo_ref_count : 0;
}

const struct mo_ecat_pdo_ref *mo_ecat_master_get_pdo_ref(
    const struct mo_ecat_master *master, size_t index)
{
    if (!master || index >= master->pdo_ref_count) {
        return NULL;
    }
    return &master->pdo_refs[index];
}

int mo_ecat_master_get_process_image(const struct mo_ecat_master *master,
                                     const uint8_t **memory,
                                     size_t *size)
{
    if (!master || !memory || !size) {
        return -1;
    }

    if (master->state != MO_ECAT_MASTER_STATE_READY &&
        master->state != MO_ECAT_MASTER_STATE_RUNNING &&
        master->state != MO_ECAT_MASTER_STATE_DEGRADED) {
        return -1;
    }

    *memory = master->image.memory;
    *size   = master->image.size;
    return 0;
}

const void *mo_ecat_pdo_input(const struct mo_ecat_master *master,
                              const struct mo_ecat_pdo_ref *ref)
{
    if (!master || !ref) {
        return NULL;
    }

    if (ref->direction != MO_ECAT_PDO_INPUT) {
        return NULL;
    }

    if (ref->generation != master->image.generation ||
        !pdo_ref_in_bounds(&master->image, ref)) {
        return NULL;
    }

    return &master->image.memory[ref->byte_offset];
}

void *mo_ecat_pdo_output(struct mo_ecat_master *master,
                         const struct mo_ecat_pdo_ref *ref)
{
    if (!master || !ref) {
        return NULL;
    }

    if (ref->direction != MO_ECAT_PDO_OUTPUT) {
        return NULL;
    }

    if (ref->generation != master->image.generation ||
        !pdo_ref_in_bounds(&master->image, ref)) {
        return NULL;
    }

    return &master->image.memory[ref->byte_offset];
}

void mo_ecat_master_set_user_data(struct mo_ecat_master *master,
                                  void *user_data)
{
    if (master) {
        master->user_data = user_data;
    }
}

void *mo_ecat_master_get_user_data(const struct mo_ecat_master *master)
{
    return master ? master->user_data : NULL;
}
