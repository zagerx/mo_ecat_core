#ifndef MO_ECAT_MASTER_PRIV_H
#define MO_ECAT_MASTER_PRIV_H

#include <stddef.h>
#include <pthread.h>

#include "mo_ecat/mo_ecat_types.h"
#include "common/statemachine/statemachine.h"

#ifdef __cplusplus
extern "C" {
#endif

enum mo_ecat_master_command {
	MO_ECAT_MASTER_CMD_NONE,
	MO_ECAT_MASTER_CMD_CONFIGURE,
	MO_ECAT_MASTER_CMD_ACTIVATE,
	MO_ECAT_MASTER_CMD_DEACTIVATE,
	MO_ECAT_MASTER_CMD_RESET
};

struct mo_ecat_master {
	struct statemachine sm;
	struct mo_ecat_backend backend;
	struct mo_ecat_config config;
	struct mo_ecat_process_image image;
	struct mo_ecat_slave *slaves;
	struct mo_ecat_slave_state *diagnostics;
	struct mo_ecat_pdo_ref *pdo_refs;
	size_t pdo_ref_count;
	enum mo_ecat_master_state state;
	enum mo_ecat_master_command command;
	int command_pending;
	int command_result;
	const struct mo_ecat_config *pending_config;
	struct mo_ecat_backend *pending_backend;
	int cycle_result_pending;
	int cycle_abnormal;
	unsigned int consecutive_cycle_errors;
	void *user_data;
	struct mo_ecat_cycle_result last_result;
	pthread_mutex_t lock;
};

int mo_ecat_master_prepare_config(struct mo_ecat_master *master,
				  const struct mo_ecat_config *config,
				  struct mo_ecat_backend *backend);
int mo_ecat_master_backend_configure(struct mo_ecat_master *master);
int mo_ecat_master_backend_activate(struct mo_ecat_master *master);
int mo_ecat_master_backend_deactivate(struct mo_ecat_master *master);
void mo_ecat_master_release_resources(struct mo_ecat_master *master);
void mo_ecat_master_clear_command(struct mo_ecat_master *master, int result);
int mo_ecat_master_take_cycle_result(struct mo_ecat_master *master, int *abnormal);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_PRIV_H */
