/**
 * @file soem_backend.c
 * @brief SOEM 后端装配
 */

#include <string.h>

#include "../backend_ops.h"
#include "soem_backend.h"

static struct soem_backend_context s_soem_context;

static const struct backend_translation_ops soem_translation_ops = {
	.read_pdo_entries = soem_backend_read_pdo_entries,
	.translate_slave_info = soem_backend_translate_slave_info,
	.get_pdo_image = soem_backend_get_pdo_image,
};

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

int backend_init(struct backend_instance *backend)
{
	if (!backend) {
		return -1;
	}

	memset(&s_soem_context, 0, sizeof(s_soem_context));
	backend->name = "soem";
	backend->ops = &soem_ops;
	backend->translation_ops = &soem_translation_ops;
	backend->ctx = &s_soem_context;
	return 0;
}
