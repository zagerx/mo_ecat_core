/*
 * statemachine.h - 轻量级有限状态机框架
 *
 * 专为嵌入式硬实时场景设计，支持延迟切换（deferred transition）
 * 以避免中断嵌套导致的状态机重入。
 */

#ifndef _STATEMACHINE_H
#define _STATEMACHINE_H

#include <stdint.h>
#include <stddef.h>

/**
 * enum statemachine_phase - 状态机内置阶段
 * @SM_PHASE_ENTER: 进入新状态阶段
 * @SM_PHASE_EXIT: 退出旧状态阶段
 * @SM_PHASE_START: 状态运行阶段起点，用户可在此之上定义子阶段
 */
enum {
	SM_PHASE_ENTER = 0,
	SM_PHASE_EXIT,
	SM_PHASE_START
};

struct statemachine;
typedef void (*sm_state_t)(struct statemachine *);

/**
 * struct statemachine - 状态机实例
 * @phase: 当前阶段：SM_PHASE_ENTER / SM_PHASE_EXIT / 用户自定义
 * @count: 通用计数器，由用户代码自行维护
 * @data: 用户私有数据
 * @current_state: 当前状态函数
 * @previous_state: 上一个状态函数
 * @sub_statemachine: 子状态机指针（预留，框架不自动调度）
 * @next_state: 延迟切换目标，NULL 表示无 pending
 */
struct statemachine {
	volatile int16_t phase;
	volatile uint32_t count;
	void *data;
	sm_state_t current_state;
	sm_state_t previous_state;
	struct statemachine *sub_statemachine;
	sm_state_t next_state;
};

void sm_init(struct statemachine *obj, void *data, sm_state_t initial_state);

void sm_dispatch(struct statemachine *sm);

void sm_transition(struct statemachine *sm, sm_state_t new_state);

void sm_transition_sync(struct statemachine *sm, sm_state_t new_state);

#endif /* _STATEMACHINE_H */
