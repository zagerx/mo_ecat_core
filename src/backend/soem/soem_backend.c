/*
 * soem_backend.c - SOEM 后端装配
 *
 * 初始化 SOEM 后端实例，将后端回调表注册到 backend_instance。
 */

#include <stdlib.h>

#include "../backend_ops.h"
#include "soem_backend.h"

/**
 * soem_translation_ops - SOEM 后端数据转换回调表
 *
 * 负责从站信息转换、PDO entry 描述读取以及 PDO 数据映像绑定。
 */
static const struct backend_translation_ops soem_translation_ops = {
	.read_pdo_entries = soem_backend_read_pdo_entries,
	.translate_slave_info = soem_backend_translate_slave_info,
	.get_pdo_image = soem_backend_get_pdo_image,
};

/**
 * soem_ops - SOEM 后端生命周期/运行时回调表
 *
 * 负责后端打开、扫描、DC 配置、PDO 映射、周期通信与关闭。
 */
static const struct backend_ops soem_ops = {
	.open = soem_backend_open,
	.load_slave_info = soem_backend_load_slave_info,
	.configure_dc = soem_backend_configure_dc,
	.build_pdo_mapping = soem_backend_build_pdo_mapping,
	.activate = soem_backend_activate,
	.cyclic_receive = soem_backend_cyclic_receive,
	.cyclic_send = soem_backend_cyclic_send,
	.read_all_slave_states = soem_backend_read_all_slave_states,
	.read_single_slave_state = soem_backend_read_single_slave_state,
	.deactivate = soem_backend_deactivate,
	.close = soem_backend_close,
};

/**
 * backend_init - 初始化后端实例
 * @backend: 后端实例指针
 *
 * 当前实现固定装配 SOEM 后端。
 * 每次调用分配独立的 SOEM 后端上下文，由 backend_close 释放。
 *
 * Return: 0 成功，非 0 失败
 */
enum backend_error backend_init(struct backend_instance *backend)
{
	struct soem_backend_context *context;

	if (!backend) {
		return BACKEND_ERROR_INVALID_ARGUMENT;
	}

	context = calloc(1, sizeof(*context));
	if (!context) {
		return BACKEND_ERROR_NO_MEMORY;
	}

	backend->name = "soem";
	backend->ops = &soem_ops;
	backend->translation_ops = &soem_translation_ops;
	backend->ctx = context;
	return BACKEND_ERROR_NONE;
}
