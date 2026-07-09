/**
 * @file commands.c
 * @brief CLI 命令处理函数实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "cli_state.h"
#include "print.h"
#include "mo_ecat/mo_ecat_master.h"

void cmd_state(void)
{
	print_state(g_master);
}

void cmd_config(const char *ifname)
{
	const char *name = (ifname && ifname[0] != '\0') ? ifname : g_ifname_buf;

	strncpy(g_ifname_buf, name, sizeof(g_ifname_buf) - 1);
	g_ifname_buf[sizeof(g_ifname_buf) - 1] = '\0';
	g_config.interface_name = g_ifname_buf;

	const struct mo_ecat_backend_config backend_config = {
		.type = MO_ECAT_BACKEND_SOEM,
		.options.soem = g_soem_options,
	};

	int rc = mo_ecat_master_configure(g_master, &g_config, &backend_config);
	printf("configure('%s') submit: %d\n", g_ifname_buf, rc);
}

void cmd_activate(void)
{
	int rc = mo_ecat_master_activate(g_master);
	printf("activate submit: %d\n", rc);
}

void cmd_deactivate(void)
{
	int rc = mo_ecat_master_deactivate(g_master);
	printf("deactivate submit: %d\n", rc);
}

void cmd_reset(void)
{
	int rc = mo_ecat_master_reset(g_master);
	printf("reset submit: %d\n", rc);
}

void cmd_diag(void)
{
	print_diagnostics(g_master);
}

void cmd_pdo(const char *arg)
{
	if (!arg || arg[0] == '\0') {
		printf("usage: pdo <idx>\n");
		return;
	}
	print_pdo_ref(g_master, (size_t)atoi(arg));
}

void cmd_help(void)
{
	print_help();
}

void cmd_unknown(const char *cmd)
{
	printf("unknown command: %s\n", cmd);
}
