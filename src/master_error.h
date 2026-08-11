/*
 * master_error.h - 主站核心层内部错误模型
 *
 * 核心层与后端共用 enum backend_error 作为技术错误码，
 * 本头文件只保留错误来源标记与故障记录结构。
 */

#ifndef MASTER_ERROR_H
#define MASTER_ERROR_H

#include <stddef.h>
#include <stdint.h>

#include "backend/backend_error.h"
#include "mo_ecat/mo_ecat_master.h"

/** 错误来源。 */
enum master_error_source {
	MASTER_ERROR_SOURCE_CORE,
	MASTER_ERROR_SOURCE_BACKEND,
	MASTER_ERROR_SOURCE_SOEM,
};

/** 最近一次主站故障的内部记录。 */
struct master_error_record {
	enum mo_ecat_master_error master_error;
	enum backend_error detail;
	enum master_error_source source;
	int native_code;
	size_t slave_index;
	uint16_t object_index;
	uint8_t object_subindex;
};

#endif /* MASTER_ERROR_H */
