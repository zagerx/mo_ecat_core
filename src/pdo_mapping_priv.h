/*
 * pdo_mapping_priv.h - PDO entry 内部物理映射结构
 *
 * 定义 PDO entry 在核心层与后端之间的内部物理映射信息。
 */

#ifndef PDO_MAPPING_PRIV_H
#define PDO_MAPPING_PRIV_H

#include "mo_ecat/mo_ecat_pdo.h"

/**
 * struct master_pdo_entry_mapping - PDO entry 的内部物理映射
 * @entry: PDO entry 逻辑描述
 * @byte_offset: 在 PDO 数据区域中的字节偏移
 * @bit_offset: 在 PDO 数据区域中的位偏移
 * @generation: 当前映射代际
 *
 * 逻辑对象身份可提供给应用层；IOmap/domain 中的物理地址与代际只限核心层和
 * 后端使用。
 */
struct master_pdo_entry_mapping {
	struct mo_ecat_pdo_entry entry;
	uint32_t byte_offset;
	uint8_t bit_offset;
	uint32_t generation;
};

#endif /* PDO_MAPPING_PRIV_H */
