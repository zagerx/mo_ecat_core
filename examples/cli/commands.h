#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file commands.h
 * @brief CLI 命令处理函数
 *
 * 每个函数对应一条用户命令，内部调用核心层 API。
 */

/**
 * @brief 处理 state 命令
 */
void cmd_state(void);

/**
 * @brief 处理 config 命令
 * @param ifname 网口名，空字符串表示使用默认值
 */
void cmd_config(const char *ifname);

/**
 * @brief 处理 activate 命令
 */
void cmd_activate(void);

/**
 * @brief 处理 deactivate 命令
 */
void cmd_deactivate(void);

/**
 * @brief 处理 reset 命令
 */
void cmd_reset(void);

/**
 * @brief 处理 diag 命令
 */
void cmd_diag(void);

/**
 * @brief 处理 pdo 命令
 * @param arg 索引字符串
 */
void cmd_pdo(const char *arg);

/**
 * @brief 处理 help 命令
 */
void cmd_help(void);

/**
 * @brief 处理未知命令
 * @param cmd 用户输入的命令名
 */
void cmd_unknown(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* CLI_COMMANDS_H */
