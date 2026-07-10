/**
 * @file threads.c
 * @brief CLI 后台线程实现
 */

#include <unistd.h>

#include "threads.h"
#include "cli_state.h"
#include "mo_ecat/mo_ecat_master.h"

void *dispatch_thread_routine(void *arg)
{
	(void)arg;

	while (g_running) {
		mo_ecat_master_dispatch(g_master);
		usleep(1000);
	}

	return NULL;
}
