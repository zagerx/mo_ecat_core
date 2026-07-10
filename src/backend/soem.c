/**
 * @file soem_backend.c
 * @brief SOEM 后端适配器实现（简化版）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soem/soem.h"
#include "backend.h"

#define MO_ECAT_SOEM_IOMAP_SIZE 2048

struct soem_backend_ctx {
	ecx_contextt context;
	uint8_t iomap[MO_ECAT_SOEM_IOMAP_SIZE];
	uint32_t expected_wkc;
	int opened;
};

static struct soem_backend_ctx s_soem_ctx;

static struct soem_backend_ctx *soem_ctx(struct mo_ecat_backend *backend)
{
	return (struct soem_backend_ctx *)backend->ctx;
}

static enum mo_ecat_al_state soem_to_al_state(uint16_t soem_state)
{
	switch (soem_state & 0x0F) {
	case EC_STATE_INIT:
		return MO_ECAT_AL_STATE_INIT;
	case EC_STATE_PRE_OP:
		return MO_ECAT_AL_STATE_PRE_OP;
	case EC_STATE_SAFE_OP:
		return MO_ECAT_AL_STATE_SAFE_OP;
	case EC_STATE_OPERATIONAL:
		return MO_ECAT_AL_STATE_OP;
	case EC_STATE_BOOT:
		return MO_ECAT_AL_STATE_BOOTSTRAP;
	default:
		return MO_ECAT_AL_STATE_UNKNOWN;
	}
}

static int soem_backend_open(struct mo_ecat_backend *backend,
			     const struct mo_ecat_master_options *options)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || !options || options->interface_name[0] == '\0') {
		return -1;
	}

	if (!ecx_init(&ctx->context, options->interface_name)) {
		fprintf(stderr, "SOEM backend: ecx_init failed on %s\n", options->interface_name);
		return -1;
	}

	ctx->opened = 1;
	return 0;
}

static int soem_backend_scan(struct mo_ecat_backend *backend, size_t *slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	int count;

	if (!ctx || !slave_count || !ctx->opened) {
		return -1;
	}

	count = ecx_config_init(&ctx->context);
	if (count <= 0) {
		fprintf(stderr, "SOEM backend: ecx_config_init failed\n");
		return -1;
	}

	*slave_count = (size_t)count;
	return 0;
}

static int soem_validate_config(const struct mo_ecat_user_config *config, ecx_contextt *context)
{
	if (!config || !context) {
		return -1;
	}

	if (config->slave_count != (size_t)context->slavecount) {
		fprintf(stderr, "SOEM backend: slave count mismatch: config=%zu, bus=%d\n",
			config->slave_count, context->slavecount);
		return -1;
	}

	for (size_t i = 0; i < config->slave_count; ++i) {
		const struct mo_ecat_slave_config *cfg = &config->slaves[i];
		const ec_slavet *soem_slave = &context->slavelist[i + 1];

		if (cfg->vendor_id != 0 && cfg->vendor_id != soem_slave->eep_man) {
			fprintf(stderr, "SOEM backend: vendor_id mismatch at slave %zu\n", i);
			return -1;
		}
		if (cfg->product_code != 0 && cfg->product_code != soem_slave->eep_id) {
			fprintf(stderr, "SOEM backend: product_code mismatch at slave %zu\n", i);
			return -1;
		}
	}

	return 0;
}

static size_t bytes_for_bits(uint32_t bits)
{
	return (bits + 7U) / 8U;
}

static size_t soem_estimate_iomap_size_from_config(const struct mo_ecat_user_config *config)
{
	size_t output_bytes = 0;
	size_t input_bytes = 0;

	for (size_t i = 0; i < config->slave_count; ++i) {
		uint32_t output_bits = 0;
		uint32_t input_bits = 0;

		for (size_t j = 0; j < config->slaves[i].pdo_entry_count; ++j) {
			const struct mo_ecat_pdo_entry_config *entry =
				&config->slaves[i].pdo_entries[j];
			if (entry->direction == MO_ECAT_PDO_OUTPUT) {
				output_bits += entry->bit_length;
			} else {
				input_bits += entry->bit_length;
			}
		}

		output_bytes += bytes_for_bits(output_bits);
		input_bytes += bytes_for_bits(input_bits);
	}

	return output_bytes + input_bytes;
}

/*
 * 根据配置中的 pdo_entries 顺序，在 slave 的输入/输出区域内顺序分配 offset。
 * 这是简化实现：假设配置顺序与实际 PDO 映射顺序一致。
 */
static int soem_fill_pdo_refs(ecx_contextt *context, const struct mo_ecat_user_config *config,
			      struct mo_ecat_pdo_ref *refs, size_t pdo_ref_count, uint8_t *iomap,
			      size_t iomap_size)
{
	size_t idx = 0;

	for (size_t i = 0; i < config->slave_count; ++i) {
		const struct mo_ecat_slave_config *slave_cfg = &config->slaves[i];
		const ec_slavet *soem_slave = &context->slavelist[i + 1];

		uint32_t out_bit = 0;
		uint32_t in_bit = 0;
		uint32_t output_bits = soem_slave->Obits;
		uint32_t input_bits = soem_slave->Ibits;

		for (size_t j = 0; j < slave_cfg->pdo_entry_count; ++j) {
			if (idx >= pdo_ref_count) {
				return -1;
			}

			struct mo_ecat_pdo_ref *ref = &refs[idx++];
			uint32_t *used_bits =
				(ref->direction == MO_ECAT_PDO_OUTPUT) ? &out_bit : &in_bit;
			uint32_t available_bits =
				(ref->direction == MO_ECAT_PDO_OUTPUT) ? output_bits : input_bits;

			if (ref->direction == MO_ECAT_PDO_OUTPUT) {
				if (!soem_slave->outputs ||
				    (*used_bits + ref->bit_length) > available_bits) {
					return -1;
				}
				ref->byte_offset =
					(uint32_t)(soem_slave->outputs - iomap) + (*used_bits / 8);
			} else {
				if (!soem_slave->inputs ||
				    (*used_bits + ref->bit_length) > available_bits) {
					return -1;
				}
				ref->byte_offset =
					(uint32_t)(soem_slave->inputs - iomap) + (*used_bits / 8);
			}

			ref->bit_offset = (uint8_t)(*used_bits % 8);
			*used_bits += ref->bit_length;

			size_t start_bit = (size_t)ref->byte_offset * 8U + ref->bit_offset;
			size_t end_bit = start_bit + ref->bit_length;
			if (end_bit < start_bit || end_bit > iomap_size * 8U) {
				return -1;
			}
		}
	}

	return idx == pdo_ref_count ? 0 : -1;
}

static void soem_set_backend_error_once(struct mo_ecat_cycle_result *result, int error)
{
	if (result && result->backend_error == 0) {
		result->backend_error = error;
	}
}

static void soem_fill_slave_info(struct mo_ecat_slave *slaves, size_t slave_count,
				 ecx_contextt *context)
{
	for (size_t i = 0; i < slave_count; ++i) {
		struct mo_ecat_slave *slave = &slaves[i];
		const ec_slavet *soem_slave = &context->slavelist[i + 1];

		slave->position = soem_slave->configadr - EC_NODEOFFSET;
		slave->alias = soem_slave->aliasadr;
		slave->vendor_id = soem_slave->eep_man;
		slave->product_code = soem_slave->eep_id;
		slave->revision_number = soem_slave->eep_rev;
		slave->has_dc = soem_slave->hasdc ? 1 : 0;
		slave->propagation_delay_ns = soem_slave->pdelay;

		strncpy(slave->name, (const char *)soem_slave->name, MO_ECAT_MAX_NAME_LEN);
		slave->name[MO_ECAT_MAX_NAME_LEN] = '\0';

		slave->state.al_state = soem_to_al_state(soem_slave->state);

		/* 邮箱参数与协议能力 */
		slave->mailbox.protocol = soem_slave->mbx_proto;
		slave->mailbox.write_address = soem_slave->mbx_wo;
		slave->mailbox.write_size = soem_slave->mbx_l;
		slave->mailbox.read_address = soem_slave->mbx_ro;
		slave->mailbox.read_size = soem_slave->mbx_rl;

		slave->has_coe = (soem_slave->mbx_proto & ECT_MBXPROT_COE) ? 1 : 0;
		slave->has_foe = (soem_slave->mbx_proto & ECT_MBXPROT_FOE) ? 1 : 0;
		slave->has_eoe = (soem_slave->mbx_proto & ECT_MBXPROT_EOE) ? 1 : 0;
		slave->has_soe = (soem_slave->mbx_proto & ECT_MBXPROT_SOE) ? 1 : 0;

		/* Sync Manager 配置 */
		for (int sm_idx = 0; sm_idx < EC_MAXSM; ++sm_idx) {
			slave->sm[sm_idx].start_address = soem_slave->SM[sm_idx].StartAddr;
			slave->sm[sm_idx].length = soem_slave->SM[sm_idx].SMlength;
			slave->sm[sm_idx].flags = soem_slave->SM[sm_idx].SMflags;
			slave->sm[sm_idx].type = soem_slave->SMtype[sm_idx];
		}

		/* FMMU 功能分配 */
		slave->fmmu[0].function = soem_slave->FMMU0func;
		slave->fmmu[1].function = soem_slave->FMMU1func;
		slave->fmmu[2].function = soem_slave->FMMU2func;
		slave->fmmu[3].function = soem_slave->FMMU3func;
	}
}

static int soem_backend_read_discovered_slaves(struct mo_ecat_backend *backend,
					       struct mo_ecat_slave *slaves, size_t slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	soem_fill_slave_info(slaves, slave_count, &ctx->context);
	return 0;
}

static int soem_backend_configure(struct mo_ecat_backend *backend,
				  const struct mo_ecat_user_config *config,
				  struct mo_ecat_process_image *image,
				  struct mo_ecat_pdo_ref *pdo_refs, size_t pdo_ref_count,
				  struct mo_ecat_slave *slaves, size_t slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || !config || !image || (slave_count > 0 && !slaves)) {
		return -1;
	}

	size_t estimated_size = soem_estimate_iomap_size_from_config(config);
	if (estimated_size > MO_ECAT_SOEM_IOMAP_SIZE) {
		fprintf(stderr,
			"SOEM backend: configured PDO size estimate %zu exceeds fixed IOmap size "
			"%zu\n",
			estimated_size, (size_t)MO_ECAT_SOEM_IOMAP_SIZE);
		return -1;
	}

	/* 扫描阶段已完成总线发现；此处校验配置与实际总线是否一致。 */
	if (soem_validate_config(config, &ctx->context) < 0) {
		return -1;
	}

	if (slave_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	/* 执行 PDO 映射。 */
	int mapped_size = ecx_config_map_group(&ctx->context, ctx->iomap, 0);
	if (mapped_size <= 0 || (size_t)mapped_size > MO_ECAT_SOEM_IOMAP_SIZE) {
		fprintf(stderr, "SOEM backend: map failed or exceeds fixed IOmap size\n");
		return -1;
	}

	/* 5. 回填过程数据域 */
	image->memory = ctx->iomap;
	image->size = (size_t)mapped_size;
	image->generation++;
	image->active = 0;

	/* 6. 计算期望 WKC */
	ctx->expected_wkc = (uint32_t)ctx->context.grouplist[0].outputsWKC * 2U +
			    ctx->context.grouplist[0].inputsWKC;

	/* 7. 填充从站静态信息 */
	soem_fill_slave_info(slaves, slave_count, &ctx->context);

	/* 8. 填充 PDO 引用 offset */
	if (pdo_refs && pdo_ref_count > 0) {
		if (soem_fill_pdo_refs(&ctx->context, config, pdo_refs, pdo_ref_count, ctx->iomap,
				       image->size) < 0) {
			fprintf(stderr,
				"SOEM backend: configured PDO entries do not match mapped IO\n");
			return -1;
		}
		for (size_t i = 0; i < pdo_ref_count; ++i) {
			pdo_refs[i].generation = image->generation;
		}
	}

	printf("SOEM backend: %d slaves, IOmap %d bytes, expected WKC %u\n",
	       ctx->context.slavecount, mapped_size, ctx->expected_wkc);

	return 0;
}

static int soem_backend_activate(struct mo_ecat_backend *backend)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	/* SOEM 下 activate 可视为完成一次 send，为后续 receive 建立起点 */
	ecx_send_processdata(&ctx->context);

	/* 请求所有从站进入 OP */
	ctx->context.slavelist[0].state = EC_STATE_OPERATIONAL;
	ecx_writestate(&ctx->context, 0);

	/* 简单等待进入 OP（简化版，不阻塞太久） */
	if (ecx_statecheck(&ctx->context, 0, EC_STATE_OPERATIONAL, 50000) != EC_STATE_OPERATIONAL) {
		return -1;
	}

	return 0;
}

static int soem_backend_cycle_begin(struct mo_ecat_backend *backend,
				    struct mo_ecat_cycle_result *result)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	int rc = ecx_receive_processdata(&ctx->context, EC_TIMEOUTRET);
	if (rc > 0) {
		result->link_up = 1;
	} else {
		result->link_up = 0;
	}

	result->actual_wkc = (rc > 0) ? (uint32_t)rc : 0;
	result->expected_wkc = ctx->expected_wkc;
	result->dc_time_ns = ctx->context.DCtime;
	result->dc_time_valid = 1;

	if (rc <= 0) {
		soem_set_backend_error_once(result, rc);
		result->diagnostics_required = 1;
		return -1;
	}

	if (result->expected_wkc > 0 && result->actual_wkc != result->expected_wkc) {
		result->diagnostics_required = 1;
	}

	return 0;
}

static int soem_backend_cycle_end(struct mo_ecat_backend *backend,
				  struct mo_ecat_cycle_result *result)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	int rc = ecx_send_processdata(&ctx->context);
	if (rc <= 0) {
		soem_set_backend_error_once(result, rc);
		result->diagnostics_required = 1;
		return -1;
	}

	return 0;
}

static int soem_backend_read_diagnostics(struct mo_ecat_backend *backend,
					 struct mo_ecat_slave_state *states, size_t state_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	ecx_readstate(&ctx->context);

	if (state_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	for (size_t i = 0; i < state_count; ++i) {
		const ec_slavet *soem_slave = &ctx->context.slavelist[i + 1];
		states[i].al_state = soem_to_al_state(soem_slave->state);
		states[i].error = (soem_slave->ALstatuscode != 0) ? 1 : 0;
		states[i].al_status_code = soem_slave->ALstatuscode;
		states[i].online = 1;
		states[i].operational = (states[i].al_state == MO_ECAT_AL_STATE_OP) ? 1 : 0;
	}

	return 0;
}

static int soem_backend_deactivate(struct mo_ecat_backend *backend)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	/* 请求所有从站回到 SAFE_OP */
	ctx->context.slavelist[0].state = EC_STATE_SAFE_OP;
	ecx_writestate(&ctx->context, 0);
	return 0;
}

static void soem_backend_close(struct mo_ecat_backend *backend)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return;
	}

	if (ctx->opened) {
		ecx_close(&ctx->context);
		ctx->opened = 0;
	}

	backend->ctx = NULL;
}

static const struct mo_ecat_backend_ops soem_ops = {
	.name = "soem",
	.open = soem_backend_open,
	.scan = soem_backend_scan,
	.read_discovered_slaves = soem_backend_read_discovered_slaves,
	.configure = soem_backend_configure,
	.activate = soem_backend_activate,
	.cycle_begin = soem_backend_cycle_begin,
	.cycle_end = soem_backend_cycle_end,
	.read_diagnostics = soem_backend_read_diagnostics,
	.deactivate = soem_backend_deactivate,
	.close = soem_backend_close,
};

int backend_init(struct mo_ecat_backend *backend)
{
	if (!backend) {
		return -1;
	}

	struct soem_backend_ctx *ctx = &s_soem_ctx;
	memset(ctx, 0, sizeof(*ctx));

	backend->ops = &soem_ops;
	backend->discovery_ops = NULL;    /* 简化版不实现在线发现 */
	backend->manual_state_ops = NULL; /* 简化版不实现手动状态控制 */
	backend->caps.online_discovery = 0;
	backend->caps.dynamic_pdo_mapping = 0;
	backend->caps.dc_support = 1;
	backend->caps.redundancy = 0;
	backend->caps.manual_state_control = 0;
	backend->ctx = ctx;

	return 0;
}
