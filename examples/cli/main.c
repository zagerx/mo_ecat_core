/**
 * @file main.c
 * @brief CLI 示例入口
 *
 * 本文件只负责：
 * - 初始化全局状态和默认配置
 * - 创建主站对象和后台线程
 * - 读取用户命令并分发给 commands.c
 * - 退出时停止线程并销毁资源
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "cli_state.h"
#include "threads.h"
#include "commands.h"
#include "mo_ecat/mo_ecat_types.h"
#include "mo_ecat/mo_ecat_master.h"

/* 全局状态定义，其他模块通过 cli_state.h 引用 */
struct mo_ecat_master *g_master = NULL;
volatile int g_running = 1;
struct mo_ecat_config g_config = {0};
char g_ifname_buf[128] = {0};

static void signal_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

/**
 * @brief 解析一行输入，得到命令和参数
 */
static void parse_line(char *line, char *cmd, size_t cmd_size, char *arg, size_t arg_size)
{
	char *p = line;

	cmd[0] = '\0';
	arg[0] = '\0';

	while (isspace((unsigned char)*p)) {
		++p;
	}
	if (*p == '\0') {
		return;
	}

	sscanf(p, "%255s %255s", cmd, arg);
}

int main(int argc, char *argv[])
{
	const char *default_ifname = (argc > 1) ? argv[1] : "eth0";

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* 初始化默认网口名 */
	strncpy(g_ifname_buf, default_ifname, sizeof(g_ifname_buf) - 1);
	g_ifname_buf[sizeof(g_ifname_buf) - 1] = '\0';

	/* 初始化默认配置 */
	strncpy(g_config.interface_name, g_ifname_buf, sizeof(g_config.interface_name) - 1);
	g_config.interface_name[sizeof(g_config.interface_name) - 1] = '\0';

	g_config.slaves[0].alias = 0;
	g_config.slaves[0].position = 0;
	g_config.slaves[0].vendor_id = 0;
	g_config.slaves[0].product_code = 0;
	g_config.slaves[0].revision_number = 0;
	g_config.slaves[0].pdo_entries[0] =
	    (struct mo_ecat_pdo_entry_config){0x7000, 0x01, 16, MO_ECAT_PDO_OUTPUT};
	g_config.slaves[0].pdo_entries[1] =
	    (struct mo_ecat_pdo_entry_config){0x6000, 0x01, 16, MO_ECAT_PDO_INPUT};
	g_config.slaves[0].pdo_entry_count = 2;
	g_config.slaves[0].dc_active = 0;
	g_config.slave_count = 1;

	printf("EtherCAT CLI test harness (decoupled backend)\n");
	printf("Dispatch thread and cycle thread run automatically.\n");
	printf("Type 'help' for commands.\n");

	g_master = mo_ecat_master_create(&g_config);
	if (!g_master) {
		fprintf(stderr, "Failed to create master\n");
		return -1;
	}

	pthread_t dispatch_thread;
	pthread_t cycle_thread;

	if (pthread_create(&dispatch_thread, NULL, dispatch_thread_routine, NULL) != 0) {
		fprintf(stderr, "Failed to create dispatch thread\n");
		mo_ecat_master_destroy(g_master);
		return -1;
	}

	if (pthread_create(&cycle_thread, NULL, cycle_thread_routine, NULL) != 0) {
		fprintf(stderr, "Failed to create cycle thread\n");
		g_running = 0;
		pthread_join(dispatch_thread, NULL);
		mo_ecat_master_destroy(g_master);
		return -1;
	}

	char line[256];
	char cmd[256];
	char arg[256];

	while (g_running) {
		printf("> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		line[strcspn(line, "\n")] = '\0';
		parse_line(line, cmd, sizeof(cmd), arg, sizeof(arg));

		if (cmd[0] == '\0') {
			continue;
		}

		if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
			cmd_help();
		} else if (strcmp(cmd, "state") == 0 || strcmp(cmd, "status") == 0) {
			cmd_state();
		} else if (strcmp(cmd, "config") == 0) {
			cmd_config(arg);
		} else if (strcmp(cmd, "activate") == 0) {
			cmd_activate();
		} else if (strcmp(cmd, "deactivate") == 0) {
			cmd_deactivate();
		} else if (strcmp(cmd, "reset") == 0) {
			cmd_reset();
		} else if (strcmp(cmd, "diag") == 0) {
			cmd_diag();
		} else if (strcmp(cmd, "pdo") == 0) {
			cmd_pdo(arg);
		} else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
			break;
		} else {
			cmd_unknown(cmd);
		}
	}

	printf("\nStopping...\n");
	g_running = 0;
	pthread_join(dispatch_thread, NULL);
	pthread_join(cycle_thread, NULL);
	mo_ecat_master_destroy(g_master);

	return 0;
}
