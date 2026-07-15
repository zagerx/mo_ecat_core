/*
 * soem_mapping.c - SOEM PDO 描述读取、DC 配置与 PDO 映射
 *
 * 通过 SDO 读取从站 PDO 分配与映射对象，配置 SOEM 分布式时钟，并建立
 * IOmap 以获取 PDO entry 在过程数据区域中的偏移。
 */

#include <stdlib.h>

#include "soem_backend.h"
#include "topology_priv.h"

/**
 * soem_check_dc_support - 检查所有从站是否支持 DC
 * @context: SOEM 上下文指针
 *
 * Return: 全部支持返回 0，任一不支持返回 -1
 */
static enum backend_error soem_check_dc_support(ecx_contextt *context)
{
	if (!context) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	for (int i = 1; i <= context->slavecount; ++i) {
		if (!context->slavelist[i].hasdc) {
			return BACKEND_ERROR_DC_UNSUPPORTED;
		}
	}
	return BACKEND_ERROR_NONE;
}

/**
 * soem_read_pdo_assignment - 读取单个 PDO 分配对象下的所有 entry
 * @context: SOEM 上下文指针
 * @slave_number: 从站编号（SOEM 内部编号，从 1 开始）
 * @assignment_index: PDO 分配对象索引（如 0x1C12/0x1C13）
 * @direction: PDO 方向
 * @slave: 核心层从站缓存
 *
 * 读取 PDO 分配对象，再读取每个 PDO 的映射对象，将解析结果填充到
 * slave->pdo_entries[]。
 *
 * Return: 0 成功，非 0 失败
 */
static enum backend_error soem_read_pdo_assignment(
	ecx_contextt *context, uint16_t slave_number, uint16_t assignment_index,
	enum mo_ecat_cyclic_direction direction, struct master_slave *slave)
{
	uint8_t pdo_count = 0;
	int size = sizeof(pdo_count);
	int wkc;

	wkc = ecx_SDOread(context, slave_number, assignment_index, 0, FALSE, &size, &pdo_count,
			  EC_TIMEOUTRXM);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}

	for (uint8_t pdo_subindex = 1; pdo_subindex <= pdo_count; ++pdo_subindex) {
		uint16_t pdo_index = 0;
		uint8_t entry_count = 0;

		size = sizeof(pdo_index);
		wkc = ecx_SDOread(context, slave_number, assignment_index, pdo_subindex, FALSE,
				  &size, &pdo_index, EC_TIMEOUTRXM);
		if (wkc <= 0) {
			return BACKEND_ERROR_SDO_READ_FAILED;
		}
		pdo_index = etohs(pdo_index);
		if (pdo_index == 0) {
			continue;
		}

		size = sizeof(entry_count);
		wkc = ecx_SDOread(context, slave_number, pdo_index, 0, FALSE, &size,
				  &entry_count, EC_TIMEOUTRXM);
		if (wkc <= 0) {
			return BACKEND_ERROR_SDO_READ_FAILED;
		}

		for (uint8_t entry_subindex = 1; entry_subindex <= entry_count; ++entry_subindex) {
			uint32_t mapping = 0;
			struct master_slave_pdo_entry *entry;

			if (slave->pdo_entry_count >= MASTER_MAX_PDO_ENTRIES) {
				return BACKEND_ERROR_PDO_ENTRY_LIMIT_EXCEEDED;
			}
			size = sizeof(mapping);
			wkc = ecx_SDOread(context, slave_number, pdo_index, entry_subindex, FALSE,
					  &size, &mapping, EC_TIMEOUTRXM);
			if (wkc <= 0) {
				return BACKEND_ERROR_SDO_READ_FAILED;
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

	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_read_pdo_entries - 读取 SOEM 从站默认 PDO 映射条目
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_read_pdo_entries(struct backend_instance *backend,
						  struct master_slave *slaves, size_t slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	enum backend_error error;

	if (!context || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		struct master_slave *slave = &slaves[i];

		slave->pdo_entry_count = 0;
		if (!slave->base_info.has_coe) {
			continue;
		}
		if ((ecx_statecheck(&context->context, (uint16_t)(i + 1), EC_STATE_PRE_OP,
				    EC_TIMEOUTSTATE) & 0x0f) != EC_STATE_PRE_OP) {
			return BACKEND_ERROR_READ_NODE_STATE_FAILED;
		}
		error = soem_read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c12,
						 MO_ECAT_CYCLIC_OUTPUT, slave);
		if (error != BACKEND_ERROR_NONE) {
			return error;
		}
		error = soem_read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c13,
						 MO_ECAT_CYCLIC_INPUT, slave);
		if (error != BACKEND_ERROR_NONE) {
			return error;
		}
	}

	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_configure_dc - 配置 SOEM 后端分布式时钟
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_configure_dc(struct backend_instance *backend)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	enum backend_error error;

	if (!context || !context->opened) {
		return BACKEND_ERROR_NOT_READY;
	}
	error = soem_check_dc_support(&context->context);
	if (error != BACKEND_ERROR_NONE) {
		return error;
	}
	if (!ecx_configdc(&context->context)) {
		return BACKEND_ERROR_DC_CONFIG_FAILED;
	}

	context->dc_configured = 1;
	return BACKEND_ERROR_NONE;
}

/**
 * soem_resolve_pdo_entry_offsets - 解析所有 PDO entry 在 IOmap 中的偏移
 * @context: SOEM 后端上下文指针
 * @entries: PDO entry 映射数组
 * @entry_count: PDO entry 数量
 *
 * 根据 SOEM slave 的输入/输出指针和位宽，计算每个 entry 的 byte_offset
 * 与 bit_offset。
 *
 * Return: 0 成功，非 0 失败
 */
static enum backend_error soem_resolve_pdo_entry_offsets(
	struct soem_backend_context *context, struct master_pdo_entry_mapping *entries,
	size_t entry_count)
{
	uint32_t *used_output_bits = NULL;
	uint32_t *used_input_bits = NULL;
	enum backend_error result = BACKEND_ERROR_PDO_OFFSET_RESOLVE_FAILED;
	int slave_count;

	if (!context || (entry_count > 0 && !entries)) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	slave_count = context->context.slavecount;
	if (slave_count < 0) {
		return BACKEND_ERROR_PDO_OFFSET_RESOLVE_FAILED;
	}
	if (slave_count > 0) {
		used_output_bits = calloc((size_t)slave_count, sizeof(*used_output_bits));
		used_input_bits = calloc((size_t)slave_count, sizeof(*used_input_bits));
		if (!used_output_bits || !used_input_bits) {
			result = BACKEND_ERROR_NO_MEMORY;
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

	result = BACKEND_ERROR_NONE;
cleanup:
	free(used_output_bits);
	free(used_input_bits);
	return result;
}

/**
 * soem_backend_build_pdo_mapping - 建立 SOEM 后端 PDO 映射
 * @backend: 后端实例指针
 * @entries: PDO entry 映射数组
 * @entry_count: PDO entry 数量
 *
 * 调用 SOEM ecx_config_map_group() 建立 IOmap，并回填每个 entry 的偏移。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_build_pdo_mapping(struct backend_instance *backend,
						   struct master_pdo_entry_mapping *entries, size_t entry_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	enum backend_error error;
	int mapped_size;

	if (!context || !context->opened || !context->dc_configured) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (entry_count > 0 && !entries) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	context->pdo_mapping_ready = 0;
	context->pdo_image_size = 0;
	mapped_size = ecx_config_map_group(&context->context, context->iomap, 0);
	if (mapped_size <= 0 || (size_t)mapped_size > SOEM_BACKEND_IOMAP_SIZE) {
		return mapped_size > (int)SOEM_BACKEND_IOMAP_SIZE ?
			BACKEND_ERROR_PDO_IMAGE_TOO_LARGE : BACKEND_ERROR_PDO_MAPPING_FAILED;
	}

	context->pdo_image_size = (size_t)mapped_size;
	context->expected_wkc = (uint32_t)context->context.grouplist[0].outputsWKC * 2U +
				context->context.grouplist[0].inputsWKC;
	error = soem_resolve_pdo_entry_offsets(context, entries, entry_count);
	if (error != BACKEND_ERROR_NONE) {
		return error;
	}
	context->pdo_mapping_ready = 1;

	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_get_pdo_image - 获取 SOEM 后端 PDO 数据区域
 * @backend: 后端实例指针
 * @image: 用于返回 PDO 数据映像的指针
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_get_pdo_image(struct backend_instance *backend,
					      struct master_pdo_image *image)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);

	if (!image) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (!context || !context->pdo_mapping_ready) {
		return BACKEND_ERROR_NOT_READY;
	}
	image->memory = context->iomap;
	image->size = context->pdo_image_size;
	return BACKEND_ERROR_NONE;
}
