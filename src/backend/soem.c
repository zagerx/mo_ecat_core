/**
 * @file soem_backend.c
 * @brief SOEM 后端适配器实现（简化版）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soem/soem.h"
#include "backend.h"
#include "backend_ops.h"

#define MO_ECAT_SOEM_IOMAP_SIZE 2048

struct soem_backend_ctx {
	ecx_contextt context;
	uint8_t iomap[MO_ECAT_SOEM_IOMAP_SIZE];
	size_t mapped_size;
	uint32_t expected_wkc;
	int opened;
	int configured;
};

static struct soem_backend_ctx s_soem_ctx;

static struct soem_backend_ctx *soem_ctx(struct backend_instance *backend)
{
	return (struct soem_backend_ctx *)backend->ctx;
}

static enum mo_ecat_slave_al_state soem_to_al_state(uint16_t soem_state)
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

static int soem_backend_open(struct backend_instance *backend,
			     const struct mo_ecat_master_config *config)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || !config || config->interface_name[0] == '\0') {
		return -1;
	}

	if (!ecx_init(&ctx->context, config->interface_name)) {
		fprintf(stderr, "SOEM backend: ecx_init failed on %s\n", config->interface_name);
		return -1;
	}

	ctx->opened = 1;
	return 0;
}

static int soem_backend_load_slave_info(struct backend_instance *backend, size_t *slave_count)
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

int backend_get_slave_count(struct backend_instance *backend, size_t *slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || !slave_count || !ctx->opened) {
		return -1;
	}

	*slave_count = (size_t)ctx->context.slavecount;
	return 0;
}

static int soem_check_dc_support(ecx_contextt *context)
{
	if (!context) {
		return -1;
	}

	for (int i = 1; i <= context->slavecount; ++i) {
		if (!context->slavelist[i].hasdc) {
			fprintf(stderr, "SOEM backend: slave %d does not support DC\n", i);
			return -1;
		}
	}

	return 0;
}

static int soem_backend_translate_slave_info(struct backend_instance *backend,
					     struct mo_ecat_slave *slaves, size_t slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	ecx_contextt *context;

	if (!ctx || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	context = &ctx->context;
	for (size_t i = 0; i < slave_count; ++i) {
		struct mo_ecat_slave *slave = &slaves[i];
		const ec_slavet *soem_slave = &context->slavelist[i + 1];

		slave->base_info.position = soem_slave->configadr - EC_NODEOFFSET;
		slave->base_info.alias = soem_slave->aliasadr;
		slave->base_info.vendor_id = soem_slave->eep_man;
		slave->base_info.product_code = soem_slave->eep_id;
		slave->base_info.revision_number = soem_slave->eep_rev;
		slave->base_info.has_dc = soem_slave->hasdc ? 1 : 0;
		slave->base_info.propagation_delay_ns = soem_slave->pdelay;

		strncpy(slave->base_info.name, (const char *)soem_slave->name,
			MO_ECAT_MAX_NAME_LEN);
		slave->base_info.name[MO_ECAT_MAX_NAME_LEN] = '\0';

		/* 邮箱参数与协议能力 */
		slave->base_info.mailbox.protocol = soem_slave->mbx_proto;
		slave->base_info.mailbox.write_address = soem_slave->mbx_wo;
		slave->base_info.mailbox.write_size = soem_slave->mbx_l;
		slave->base_info.mailbox.read_address = soem_slave->mbx_ro;
		slave->base_info.mailbox.read_size = soem_slave->mbx_rl;

		slave->base_info.has_coe = (soem_slave->mbx_proto & ECT_MBXPROT_COE) ? 1 : 0;
		slave->base_info.has_foe = (soem_slave->mbx_proto & ECT_MBXPROT_FOE) ? 1 : 0;
		slave->base_info.has_eoe = (soem_slave->mbx_proto & ECT_MBXPROT_EOE) ? 1 : 0;
		slave->base_info.has_soe = (soem_slave->mbx_proto & ECT_MBXPROT_SOE) ? 1 : 0;

		/* Sync Manager 配置 */
		for (int sm_idx = 0; sm_idx < EC_MAXSM; ++sm_idx) {
			slave->base_info.sm[sm_idx].start_address =
				soem_slave->SM[sm_idx].StartAddr;
			slave->base_info.sm[sm_idx].length = soem_slave->SM[sm_idx].SMlength;
			slave->base_info.sm[sm_idx].flags = soem_slave->SM[sm_idx].SMflags;
			slave->base_info.sm[sm_idx].type = soem_slave->SMtype[sm_idx];
		}

		/* FMMU 功能分配 */
		slave->base_info.fmmu[0].function = soem_slave->FMMU0func;
		slave->base_info.fmmu[1].function = soem_slave->FMMU1func;
		slave->base_info.fmmu[2].function = soem_slave->FMMU2func;
		slave->base_info.fmmu[3].function = soem_slave->FMMU3func;
	}

	return 0;
}

static int soem_read_pdo_assignment(ecx_contextt *context, uint16_t slave_number,
				    uint16_t assignment_index, enum mo_ecat_pdo_direction direction,
				    struct mo_ecat_slave *slave)
{
	uint8_t pdo_count = 0;
	int size = sizeof(pdo_count);
	int wkc;

	wkc = ecx_SDOread(context, slave_number, assignment_index, 0, FALSE, &size, &pdo_count,
			  EC_TIMEOUTRXM);
	if (wkc <= 0) {
		return -1;
	}

	for (uint8_t pdo_subindex = 1; pdo_subindex <= pdo_count; ++pdo_subindex) {
		uint16_t pdo_index = 0;
		uint8_t entry_count = 0;

		size = sizeof(pdo_index);
		wkc = ecx_SDOread(context, slave_number, assignment_index, pdo_subindex, FALSE,
				  &size, &pdo_index, EC_TIMEOUTRXM);
		if (wkc <= 0) {
			return -1;
		}
		pdo_index = etohs(pdo_index);
		if (pdo_index == 0) {
			continue;
		}

		size = sizeof(entry_count);
		wkc = ecx_SDOread(context, slave_number, pdo_index, 0, FALSE, &size, &entry_count,
				  EC_TIMEOUTRXM);
		if (wkc <= 0) {
			return -1;
		}

		for (uint8_t entry_subindex = 1; entry_subindex <= entry_count; ++entry_subindex) {
			uint32_t mapping = 0;
			struct mo_ecat_slave_pdo_entry *entry;

			if (slave->pdo_entry_count >= MO_ECAT_MAX_PDO_ENTRIES) {
				return -1;
			}

			size = sizeof(mapping);
			wkc = ecx_SDOread(context, slave_number, pdo_index, entry_subindex, FALSE,
					  &size, &mapping, EC_TIMEOUTRXM);
			if (wkc <= 0) {
				return -1;
			}

			mapping = etohl(mapping);
			entry = &slave->pdo_entries[slave->pdo_entry_count++];
			entry->pdo_index = pdo_index;
			entry->object_index = (uint16_t)(mapping >> 16);
			entry->object_subindex = (uint8_t)(mapping >> 8);
			entry->bit_length = (uint8_t)mapping;
			entry->direction = direction;
		}
	}

	return 0;
}

static int soem_backend_read_pdo_entries(struct backend_instance *backend,
					 struct mo_ecat_slave *slaves, size_t slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		struct mo_ecat_slave *slave = &slaves[i];

		slave->pdo_entry_count = 0;
		/* 非 CoE 从站没有标准 SDO PDO 分配对象，保留为空即可。 */
		if (!slave->base_info.has_coe) {
			continue;
		}
		/* ecx_config_init() 已请求 PRE_OP；确认完成迁移后再进行 SDO 读取。 */
		if ((ecx_statecheck(&ctx->context, (uint16_t)(i + 1), EC_STATE_PRE_OP,
				    EC_TIMEOUTSTATE) &
		     0x0f) != EC_STATE_PRE_OP) {
			return -1;
		}

		if (soem_read_pdo_assignment(&ctx->context, (uint16_t)(i + 1), 0x1c12,
					     MO_ECAT_PDO_OUTPUT, slave) < 0 ||
		    soem_read_pdo_assignment(&ctx->context, (uint16_t)(i + 1), 0x1c13,
					     MO_ECAT_PDO_INPUT, slave) < 0) {
			return -1;
		}
	}

	return 0;
}

static int soem_backend_configure(struct backend_instance *backend)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	int mapped_size;

	if (!ctx || !ctx->opened) {
		return -1;
	}

	/* 所有从站必须支持 DC。 */
	if (soem_check_dc_support(&ctx->context) < 0) {
		return -1;
	}

	/* 配置 DC 时钟分布。 */
	if (!ecx_configdc(&ctx->context)) {
		fprintf(stderr, "SOEM backend: ecx_configdc failed\n");
		return -1;
	}

	/* 执行 PDO 映射。 */
	mapped_size = ecx_config_map_group(&ctx->context, ctx->iomap, 0);
	if (mapped_size <= 0 || (size_t)mapped_size > MO_ECAT_SOEM_IOMAP_SIZE) {
		fprintf(stderr, "SOEM backend: map failed or exceeds fixed IOmap size\n");
		return -1;
	}

	ctx->mapped_size = (size_t)mapped_size;

	/* 计算期望 WKC */
	ctx->expected_wkc = (uint32_t)ctx->context.grouplist[0].outputsWKC * 2U +
			    ctx->context.grouplist[0].inputsWKC;

	ctx->configured = 1;

	printf("SOEM backend: %d slaves, IOmap %d bytes, expected WKC %u\n",
	       ctx->context.slavecount, mapped_size, ctx->expected_wkc);

	return 0;
}

static int soem_backend_get_process_image(struct backend_instance *backend,
					  struct mo_ecat_process_image *image)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);

	if (!ctx || !ctx->configured || !image) {
		return -1;
	}

	image->memory = ctx->iomap;
	image->size = ctx->mapped_size;
	image->active = 0;
	return 0;
}

static int soem_backend_fill_pdo_refs(struct backend_instance *backend,
				      struct mo_ecat_slave_pdo_ref *refs, size_t ref_count,
				      uint32_t generation)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	int slavecount;
	uint32_t *used_out_bits = NULL;
	uint32_t *used_in_bits = NULL;
	int result = -1;

	if (!ctx || !ctx->configured || (ref_count > 0 && !refs)) {
		return -1;
	}

	slavecount = ctx->context.slavecount;
	if (slavecount < 0) {
		return -1;
	}

	if (slavecount > 0) {
		used_out_bits = calloc((size_t)slavecount, sizeof(*used_out_bits));
		used_in_bits = calloc((size_t)slavecount, sizeof(*used_in_bits));
		if (!used_out_bits || !used_in_bits) {
			goto cleanup;
		}
	}

	for (size_t i = 0; i < ref_count; ++i) {
		struct mo_ecat_slave_pdo_ref *ref = &refs[i];
		const ec_slavet *soem_slave;
		uint32_t *used_bits;
		uint32_t available_bits;
		const uint8_t *base;
		size_t start_bit;
		size_t end_bit;

		if (ref->slave_index >= (size_t)slavecount) {
			goto cleanup;
		}
		soem_slave = &ctx->context.slavelist[ref->slave_index + 1];

		if (ref->direction == MO_ECAT_PDO_OUTPUT) {
			used_bits = &used_out_bits[ref->slave_index];
			available_bits = soem_slave->Obits;
			base = soem_slave->outputs;
		} else {
			used_bits = &used_in_bits[ref->slave_index];
			available_bits = soem_slave->Ibits;
			base = soem_slave->inputs;
		}

		if (!base || (*used_bits + ref->bit_length) > available_bits) {
			goto cleanup;
		}

		ref->generation = generation;
		ref->byte_offset = (uint32_t)(base - ctx->iomap) + (*used_bits / 8);
		ref->bit_offset = (uint8_t)(*used_bits % 8);

		start_bit = (size_t)ref->byte_offset * 8U + ref->bit_offset;
		end_bit = start_bit + ref->bit_length;
		if (end_bit < start_bit || end_bit > ctx->mapped_size * 8U) {
			goto cleanup;
		}

		*used_bits += ref->bit_length;
	}

	result = 0;

cleanup:
	free(used_out_bits);
	free(used_in_bits);
	return result;
}

static int soem_backend_activate(struct backend_instance *backend)
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

static int soem_backend_cycle_begin(struct backend_instance *backend,
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
		result->diagnostics_required = 1;
		return -1;
	}

	if (result->expected_wkc > 0 && result->actual_wkc != result->expected_wkc) {
		result->diagnostics_required = 1;
	}

	return 0;
}

static int soem_backend_cycle_end(struct backend_instance *backend,
				  struct mo_ecat_cycle_result *result)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	int rc = ecx_send_processdata(&ctx->context);
	if (rc <= 0) {
		result->diagnostics_required = 1;
		return -1;
	}

	return 0;
}

static int soem_backend_read_slave_states(struct backend_instance *backend,
					  struct mo_ecat_slave *slaves, size_t slave_count)
{
	struct soem_backend_ctx *ctx = soem_ctx(backend);
	if (!ctx) {
		return -1;
	}

	ecx_readstate(&ctx->context);

	if (slave_count != (size_t)ctx->context.slavecount) {
		return -1;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		const ec_slavet *soem_slave = &ctx->context.slavelist[i + 1];
		slaves[i].state.al_state = soem_to_al_state(soem_slave->state);
		slaves[i].state.error = (soem_slave->ALstatuscode != 0) ? 1 : 0;
		slaves[i].state.al_status_code = soem_slave->ALstatuscode;
		slaves[i].state.online = 1;
		slaves[i].state.operational =
			(slaves[i].state.al_state == MO_ECAT_AL_STATE_OP) ? 1 : 0;
	}

	return 0;
}

static int soem_backend_deactivate(struct backend_instance *backend)
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

static void soem_backend_close(struct backend_instance *backend)
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

static const struct backend_translation_ops soem_translation_ops = {
	.read_pdo_entries = soem_backend_read_pdo_entries,
	.translate_slave_info = soem_backend_translate_slave_info,
	.fill_pdo_refs = soem_backend_fill_pdo_refs,
	.get_process_image = soem_backend_get_process_image,
};

static const struct backend_ops soem_ops = {
	.open = soem_backend_open,
	.load_slave_info = soem_backend_load_slave_info,
	.configure = soem_backend_configure,
	.activate = soem_backend_activate,
	.cycle_begin = soem_backend_cycle_begin,
	.cycle_end = soem_backend_cycle_end,
	.read_slave_states = soem_backend_read_slave_states,
	.deactivate = soem_backend_deactivate,
	.close = soem_backend_close,
};

int backend_init(struct backend_instance *backend)
{
	if (!backend) {
		return -1;
	}

	struct soem_backend_ctx *ctx = &s_soem_ctx;
	memset(ctx, 0, sizeof(*ctx));

	backend->name = "soem";
	backend->ops = &soem_ops;
	backend->translation_ops = &soem_translation_ops;
	backend->ctx = ctx;

	return 0;
}
