/**
 * @file soem_mapping.c
 * @brief SOEM PDO 描述读取、DC 配置与 PDO 映射
 */

#include <stdio.h>
#include <stdlib.h>

#include "soem_backend.h"
#include "topology_priv.h"

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

static int soem_read_pdo_assignment(ecx_contextt *context, uint16_t slave_number,
				    uint16_t assignment_index, enum mo_ecat_cyclic_direction direction,
				    struct master_slave *slave)
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
		wkc = ecx_SDOread(context, slave_number, pdo_index, 0, FALSE, &size,
				  &entry_count, EC_TIMEOUTRXM);
		if (wkc <= 0) {
			return -1;
		}

		for (uint8_t entry_subindex = 1; entry_subindex <= entry_count; ++entry_subindex) {
			uint32_t mapping = 0;
			struct master_slave_pdo_entry *entry;

			if (slave->pdo_entry_count >= MASTER_MAX_PDO_ENTRIES) {
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

int soem_backend_read_pdo_entries(struct backend_instance *backend,
				  struct master_slave *slaves, size_t slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)context->context.slavecount) {
		return -1;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		struct master_slave *slave = &slaves[i];

		slave->pdo_entry_count = 0;
		if (!slave->base_info.has_coe) {
			continue;
		}
		if ((ecx_statecheck(&context->context, (uint16_t)(i + 1), EC_STATE_PRE_OP,
				    EC_TIMEOUTSTATE) & 0x0f) != EC_STATE_PRE_OP) {
			return -1;
		}
		if (soem_read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c12,
					     MO_ECAT_CYCLIC_OUTPUT, slave) < 0 ||
		    soem_read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c13,
					     MO_ECAT_CYCLIC_INPUT, slave) < 0) {
			return -1;
		}
	}

	return 0;
}

int soem_backend_configure_dc(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !context->opened || soem_check_dc_support(&context->context) < 0) {
		return -1;
	}
	if (!ecx_configdc(&context->context)) {
		fprintf(stderr, "SOEM backend: ecx_configdc failed\n");
		return -1;
	}

	context->dc_configured = 1;
	return 0;
}

static int soem_resolve_pdo_entry_offsets(struct soem_backend_context *context,
					  struct master_pdo_entry_mapping *entries,
					  size_t entry_count)
{
	uint32_t *used_output_bits = NULL;
	uint32_t *used_input_bits = NULL;
	int result = -1;
	int slave_count;

	if (!context || (entry_count > 0 && !entries)) {
		return -1;
	}
	slave_count = context->context.slavecount;
	if (slave_count < 0) {
		return -1;
	}
	if (slave_count > 0) {
		used_output_bits = calloc((size_t)slave_count, sizeof(*used_output_bits));
		used_input_bits = calloc((size_t)slave_count, sizeof(*used_input_bits));
		if (!used_output_bits || !used_input_bits) {
			goto cleanup;
		}
	}

	for (size_t i = 0; i < entry_count; ++i) {
		struct master_pdo_entry_mapping *mapping = &entries[i];
		const ec_slavet *slave;
		uint32_t *used_bits;
		uint32_t available_bits;
		const uint8_t *base;
		size_t start_bit;
		size_t end_bit;

		if (mapping->entry.node_index >= (size_t)slave_count) {
			goto cleanup;
		}
		slave = &context->context.slavelist[mapping->entry.node_index + 1];
		if (mapping->entry.direction == MO_ECAT_CYCLIC_OUTPUT) {
			used_bits = &used_output_bits[mapping->entry.node_index];
			available_bits = slave->Obits;
			base = slave->outputs;
		} else {
			used_bits = &used_input_bits[mapping->entry.node_index];
			available_bits = slave->Ibits;
			base = slave->inputs;
		}
		if (!base || (*used_bits + mapping->entry.bit_length) > available_bits) {
			goto cleanup;
		}

		mapping->byte_offset = (uint32_t)(base - context->iomap) + (*used_bits / 8);
		mapping->bit_offset = (uint8_t)(*used_bits % 8);
		start_bit = (size_t)mapping->byte_offset * 8U + mapping->bit_offset;
		end_bit = start_bit + mapping->entry.bit_length;
		if (end_bit < start_bit || end_bit > context->pdo_image_size * 8U) {
			goto cleanup;
		}
		*used_bits += mapping->entry.bit_length;
	}

	result = 0;
cleanup:
	free(used_output_bits);
	free(used_input_bits);
	return result;
}

int soem_backend_build_pdo_mapping(struct backend_instance *backend,
				   struct master_pdo_entry_mapping *entries, size_t entry_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	int mapped_size;

	if (!context || !context->opened || !context->dc_configured ||
	    (entry_count > 0 && !entries)) {
		return -1;
	}
	context->pdo_mapping_ready = 0;
	context->pdo_image_size = 0;
	mapped_size = ecx_config_map_group(&context->context, context->iomap, 0);
	if (mapped_size <= 0 || (size_t)mapped_size > SOEM_BACKEND_IOMAP_SIZE) {
		fprintf(stderr, "SOEM backend: map failed or exceeds fixed IOmap size\n");
		return -1;
	}

	context->pdo_image_size = (size_t)mapped_size;
	context->expected_wkc = (uint32_t)context->context.grouplist[0].outputsWKC * 2U +
				context->context.grouplist[0].inputsWKC;
	if (soem_resolve_pdo_entry_offsets(context, entries, entry_count) < 0) {
		return -1;
	}
	context->pdo_mapping_ready = 1;

	printf("SOEM backend: %d slaves, IOmap %d bytes, expected WKC %u\n",
	       context->context.slavecount, mapped_size, context->expected_wkc);
	return 0;
}

int soem_backend_get_pdo_image(struct backend_instance *backend,
			       struct master_pdo_image *image)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!context || !context->pdo_mapping_ready || !image) {
		return -1;
	}
	image->memory = context->iomap;
	image->size = context->pdo_image_size;
	return 0;
}
