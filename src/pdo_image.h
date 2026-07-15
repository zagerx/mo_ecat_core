#ifndef PDO_IMAGE_H
#define PDO_IMAGE_H

#include <stddef.h>
#include <stdint.h>

/** 主站内部 PDO 数据区域。 */
struct master_pdo_image {
    uint8_t *memory;
    size_t size;
};

#endif /* PDO_IMAGE_H */
