#ifndef PROCESS_IMAGE_H
#define PROCESS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

/** 主站内部过程数据映像。 */
struct mo_ecat_process_image {
    uint8_t *memory;
    size_t size;
    uint32_t generation;
    int active;
};

#endif /* PROCESS_IMAGE_H */
