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

#ifdef __cplusplus
}
#endif

#endif /* CLI_THREADS_H */
