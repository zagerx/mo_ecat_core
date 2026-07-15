/**
 * @file soem_discovery.c
 * @brief SOEM 网口打开、扫描与节点信息转换
 */

#include <stdio.h>
#include <string.h>

#include "soem_backend.h"
#include "topology_priv.h"

struct soem_backend_context *soem_backend_context_get(struct backend_instance *backend)
{
	return backend ? (struct soem_backend_context *)backend->ctx : NULL;
}

enum mo_ecat_node_al_state soem_backend_node_al_state(uint16_t soem_state)
{
	switch (soem_state & 0x0F) {
	case EC_STATE_INIT: return MO_ECAT_NODE_AL_STATE_INIT;
	case EC_STATE_PRE_OP: return MO_ECAT_NODE_AL_STATE_PRE_OP;
	case EC_STATE_SAFE_OP: return MO_ECAT_NODE_AL_STATE_SAFE_OP;
	case EC_STATE_OPERATIONAL: return MO_ECAT_NODE_AL_STATE_OP;
	case EC_STATE_BOOT: return MO_ECAT_NODE_AL_STATE_BOOTSTRAP;
	default: return MO_ECAT_NODE_AL_STATE_UNKNOWN;
	}
}

int soem_backend_open(struct backend_instance *backend,
		      const struct mo_ecat_master_config *config)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !config || config->interface_name[0] == '\0') {
		return -1;
	}
	if (!ecx_init(&context->context, config->interface_name)) {
		fprintf(stderr, "SOEM backend: ecx_init failed on %s\n", config->interface_name);
		return -1;
	}

	context->opened = 1;
	return 0;
}

int soem_backend_load_slave_info(struct backend_instance *backend, size_t *slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	int count;

	if (!context || !slave_count || !context->opened) {
		return -1;
	}
	count = ecx_config_init(&context->context);
	if (count <= 0) {
		fprintf(stderr, "SOEM backend: ecx_config_init failed\n");
		return -1;
	}

	*slave_count = (size_t)count;
	return 0;
}

int backend_get_slave_count(struct backend_instance *backend, size_t *slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !slave_count || !context->opened) {
		return -1;
	}
	*slave_count = (size_t)context->context.slavecount;
	return 0;
}

int soem_backend_translate_slave_info(struct backend_instance *backend,
				     struct master_slave *slaves, size_t slave_count)
{
	struct soem_backend_context *backend_context = soem_backend_context_get(backend);
	ecx_contextt *context;

	if (!backend_context || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)backend_context->context.slavecount) {
		return -1;
	}

	context = &backend_context->context;
	for (size_t i = 0; i < slave_count; ++i) {
		struct master_slave *slave = &slaves[i];
		const ec_slavet *soem_slave = &context->slavelist[i + 1];

		slave->base_info.position = soem_slave->configadr - EC_NODEOFFSET;
		slave->base_info.alias = soem_slave->aliasadr;
		slave->base_info.vendor_id = soem_slave->eep_man;
		slave->base_info.product_code = soem_slave->eep_id;
		slave->base_info.revision_number = soem_slave->eep_rev;
		slave->base_info.dc_supported = soem_slave->hasdc ? 1 : 0;
		slave->base_info.propagation_delay_ns = soem_slave->pdelay;
		strncpy(slave->base_info.name, (const char *)soem_slave->name,
			MO_ECAT_MAX_NAME_LEN);
		slave->base_info.name[MO_ECAT_MAX_NAME_LEN] = '\0';

		slave->base_info.mailbox.protocol = soem_slave->mbx_proto;
		slave->base_info.mailbox.write_address = soem_slave->mbx_wo;
		slave->base_info.mailbox.write_size = soem_slave->mbx_l;
		slave->base_info.mailbox.read_address = soem_slave->mbx_ro;
		slave->base_info.mailbox.read_size = soem_slave->mbx_rl;
		slave->base_info.has_coe = (soem_slave->mbx_proto & ECT_MBXPROT_COE) ? 1 : 0;
		slave->base_info.has_foe = (soem_slave->mbx_proto & ECT_MBXPROT_FOE) ? 1 : 0;
		slave->base_info.has_eoe = (soem_slave->mbx_proto & ECT_MBXPROT_EOE) ? 1 : 0;
		slave->base_info.has_soe = (soem_slave->mbx_proto & ECT_MBXPROT_SOE) ? 1 : 0;

		for (int sm_index = 0; sm_index < EC_MAXSM; ++sm_index) {
			slave->base_info.sm[sm_index].start_address = soem_slave->SM[sm_index].StartAddr;
			slave->base_info.sm[sm_index].length = soem_slave->SM[sm_index].SMlength;
			slave->base_info.sm[sm_index].flags = soem_slave->SM[sm_index].SMflags;
			slave->base_info.sm[sm_index].type = soem_slave->SMtype[sm_index];
		}

		slave->base_info.fmmu[0].function = soem_slave->FMMU0func;
		slave->base_info.fmmu[1].function = soem_slave->FMMU1func;
		slave->base_info.fmmu[2].function = soem_slave->FMMU2func;
		slave->base_info.fmmu[3].function = soem_slave->FMMU3func;
	}

	return 0;
}

void soem_backend_close(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context) {
		return;
	}
	if (context->opened) {
		ecx_close(&context->context);
		context->opened = 0;
	}
	backend->ctx = NULL;
}
