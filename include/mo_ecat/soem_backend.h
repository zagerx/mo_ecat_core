#ifndef MO_ECAT_SOEM_BACKEND_H
#define MO_ECAT_SOEM_BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_soem_options {
    size_t process_image_capacity;
};

int mo_ecat_soem_backend_init(struct mo_ecat_backend *backend,
                              const struct mo_ecat_soem_options *options);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_SOEM_BACKEND_H */
