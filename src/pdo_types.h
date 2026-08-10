/*
 * pdo_types.h - PDO 相关对象类型定义
 *
 * 定义核心层内部使用的 PDO 数据映像、映像条目及布局对象。
 */

#ifndef PDO_TYPES_H
#define PDO_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "mo_ecat/mo_ecat_pdo.h"

/**
 * struct pdo_image - PDO 过程数据映像（Process Data Image）
 * @memory: 适配层持有的 PDO 数据内存起始地址
 * @size: PDO 数据内存总字节数
 *
 * 该映像由后端适配层分配并持有，核心层仅通过本结构引用。
 * memory 指向一片连续内存，按 PDO 映射顺序包含所有从站的
 * 输入/输出过程数据；size 为该片内存的总字节数。
 */
struct pdo_image {
	uint8_t *memory;
	size_t size;
};

/**
 * struct pdo_image_entry - PDO 映像中的单个条目
 * @record: Master 发现的 PDO entry 记录
 * @byte_offset: 在 PDO 数据区域中的字节偏移
 * @bit_offset: 在 PDO 数据区域中的位偏移
 *
 * 逻辑对象身份可提供给应用层；IOmap/domain 中的物理地址只限核心层和
 * 后端使用。布局代际统一保存在 pdo_layout 中。
 */
struct pdo_image_entry {
	struct pdo_entry_record record;
	uint32_t byte_offset;
	uint8_t bit_offset;
};

/**
 * struct pdo_layout - PDO 数据映像布局
 * @image: PDO 数据区域
 * @entries: PDO 映像条目数组；每个元素保存条目记录、数据映像字节/位偏移
 *           和所属布局代际
 * @entry_count: PDO entry 映射数量
 * @is_active: 是否允许周期读写
 *
 * 保存主站 PDO 数据区域、所有 PDO entry 的地址映射以及映射运行状态。
 */
struct pdo_layout {
	struct pdo_image image;
	struct pdo_image_entry *entries;
	size_t entry_count;
	int is_active;
};

#endif /* PDO_TYPES_H */
