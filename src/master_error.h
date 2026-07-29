/*
 * master_error.h - 主站核心层内部错误模型
 */

#ifndef MASTER_ERROR_H
#define MASTER_ERROR_H

#include <stddef.h>
#include <stdint.h>

#include "backend/backend_error.h"
#include "mo_ecat/mo_ecat_master.h"

/** 核心层详细错误码。 */
enum master_error_detail {
	MASTER_ERROR_NONE = 0,
	MASTER_ERROR_INVALID_ARGUMENT,
	MASTER_ERROR_INVALID_STATE,
	MASTER_ERROR_NO_MEMORY,
	MASTER_ERROR_BACKEND_OPEN_FAILED,
	MASTER_ERROR_BUS_SCAN_FAILED,
	MASTER_ERROR_READ_PDO_ASSIGNMENT_FAILED,
	MASTER_ERROR_PDO_ENTRY_LIMIT_EXCEEDED,
	MASTER_ERROR_DC_UNSUPPORTED,
	MASTER_ERROR_DC_CONFIG_FAILED,
	MASTER_ERROR_PDO_IMAGE_LIMIT_EXCEEDED,
	MASTER_ERROR_PDO_MAPPING_FAILED,
	MASTER_ERROR_PDO_OFFSET_RESOLVE_FAILED,
	MASTER_ERROR_ACTIVATE_FAILED,
	MASTER_ERROR_CYCLIC_RECEIVE_FAILED,
	MASTER_ERROR_CYCLIC_SEND_FAILED,
	MASTER_ERROR_READ_NODE_STATE_FAILED,
	MASTER_ERROR_DEACTIVATE_FAILED,
};

/** 错误来源。 */
enum master_error_source {
	MASTER_ERROR_SOURCE_CORE,
	MASTER_ERROR_SOURCE_BACKEND,
	MASTER_ERROR_SOURCE_SOEM,
};

/** 最近一次主站故障的内部记录。 */
struct master_error_record {
	enum mo_ecat_master_error master_error;
	enum master_error_detail detail;
	enum master_error_source source;
	int native_code;
	size_t slave_index;
	uint16_t object_index;
	uint8_t object_subindex;
};

enum master_error_detail master_error_from_backend(enum backend_error error);

#endif /* MASTER_ERROR_H */
