#ifndef MO_ECAT_SOEM_BACKEND_H
#define MO_ECAT_SOEM_BACKEND_H

#include <stddef.h>

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * @brief 初始化 SOEM backend 实例
 *
 * 该函数用于需要自行持有 backend 实例的高级场景。一般情况下，应使用
 * mo_ecat_master_configure() 并传入 MO_ECAT_BACKEND_SOEM，由核心层
 * 负责创建和销毁 backend。
 */
int mo_ecat_soem_backend_init(struct mo_ecat_backend *backend,
                              const struct mo_ecat_soem_options *options);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_SOEM_BACKEND_H */
