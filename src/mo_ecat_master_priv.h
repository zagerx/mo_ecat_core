#ifndef MO_ECAT_MASTER_PRIV_H
#define MO_ECAT_MASTER_PRIV_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master {
    struct mo_ecat_backend backend;
    struct mo_ecat_config  config;
    struct mo_ecat_process_image image;
    struct mo_ecat_slave  *slaves;
    struct mo_ecat_slave_state *diagnostics;
    struct mo_ecat_pdo_ref *pdo_refs;
    size_t                 pdo_ref_count;
    enum mo_ecat_master_state state;
    unsigned int           consecutive_cycle_errors;
    void                  *user_data;
};

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_PRIV_H */
