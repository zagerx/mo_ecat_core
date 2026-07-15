#ifndef MO_ECAT_COMMON_H
#define MO_ECAT_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MO_ECAT_MAX_NAME_LEN   80
#define MO_ECAT_MAX_IFNAME_LEN 64

/** 主站视角的周期数据方向。 */
enum mo_ecat_cyclic_direction {
    MO_ECAT_CYCLIC_INPUT,
    MO_ECAT_CYCLIC_OUTPUT
};

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_COMMON_H */
