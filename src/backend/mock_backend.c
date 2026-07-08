/**
 * @file mock_backend.c
 * @brief Mock 后端：无硬件验证核心层契约
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mock_backend.h"
#include "mo_ecat_master_priv.h"

#define MOCK_SLAVE_COUNT 2
#define MOCK_PI_CAPACITY 256
#define MOCK_WKC         3

struct mock_backend_ctx {
    uint8_t  *memory;
    size_t    size;
    uint32_t  generation;
    uint32_t  cycle_count;
    int       active;
};

static struct mock_backend_ctx *mock_ctx(struct mo_ecat_backend *backend)
{
    return (struct mock_backend_ctx *)backend->ctx;
}

static int mock_backend_open(struct mo_ecat_backend *backend,
                             const struct mo_ecat_config *config)
{
    (void)backend;
    if (!config || !config->interface_name) {
        return -1;
    }
    printf("Mock backend: open on '%s'\n", config->interface_name);
    return 0;
}

/* 在 process_image 内按 slave 顺序分配 I/O 区域 */
static void mock_fill_pdo_refs(struct mo_ecat_pdo_ref *refs,
                               size_t pdo_ref_count,
                               const struct mo_ecat_config *config)
{
    size_t idx = 0;
    uint32_t base[MOCK_SLAVE_COUNT] = {0, 32}; /* 每个 slave 偏移 32 字节 */

    for (size_t s = 0; s < config->slave_count; ++s) {
        uint32_t bit = base[s] * 8;
        for (size_t j = 0; j < config->slaves[s].pdo_entry_count; ++j) {
            if (idx >= pdo_ref_count) {
                return;
            }
            struct mo_ecat_pdo_ref *ref = &refs[idx++];
            ref->byte_offset = bit / 8;
            ref->bit_offset  = (uint8_t)(bit % 8);
            bit += ref->bit_length;
        }
    }
}

static void mock_fill_slave_info(struct mo_ecat_slave *slaves,
                                 size_t slave_count)
{
    for (size_t i = 0; i < slave_count; ++i) {
        struct mo_ecat_slave *slave = &slaves[i];
        snprintf(slave->name, MO_ECAT_MAX_NAME_LEN, "mock_slave_%zu", i);
        slave->position = (uint16_t)i;
        slave->alias    = 0;
        slave->vendor_id    = 0x0000DEAD + (uint32_t)i;
        slave->product_code = 0x0000BEEF + (uint32_t)i;
        slave->revision_number = 1;
        slave->has_dc = 0;
        slave->propagation_delay_ns = 0;
        slave->state.al_state = MO_ECAT_AL_STATE_OP;
        slave->state.online = 1;
        slave->state.operational = 1;
    }
}

static int mock_backend_configure(struct mo_ecat_backend *backend,
                                  const struct mo_ecat_config *config,
                                  struct mo_ecat_process_image *image,
                                  struct mo_ecat_pdo_ref *pdo_refs,
                                  size_t pdo_ref_count,
                                  struct mo_ecat_slave *slaves,
                                  size_t slave_count)
{
    struct mock_backend_ctx *ctx = mock_ctx(backend);
    if (!ctx || !config || !image || !slaves) {
        return -1;
    }

    if (slave_count != MOCK_SLAVE_COUNT) {
        fprintf(stderr, "Mock backend: slave_count must be %d\n", MOCK_SLAVE_COUNT);
        return -1;
    }

    ctx->memory = (uint8_t *)calloc(1, MOCK_PI_CAPACITY);
    if (!ctx->memory) {
        return -1;
    }

    ctx->size       = MOCK_PI_CAPACITY;
    ctx->generation++;
    ctx->active     = 0;

    image->memory   = ctx->memory;
    image->size     = ctx->size;
    image->generation = ctx->generation;
    image->active   = 0;

    mock_fill_slave_info(slaves, slave_count);

    if (pdo_refs && pdo_ref_count > 0) {
        mock_fill_pdo_refs(pdo_refs, pdo_ref_count, config);
        for (size_t i = 0; i < pdo_ref_count; ++i) {
            pdo_refs[i].generation = image->generation;
        }
    }

    printf("Mock backend: configured %zu slaves, PI %zu bytes, WKC %d\n",
           slave_count, ctx->size, MOCK_WKC);
    return 0;
}

static int mock_backend_activate(struct mo_ecat_backend *backend)
{
    struct mock_backend_ctx *ctx = mock_ctx(backend);
    if (!ctx) {
        return -1;
    }
    ctx->active = 1;
    printf("Mock backend: activated\n");
    return 0;
}

static int mock_backend_cycle_begin(struct mo_ecat_backend *backend,
                                    struct mo_ecat_cycle_result *result)
{
    struct mock_backend_ctx *ctx = mock_ctx(backend);
    if (!ctx) {
        return -1;
    }

    ctx->cycle_count++;

    result->link_up       = 1;
    result->expected_wkc  = MOCK_WKC;
    result->actual_wkc    = MOCK_WKC;
    result->dc_time_ns    = (int64_t)ctx->cycle_count * 1000000LL;
    result->dc_time_valid = 1;
    result->backend_error = 0;

    return 0;
}

static int mock_backend_cycle_end(struct mo_ecat_backend *backend,
                                  struct mo_ecat_cycle_result *result)
{
    (void)backend;
    (void)result;
    return 0;
}

static int mock_backend_read_diagnostics(struct mo_ecat_backend *backend,
                                         struct mo_ecat_slave_state *states,
                                         size_t state_count)
{
    (void)backend;
    for (size_t i = 0; i < state_count; ++i) {
        states[i].al_state = MO_ECAT_AL_STATE_OP;
        states[i].error = 0;
        states[i].al_status_code = 0;
        states[i].online = 1;
        states[i].operational = 1;
    }
    return 0;
}

static int mock_backend_deactivate(struct mo_ecat_backend *backend)
{
    struct mock_backend_ctx *ctx = mock_ctx(backend);
    if (!ctx) {
        return -1;
    }
    ctx->active = 0;
    return 0;
}

static void mock_backend_close(struct mo_ecat_backend *backend)
{
    struct mock_backend_ctx *ctx = mock_ctx(backend);
    if (!ctx) {
        return;
    }
    free(ctx->memory);
    ctx->memory = NULL;
    free(ctx);
    backend->ctx = NULL;
}

static const struct mo_ecat_backend_ops mock_ops = {
    .name = "mock",
    .open = mock_backend_open,
    .configure = mock_backend_configure,
    .activate = mock_backend_activate,
    .cycle_begin = mock_backend_cycle_begin,
    .cycle_end = mock_backend_cycle_end,
    .read_diagnostics = mock_backend_read_diagnostics,
    .deactivate = mock_backend_deactivate,
    .close = mock_backend_close,
};

int mo_ecat_mock_backend_init(struct mo_ecat_backend *backend)
{
    if (!backend) {
        return -1;
    }

    struct mock_backend_ctx *ctx =
        (struct mock_backend_ctx *)calloc(1, sizeof(struct mock_backend_ctx));
    if (!ctx) {
        return -1;
    }

    backend->ops = &mock_ops;
    backend->discovery_ops = NULL;
    backend->manual_state_ops = NULL;
    backend->caps.online_discovery = 0;
    backend->caps.dynamic_pdo_mapping = 1;
    backend->caps.dc_support = 0;
    backend->caps.redundancy = 0;
    backend->caps.manual_state_control = 0;
    backend->ctx = ctx;

    return 0;
}
