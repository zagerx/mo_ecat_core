/*
 * soem_mapping.c - SOEM PDO 描述读取、DC 配置与 PDO 映射
 *
 * 通过 SDO 读取从站 PDO 分配与映射对象，配置 SOEM 分布式时钟，并建立
 * IOmap 以获取 PDO entry 在过程数据区域中的偏移。
 */

#include <stdlib.h>
#include <stdio.h>

#include "soem_backend.h"
#include "slave_priv.h"

#define SOEM_SDO_READ_ATTEMPTS 3


/* 静态辅助函数前向声明 */

static int _read_sdo(ecx_contextt *context, uint16_t slave_number, uint16_t object_index,
			 uint8_t object_subindex, int *size, void *data);

static void _log_mapping_failure(ecx_contextt *context, const char *stage, int mapped_size);

static enum backend_error _check_dc_support(ecx_contextt *context);

static enum backend_error _read_pdo_assignment(ecx_contextt *context, uint16_t slave_number,
						   uint16_t assignment_index,
						   enum mo_ecat_pdo_direction direction,
						   struct slave *slave);

static enum backend_error _resolve_pdo_entry_offsets(struct soem_backend_context *context,
							 struct pdo_image_entry *entries,
							 size_t entry_count);

/**
 * soem_backend_read_pdo_entries - 读取 SOEM 从站默认 PDO 映射条目
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_read_pdo_entries(struct backend_instance *backend,
						 struct slave *slaves, size_t slave_count)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	enum backend_error error;

	if (!context || (slave_count > 0 && !slaves) ||
	    slave_count != (size_t)context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	for (size_t i = 0; i < slave_count; ++i) {
		struct slave *slave = &slaves[i];

		slave->pdo_entry_count = 0;
		if (!slave->base_info.has_coe) {
			continue;
		}
		const uint16_t state = ecx_statecheck(&context->context, (uint16_t)(i + 1),
						      EC_STATE_PRE_OP, EC_TIMEOUTSTATE);
		if ((state & 0x0f) != EC_STATE_PRE_OP) {
			fprintf(stderr,
				"[soem] PDO description unavailable: slave=%zu state=0x%02x\n",
				i + 1, state);
			return BACKEND_ERROR_READ_NODE_STATE_FAILED;
		}
		error = _read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c12,
						 MO_ECAT_PDO_OUTPUT, slave);
		if (error != BACKEND_ERROR_NONE) {
			return error;
		}
		error = _read_pdo_assignment(&context->context, (uint16_t)(i + 1), 0x1c13,
						 MO_ECAT_PDO_INPUT, slave);
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
	error = _check_dc_support(&context->context);
	if (error != BACKEND_ERROR_NONE) {
		return error;
	}
	if (!ecx_configdc(&context->context)) {
		return BACKEND_ERROR_DC_CONFIG_FAILED;
	}

	context->dc_configured = 1;

	/* Sync0 激活推迟到 RUNNING 之后，避免干扰 OP 切换。
	   pending 参数保留，由后续 sync0_configure 调用（dc_configured=1 时直接生效）。 */

	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_sync0_configure - 激活/关闭从站 DC Sync0 输出
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（核心层逻辑下标，0 起）
 * @enable: 非 0 激活，0 关闭
 * @cycle_time_ns: Sync0 周期（ns）
 * @shift_time_ns: Sync0 相位偏移（ns）
 *
 * 只在总线配置阶段调用；运行期重复调用会导致 Sync0 重建。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_sync0_configure(struct backend_instance *backend,
						size_t slave_index, int enable,
						uint32_t cycle_time_ns, int32_t shift_time_ns)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);

	if (!context || !context->opened) {
		return BACKEND_ERROR_NOT_READY;
	}
	if (slave_number > context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (enable != 0 && cycle_time_ns == 0U) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	if (!context->dc_configured) {
		/* DC 尚未配置，暂存参数，待 configure_dc 完成后再生效。 */
		context->pending_sync0_enable = enable ? 1 : 0;
		context->pending_sync0_cycle_ns = cycle_time_ns;
		context->pending_sync0_shift_ns = shift_time_ns;
		return BACKEND_ERROR_NONE;
	}

	ecx_dcsync0(&context->context, slave_number, enable != 0, cycle_time_ns, shift_time_ns);

	return BACKEND_ERROR_NONE;
}

/**
 * soem_backend_sync0_read_status - 读回从站 Sync0 状态
 * @backend: 后端实例指针
 * @slave_index: 目标从站下标（核心层逻辑下标，0 起）
 * @status: 状态输出缓冲区
 *
 * 通过 ESC 寄存器读回 Sync0 激活位、周期、起始时间和 DC System Time。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error soem_backend_sync0_read_status(struct backend_instance *backend,
						  size_t slave_index,
						  struct mo_ecat_sync0_status *status)
{
	struct soem_backend_context *context = soem_backend_context_get(backend);
	const uint16_t slave_number = (uint16_t)(slave_index + 1U);
	uint8_t sync_act = 0;
	uint32_t cycle0 = 0;
	uint32_t start0 = 0;
	uint64_t sys_time = 0;
	int wkc;

	if (!context || !context->opened || !status) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	if (slave_number > context->context.slavecount) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	wkc = ecx_FPRD(&context->context.port, context->context.slavelist[slave_number].configadr,
		       ECT_REG_DCSYNCACT, sizeof(sync_act), &sync_act, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}
	status->active = (sync_act & 0x03U) != 0 ? 1 : 0;
	wkc = ecx_FPRD(&context->context.port, context->context.slavelist[slave_number].configadr,
		       ECT_REG_DCCYCLE0, sizeof(cycle0), &cycle0, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}
	wkc = ecx_FPRD(&context->context.port, context->context.slavelist[slave_number].configadr,
		       ECT_REG_DCSTART0, sizeof(start0), &start0, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}
	wkc = ecx_FPRD(&context->context.port, context->context.slavelist[slave_number].configadr,
		       ECT_REG_DCSYSTIME, sizeof(sys_time), &sys_time, EC_TIMEOUTRET);
	if (wkc <= 0) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}

	status->cycle_time_ns = cycle0;
	status->start_time_ns = start0;
	status->dc_time_ns = sys_time;
	return BACKEND_ERROR_NONE;
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
						  struct pdo_image_entry *entries,
						  size_t entry_count)
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
		_log_mapping_failure(&context->context, "config-map", mapped_size);
		return mapped_size > (int)SOEM_BACKEND_IOMAP_SIZE
			       ? BACKEND_ERROR_PDO_IMAGE_TOO_LARGE
			       : BACKEND_ERROR_PDO_MAPPING_FAILED;
	}

	context->pdo_image_size = (size_t)mapped_size;
	context->expected_wkc = (uint32_t)context->context.grouplist[0].outputsWKC * 2U +
				context->context.grouplist[0].inputsWKC;
	error = _resolve_pdo_entry_offsets(context, entries, entry_count);
	if (error != BACKEND_ERROR_NONE) {
		_log_mapping_failure(&context->context, "entry-offset", mapped_size);
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
					      struct pdo_image *image)
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

static int _read_sdo(ecx_contextt *context, uint16_t slave_number, uint16_t object_index,
			 uint8_t object_subindex, int *size, void *data)
{
	const int requested_size = *size;

	for (int attempt = 0; attempt < SOEM_SDO_READ_ATTEMPTS; ++attempt) {
		*size = requested_size;
		if (ecx_SDOread(context, slave_number, object_index, object_subindex, FALSE, size,
				data, EC_TIMEOUTRXM) > 0) {
			return 1;
		}
	}

	fprintf(stderr, "[soem] SDO read failed after %d attempts: slave=%u object=0x%04x:%02x\n",
		SOEM_SDO_READ_ATTEMPTS, slave_number, object_index, object_subindex);
	return 0;
}

static void _log_mapping_failure(ecx_contextt *context, const char *stage, int mapped_size)
{
	(void)ecx_readstate(context);
	fprintf(stderr, "[soem] PDO mapping failed: stage=%s mapped_size=%d\n", stage, mapped_size);
	for (int slave_number = 1; slave_number <= context->slavecount; ++slave_number) {
		const ec_slavet *slave = &context->slavelist[slave_number];

		fprintf(stderr,
			"[soem] slave=%d state=0x%02x al=0x%04x Obits=%u Ibits=%u "
			"SM2=0x%04x/%u/type%u SM3=0x%04x/%u/type%u\n",
			slave_number, slave->state, slave->ALstatuscode, slave->Obits, slave->Ibits,
			etohs(slave->SM[2].StartAddr), etohs(slave->SM[2].SMlength),
			slave->SMtype[2], etohs(slave->SM[3].StartAddr),
			etohs(slave->SM[3].SMlength), slave->SMtype[3]);
	}
}

/**
 * _check_dc_support - 检查所有从站是否支持 DC
 * @context: SOEM 上下文指针
 *
 * Return: 全部支持返回 0，任一不支持返回 -1
 */
static enum backend_error _check_dc_support(ecx_contextt *context)
{
	if (!context) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}
	for (int i = 1; i <= context->slavecount; ++i) {
		if (!context->slavelist[i].hasdc) {
			const ec_slavet *slave;

			(void)ecx_readstate(context);
			slave = &context->slavelist[i];
			fprintf(stderr,
				"[soem] DC check failed: slave=%d state=0x%02x al=0x%04x "
				"SM0=0x%04x/%u/0x%08x SM1=0x%04x/%u/0x%08x "
				"mbx-write=0x%04x/%u mbx-read=0x%04x/%u proto=0x%04x\n",
				i, slave->state, slave->ALstatuscode, etohs(slave->SM[0].StartAddr),
				etohs(slave->SM[0].SMlength), etohl(slave->SM[0].SMflags),
				etohs(slave->SM[1].StartAddr), etohs(slave->SM[1].SMlength),
				etohl(slave->SM[1].SMflags), slave->mbx_wo, slave->mbx_l,
				slave->mbx_ro, slave->mbx_rl, slave->mbx_proto);
			return BACKEND_ERROR_DC_UNSUPPORTED;
		}
	}
	return BACKEND_ERROR_NONE;
}

/**
 * _read_pdo_assignment - 读取单个 PDO 分配对象下的所有 entry
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
static enum backend_error _read_pdo_assignment(ecx_contextt *context, uint16_t slave_number,
						   uint16_t assignment_index,
						   enum mo_ecat_pdo_direction direction,
						   struct slave *slave)
{
	uint8_t pdo_count = 0;
	int size = sizeof(pdo_count);

	if (!_read_sdo(context, slave_number, assignment_index, 0, &size, &pdo_count)) {
		return BACKEND_ERROR_SDO_READ_FAILED;
	}

	for (uint8_t pdo_subindex = 1; pdo_subindex <= pdo_count; ++pdo_subindex) {
		uint16_t pdo_index = 0;
		uint8_t entry_count = 0;

		size = sizeof(pdo_index);
		if (!_read_sdo(context, slave_number, assignment_index, pdo_subindex, &size,
				   &pdo_index)) {
			return BACKEND_ERROR_SDO_READ_FAILED;
		}
		pdo_index = etohs(pdo_index);
		if (pdo_index == 0) {
			continue;
		}

		size = sizeof(entry_count);
		if (!_read_sdo(context, slave_number, pdo_index, 0, &size, &entry_count)) {
			return BACKEND_ERROR_SDO_READ_FAILED;
		}

		for (uint8_t entry_subindex = 1; entry_subindex <= entry_count; ++entry_subindex) {
			uint32_t mapping = 0;
			struct pdo_entry *entry;

			if (slave->pdo_entry_count >= SLAVE_MAX_PDO_ENTRIES) {
				return BACKEND_ERROR_PDO_ENTRY_LIMIT_EXCEEDED;
			}
			size = sizeof(mapping);
			if (!_read_sdo(context, slave_number, pdo_index, entry_subindex, &size,
					   &mapping)) {
				return BACKEND_ERROR_SDO_READ_FAILED;
			}

			mapping = etohl(mapping);
			entry = &slave->pdo_entries[slave->pdo_entry_count++];
			entry->object_index = (uint16_t)(mapping >> 16);
			entry->object_subindex = (uint8_t)(mapping >> 8);
			entry->bit_length = (uint8_t)mapping;
			entry->direction = direction;
		}
	}

	return BACKEND_ERROR_NONE;
}

/**
 * _resolve_pdo_entry_offsets - 解析所有 PDO entry 在 IOmap 中的偏移
 * @context: SOEM 后端上下文指针
 * @entries: PDO entry 映射数组
 * @entry_count: PDO entry 数量
 *
 * 根据 SOEM slave 的输入/输出指针和位宽，计算每个 entry 的 byte_offset
 * 与 bit_offset。
 *
 * Return: 0 成功，非 0 失败
 */
static enum backend_error _resolve_pdo_entry_offsets(struct soem_backend_context *context,
							 struct pdo_image_entry *entries,
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
		struct pdo_image_entry *entry = &entries[i];
		const ec_slavet *slave;
		uint32_t *used_bits;
		uint32_t available_bits;
		const uint8_t *base;
		size_t start_bit;
		size_t end_bit;

		if (entry->slave_entry.slave_index >= (size_t)slave_count) {
			goto cleanup;
		}
		slave = &context->context.slavelist[entry->slave_entry.slave_index + 1];
		if (entry->slave_entry.spec.direction == MO_ECAT_PDO_OUTPUT) {
			used_bits = &used_output_bits[entry->slave_entry.slave_index];
			available_bits = slave->Obits;
			base = slave->outputs;
		} else {
			used_bits = &used_input_bits[entry->slave_entry.slave_index];
			available_bits = slave->Ibits;
			base = slave->inputs;
		}
		if (!base || (*used_bits + entry->slave_entry.spec.bit_length) > available_bits) {
			goto cleanup;
		}

		entry->byte_offset = (uint32_t)(base - context->iomap) + (*used_bits / 8);
		entry->bit_offset = (uint8_t)(*used_bits % 8);
		start_bit = (size_t)entry->byte_offset * 8U + entry->bit_offset;
		end_bit = start_bit + entry->slave_entry.spec.bit_length;
		if (end_bit < start_bit || end_bit > context->pdo_image_size * 8U) {
			goto cleanup;
		}
		*used_bits += entry->slave_entry.spec.bit_length;
	}

	result = BACKEND_ERROR_NONE;
cleanup:
	free(used_output_bits);
	free(used_input_bits);
	return result;
}
