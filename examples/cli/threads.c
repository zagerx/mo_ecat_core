/**
 * @file threads.c
 * @brief CLI 后台线程实现
 */

#include <unistd.h>

#include "threads.h"
#include "cli_state.h"
#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_pdo.h"

void *dispatch_thread_routine(void *arg)
{
	(void)arg;

	while (g_running) {
		mo_ecat_master_dispatch(g_master);
		usleep(1000);
	}

	return NULL;
}

void *cycle_thread_routine(void *arg)
{
	(void)arg;

	while (g_running) {
		enum mo_ecat_master_state state = mo_ecat_master_get_state(g_master);
		if (state == MO_ECAT_MASTER_STATE_RUNNING ||
		    state == MO_ECAT_MASTER_STATE_DEGRADED) {
			struct mo_ecat_cycle_result result = {0};
			mo_ecat_master_cycle_begin(g_master, &result);
			mo_ecat_master_cycle_end(g_master, &result);
		}
		usleep(1000);
	}

	return NULL;
}
