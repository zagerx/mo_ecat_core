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

enum mo_ecat_master_state master_state_from_sm(const struct mo_ecat_master *master)
{
	sm_state_t current_state;

	if (!master) {
		return MO_ECAT_MASTER_STATE_INIT;
	}

	current_state = master->sm.current_state;
	if (current_state == master_state_idle) {
		return MO_ECAT_MASTER_STATE_IDLE;
	}
	if (current_state == master_state_discovered) {
		return MO_ECAT_MASTER_STATE_DISCOVERED;
	}
	if (current_state == master_state_ready) {
		return MO_ECAT_MASTER_STATE_READY;
	}
	if (current_state == master_state_running) {
		return MO_ECAT_MASTER_STATE_RUNNING;
	}
	if (current_state == master_state_fault) {
		return MO_ECAT_MASTER_STATE_FAULT;
	}

	return MO_ECAT_MASTER_STATE_INIT;
}

enum mo_ecat_master_cmd master_read_cmd(const struct mo_ecat_master *master)
{
	if (!master) {
		return MO_ECAT_MASTER_CMD_NONE;
	}

	return master->command;
}

void master_write_cmd(struct mo_ecat_master *master, enum mo_ecat_master_cmd cmd)
{
	if (master) {
		master->command = cmd;
	}
}

int mo_ecat_master_init(struct mo_ecat_master *master, const struct mo_ecat_master_config *config)
{
	if (!master || !config || config->interface_name[0] == '\0') {
		return -1;
	}

	memset(master, 0, sizeof(*master));
	if (pthread_mutex_init(&master->lock, NULL) != 0) {
		return -1;
	}

	master->config = *config;
	statemachine_init(&master->sm, master, master_state_init);
	return 0;
}

void mo_ecat_master_deinit(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	pthread_mutex_lock(&master->lock);
	master_release_resources(master);
	pthread_mutex_unlock(&master->lock);

	pthread_mutex_destroy(&master->lock);
	memset(master, 0, sizeof(*master));
}

struct mo_ecat_master *mo_ecat_master_create(const struct mo_ecat_master_config *config)
{
	struct mo_ecat_master *master;

	if (s_master_instance_in_use) {
		return NULL;
	}

	master = calloc(1, sizeof(*master));
	if (!master) {
		return NULL;
	}

	if (mo_ecat_master_init(master, config) < 0) {
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

	pthread_mutex_lock(&master->lock);
	master_write_cmd(master, cmd);
	pthread_mutex_unlock(&master->lock);
	return 0;
}

enum mo_ecat_master_error mo_ecat_master_get_error_code(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_error error;

	if (!master) {
		return MO_ECAT_MASTER_ERROR_NONE;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	error = master->error_code;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return error;
}

void mo_ecat_master_dispatch(struct mo_ecat_master *master)
{
	if (!master) {
		return;
	}

	pthread_mutex_lock(&master->lock);
	sm_dispatch(&master->sm);
	pthread_mutex_unlock(&master->lock);
}

enum mo_ecat_master_state mo_ecat_master_get_state(const struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state;

	if (!master) {
		return MO_ECAT_MASTER_STATE_INIT;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	state = master_state_from_sm(master);
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return state;
}

void mo_ecat_master_set_user_data(struct mo_ecat_master *master, void *user_data)
{
	if (!master) {
		return;
	}

	pthread_mutex_lock(&master->lock);
	master->user_data = user_data;
	pthread_mutex_unlock(&master->lock);
}

void *mo_ecat_master_get_user_data(const struct mo_ecat_master *master)
{
	void *user_data;

	if (!master) {
		return NULL;
	}

	pthread_mutex_lock((pthread_mutex_t *)&master->lock);
	user_data = master->user_data;
	pthread_mutex_unlock((pthread_mutex_t *)&master->lock);
	return user_data;
}
