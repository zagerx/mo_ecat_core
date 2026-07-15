/**
 * @file mo_ecat_master.c
 * @brief 主站对象、生命周期与命令接口
 */

#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "master_priv.h"

static int s_master_instance_in_use;

enum mo_ecat_master_cmd master_take_cmd(struct mo_ecat_master *master)
{
	if (!master) {
		return MO_ECAT_MASTER_CMD_NONE;
	}

	return atomic_exchange(&master->command, MO_ECAT_MASTER_CMD_NONE);
}

void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd)
{
	if (master) {
		atomic_store(&master->command, cmd);
	}
}

int mo_ecat_master_init(struct mo_ecat_master *master,
				const struct mo_ecat_master_config *config,
				mo_ecat_cycle_callback callback,
				void *user_data)
{
	if (!master || !config || config->interface_name[0] == '\0') {
		return -1;
	}

	memset(master, 0, sizeof(*master));
	master->config = config;
	master->cycle_callback = callback;
	master->user_data = user_data;
	atomic_init(&master->command, MO_ECAT_MASTER_CMD_NONE);
	atomic_init(&master->state, MO_ECAT_MASTER_STATE_INIT);
	atomic_init(&master->error_code, MO_ECAT_MASTER_ERROR_NONE);
	atomic_init(&master->cycle_result.link_up, 0);
	atomic_init(&master->cycle_result.expected_wkc, 0);
	atomic_init(&master->cycle_result.actual_wkc, 0);
	atomic_init(&master->cycle_result.dc_time_ns, 0);
	atomic_init(&master->cycle_result.dc_time_valid, 0);
	atomic_init(&master->cycle_result.diagnostics_required, 0);
	sm_init(&master->sm, master, master_state_init);
	return 0;
}

void mo_ecat_master_deinit(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	master_release_resources(master);
	memset(master, 0, sizeof(*master));
}

struct mo_ecat_master *mo_ecat_master_create(const struct mo_ecat_master_config *config,
						     mo_ecat_cycle_callback callback,
						     void *user_data)
{
	struct mo_ecat_master *master;

	if (s_master_instance_in_use) {
		return NULL;
	}

	master = calloc(1, sizeof(*master));
	if (!master) {
		return NULL;
	}

	if (mo_ecat_master_init(master, config, callback, user_data) < 0) {
		free(master);
		return NULL;
	}

	s_master_instance_in_use = 1;
	return master;
}

void mo_ecat_master_destroy(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	mo_ecat_master_deinit(master);
	free(master);
	s_master_instance_in_use = 0;
}

int mo_ecat_master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd)
{
	if (!master || cmd <= MO_ECAT_MASTER_CMD_NONE || cmd > MO_ECAT_MASTER_CMD_RESET) {
		return -1;
	}

	master_write_cmd(master, cmd);
	return 0;
}

enum mo_ecat_master_error mo_ecat_master_get_error_code(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_error error;

	if (!master) {
		return MO_ECAT_MASTER_ERROR_NONE;
	}

	error = atomic_load(&master->error_code);
	return error;
}

void mo_ecat_master_dispatch(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	sm_dispatch(&master->sm);
}

enum mo_ecat_master_state mo_ecat_master_get_state(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state;

	if (!master) {
		return MO_ECAT_MASTER_STATE_INIT;
	}

	state = atomic_load(&master->state);
	return state;
}

void mo_ecat_master_set_user_data(struct mo_ecat_master *master, void *user_data)
{
	if (!master) {
		return;
	}

	master->user_data = user_data;
}

void *mo_ecat_master_get_user_data(const struct mo_ecat_master *master)
{
	void *user_data;

	if (!master) {
		return NULL;
	}

	user_data = master->user_data;
	return user_data;
}
