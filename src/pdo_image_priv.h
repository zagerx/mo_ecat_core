/*
 * pdo_image_priv.h - 主站 PDO 数据映像结构
 *
 * 定义主站 PDO 过程数据映像结构，用于核心层引用后端适配层持有的 PDO 数据内存。
 */

#ifndef PDO_IMAGE_PRIV_H
#define PDO_IMAGE_PRIV_H

#include <stddef.h>
#include <stdint.h>

/**
 * struct master_pdo_image - 主站 PDO 过程数据映像（Process Data Image）
 * @memory: 适配层持有的 PDO 数据内存起始地址
 * @size: PDO 数据内存总字节数
 *
 * 该映像由后端适配层分配并持有，核心层仅通过本结构引用。
 * memory 指向一片连续内存，按 PDO 映射顺序包含所有从站的
 * 输入/输出过程数据；size 为该片内存的总字节数。
 */
struct master_pdo_image {
    uint8_t *memory;
    size_t size;
};

#endif /* PDO_IMAGE_PRIV_H */
