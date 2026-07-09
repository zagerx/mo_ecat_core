#ifndef MO_ECAT_BACKEND_CFG_H
#define MO_ECAT_BACKEND_CFG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 后端初始化选项
 *
 * 当前仅包含过程数据映像容量。不同后端实现可以按需解释该字段。
 */
struct mo_ecat_backend_options {
    size_t process_image_capacity;
};

/**
 * @brief 后端配置描述符
 *
 * 调用者通过该结构告诉核心层后端的初始化选项。
 * 具体编译哪个后端由 CMake 选项决定，运行时不再做后端分发。
 */
struct mo_ecat_backend_config {
    struct mo_ecat_backend_options options;
};

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_BACKEND_CFG_H */
