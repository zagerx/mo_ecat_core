/*
 * pdo_image_entry_priv.h - PDO 映像条目内部结构
 *
 * 定义 PDO entry 在核心层与后端之间的内部物理映射信息。
 */

#ifndef PDO_IMAGE_ENTRY_PRIV_H
#define PDO_IMAGE_ENTRY_PRIV_H

#include "mo_ecat/mo_ecat_pdo.h"

/**
 * struct pdo_image_entry - PDO 映像中的单个条目
 * @record: Master 发现的 PDO entry 记录
 * @byte_offset: 在 PDO 数据区域中的字节偏移
 * @bit_offset: 在 PDO 数据区域中的位偏移
 * @generation: 当前映射代际
 *
 * 逻辑对象身份可提供给应用层；IOmap/domain 中的物理地址与代际只限核心层和
 * 后端使用。
 */
struct pdo_image_entry {
	struct pdo_entry_record record;
	uint32_t byte_offset;
	uint8_t bit_offset;
	uint32_t generation;
};

#endif /* PDO_IMAGE_ENTRY_PRIV_H */
