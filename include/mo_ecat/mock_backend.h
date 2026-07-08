#ifndef MO_ECAT_MOCK_BACKEND_H
#define MO_ECAT_MOCK_BACKEND_H

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 mock 后端
 *
 * mock 后端用于无硬件验证核心层与后端契约，不依赖任何真实网卡或从站。
 */
int mo_ecat_mock_backend_init(struct mo_ecat_backend *backend);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MOCK_BACKEND_H */
