/*
 * statemachine.c - 轻量级有限状态机框架实现
 */

#include "mo_ecat/statemachine.h"
#include <stdint.h>

/**
 * sm_init - 初始化状态机
 * @obj: 状态机实例指针
 * @data: 用户私有数据
 * @initial_state: 初始状态函数
 *
 * 所有字段清零，phase 置为 SM_PHASE_ENTER。
 */
void sm_init(struct statemachine *obj, void *data, sm_state_t initial_state)
{
	if (!obj || !initial_state) {
		return;
	}
	obj->phase = SM_PHASE_ENTER;
	obj->data = data;
	obj->current_state = initial_state;
	obj->previous_state = NULL;
	obj->sub_statemachine = NULL;
	obj->next_state = NULL;
}

/**
 * sm_dispatch - 调度状态机
 * @sm: 状态机实例指针
 *
 * 若存在 pending transition，原子化完成旧 SM_PHASE_EXIT → 新 SM_PHASE_ENTER。
 * 否则直接执行当前状态的 phase 逻辑。
 */
void sm_dispatch(struct statemachine *sm)
{
	if (!sm || !sm->current_state) {
		return;
	}

	/* 若存在 pending transition，原子化完成旧 SM_PHASE_EXIT → 新 SM_PHASE_ENTER */
	if (sm->next_state && sm->next_state != sm->current_state) {
		sm_state_t target = sm->next_state;
		sm->next_state = NULL; /* 立即清除，防止回调内再次触发造成递归 */

		sm->phase = SM_PHASE_EXIT;
		sm->current_state(sm);

		sm->previous_state = sm->current_state;
		sm->current_state = target;

		sm->phase = SM_PHASE_ENTER;
		sm->current_state(sm);
		return; /* 切换后本次调度结束 */
	}

	sm->current_state(sm);
}

/**
 * sm_transition - 延迟状态切换（中断安全）
 * @sm: 状态机实例指针
 * @new_state: 目标状态函数
 *
 * 仅将目标状态写入 next_state，不执行回调。
 * 真正的 SM_PHASE_EXIT/SM_PHASE_ENTER 在下次 sm_dispatch() 时完成。
 * 可在任意中断中调用。
 */
void sm_transition(struct statemachine *sm, sm_state_t new_state)
{
	if (!sm || !new_state || new_state == sm->current_state) {
		return;
	}
	sm->next_state = new_state; /* 单条指令赋值，可在任意中断安全调用 */
}

/**
 * sm_transition_sync - 同步状态切换（立即执行 SM_PHASE_EXIT/SM_PHASE_ENTER）
 * @sm: 状态机实例指针
 * @new_state: 目标状态函数
 *
 * 立即在本上下文完成切换：SM_PHASE_EXIT → 更新指针 → SM_PHASE_ENTER。
 * 只能在不会被更高优先级中断抢占的上下文调用。
 */
void sm_transition_sync(struct statemachine *sm, sm_state_t new_state)
{
	if (!sm || !new_state || new_state == sm->current_state) {
		return;
	}
	sm->next_state = new_state;

	/* 立即在本上下文完成切换：SM_PHASE_EXIT → 更新指针 → SM_PHASE_ENTER */
	if (sm->current_state) {
		sm->phase = SM_PHASE_EXIT;
		sm->current_state(sm);
	}
	sm->previous_state = sm->current_state;
	sm->current_state = new_state;
	sm->phase = SM_PHASE_ENTER;
	sm->current_state(sm);
}
