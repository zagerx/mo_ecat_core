#ifndef CLI_STATE_H
#define CLI_STATE_H

#include "mo_ecat/mo_ecat_types.h"
#include "mo_ecat/mo_ecat_backend_cfg.h"
#include "mo_ecat/mo_ecat_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cli_state.h
 * @brief CLI 全局状态声明
 *
 * 所有 CLI 模块共享的状态都集中在这里声明，实际定义位于 main.c。
 */

/** 主站对象，由后台线程和操作命令共用 */
extern struct mo_ecat_master *g_master;

/** 运行标志，控制后台线程退出 */
extern volatile int g_running;

/** 当前使用的顶层配置 */
extern struct mo_ecat_config g_config;

/** 后端初始化选项 */
extern struct mo_ecat_backend_options g_backend_options;

/** 网口名持久缓冲，避免被后续输入覆盖 */
extern char g_ifname_buf[128];

#ifdef __cplusplus
}
#endif

#endif /* CLI_STATE_H */
