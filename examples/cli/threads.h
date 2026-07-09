#ifndef CLI_THREADS_H
#define CLI_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file threads.h
 * @brief CLI 后台线程入口
 */

/**
 * @brief 状态机调度线程
 *
 * 以固定周期推进主站状态机，处理命令与生命周期迁移。
 */
void *dispatch_thread_routine(void *arg);

/**
 * @brief 周期收发线程
 *
 * 当主站处于 RUNNING 或 DEGRADED 时，执行 PDO 收发。
 */
void *cycle_thread_routine(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* CLI_THREADS_H */
