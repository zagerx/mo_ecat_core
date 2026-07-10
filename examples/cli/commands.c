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

void cmd_discover(void)
{
	int rc = mo_ecat_master_write_cmd(g_master,
		MO_ECAT_MASTER_CMD_DISCOVER);
	if (rc == 0) {
		printf("Discovery requested on %s.\n", g_options.interface_name);
	} else {
		printf("Failed to request discovery.\n");
	}
}

void cmd_reset(void)
{
	int rc = mo_ecat_master_write_cmd(g_master,
		MO_ECAT_MASTER_CMD_RESET);
	if (rc == 0) {
		printf("Reset requested.\n");
	} else {
		printf("Failed to request reset.\n");
	}
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
	printf("Unknown command '%s'. Type 'help' for available commands.\n", cmd);
}
