#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/soem_backend.h"

static volatile int g_running = 1;

static void signal_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

static void print_state(struct mo_ecat_master *master,
			const struct mo_ecat_cycle_result *result)
{
	enum mo_ecat_master_state state = mo_ecat_master_get_state(master);
	const char *name = "UNKNOWN";

	switch (state) {
	case MO_ECAT_MASTER_STATE_INIT:      name = "INIT";      break;
	case MO_ECAT_MASTER_STATE_IDLE:      name = "IDLE";      break;
	case MO_ECAT_MASTER_STATE_READY:     name = "READY";     break;
	case MO_ECAT_MASTER_STATE_RUNNING:   name = "RUNNING";   break;
	case MO_ECAT_MASTER_STATE_DEGRADED:  name = "DEGRADED";  break;
	case MO_ECAT_MASTER_STATE_FAULT:     name = "FAULT";     break;
	}

	printf("state: %s (%d) | slaves: %zu | WKC: %u/%u | DC: %lld | diag: %d\n",
	       name, state,
	       mo_ecat_master_get_slave_count(master),
	       result->actual_wkc, result->expected_wkc,
	       (long long)result->dc_time_ns,
	       result->diagnostics_required);
}

static void print_help(void)
{
	printf("Commands:\n"
	       "  help                show this help\n"
	       "  state               print master state\n"
	       "  dispatch            call mo_ecat_master_dispatch() once\n"
	       "  config <ifname>     submit CONFIGURE command\n"
	       "  activate            submit ACTIVATE command\n"
	       "  deactivate          submit DEACTIVATE command\n"
	       "  reset               submit RESET command\n"
	       "  cycle               call cycle_begin() + cycle_end() once\n"
	       "  diag                call read_diagnostics() and print slave states\n"
	       "  pdo <idx>           print PDO ref info\n"
	       "  exit                quit\n");
}

static void print_diagnostics(struct mo_ecat_master *master)
{
	int rc = mo_ecat_master_read_diagnostics(master);
	if (rc < 0) {
		printf("read_diagnostics failed: %d\n", rc);
		return;
	}

	size_t count = mo_ecat_master_get_slave_count(master);
	for (size_t i = 0; i < count; ++i) {
		const struct mo_ecat_slave *slave = mo_ecat_master_get_slave(master, i);
		if (!slave) {
			continue;
		}
		printf("  Slave[%zu]: %s, al_state=%d, online=%d, op=%d, err=%d, code=0x%04X\n",
		       i, slave->name,
		       slave->state.al_state,
		       slave->state.online,
		       slave->state.operational,
		       slave->state.error,
		       slave->state.al_status_code);
	}
}

static void print_pdo_ref(struct mo_ecat_master *master, size_t idx)
{
	size_t count = mo_ecat_master_get_pdo_ref_count(master);
	if (idx >= count) {
		printf("PDO ref index out of range (count=%zu)\n", count);
		return;
	}

	const struct mo_ecat_pdo_ref *ref = mo_ecat_master_get_pdo_ref(master, idx);
	const char *dir = (ref->direction == MO_ECAT_PDO_INPUT) ? "IN" : "OUT";

	printf("PDO[%zu]: slave=%zu, pdo=0x%04X, entry=%u, "
	       "dir=%s, offset=%u.%u, bits=%u, gen=%u\n",
	       idx, ref->slave_index, ref->pdo_index, ref->entry_index,
	       dir, ref->byte_offset, ref->bit_offset, ref->bit_length,
	       ref->generation);

	if (ref->direction == MO_ECAT_PDO_INPUT) {
		const void *p = mo_ecat_pdo_input(master, ref);
		if (p) {
			printf("  input value @ %p\n", p);
		}
	} else {
		void *p = mo_ecat_pdo_output(master, ref);
		if (p) {
			printf("  output value @ %p\n", p);
		}
	}
}

int main(int argc, char *argv[])
{
	const char *default_ifname = (argc > 1) ? argv[1] : "eth0";

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	printf("EtherCAT CLI test harness (decoupled backend)\n");
	printf("Type 'help' for commands.\n");

	struct mo_ecat_pdo_entry_config slave0_pdos[] = {
		{0x7000, 0x01, 16, MO_ECAT_PDO_OUTPUT},
		{0x6000, 0x01, 16, MO_ECAT_PDO_INPUT},
	};

	struct mo_ecat_slave_config slaves[] = {
		{
			.alias = 0,
			.position = 0,
			.vendor_id = 0,
			.product_code = 0,
			.revision_number = 0,
			.pdo_entries = slave0_pdos,
			.pdo_entry_count = 2,
			.dc_active = 0,
		},
	};

	struct mo_ecat_config config = {
		.interface_name = default_ifname,
		.slaves = slaves,
		.slave_count = 1,
	};

	static char ifname_buf[128];
	strncpy(ifname_buf, default_ifname, sizeof(ifname_buf) - 1);
	ifname_buf[sizeof(ifname_buf) - 1] = '\0';

	struct mo_ecat_soem_options soem_options = {
		.process_image_capacity = 4096,
	};

	struct mo_ecat_backend backend;
	memset(&backend, 0, sizeof(backend));

	struct mo_ecat_master *master = mo_ecat_master_create();
	if (!master) {
		fprintf(stderr, "Failed to create master\n");
		return -1;
	}

	struct mo_ecat_cycle_result result = {0};
	char line[256];

	while (g_running) {
		printf("> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		/* strip trailing newline */
		line[strcspn(line, "\n")] = '\0';

		/* skip leading spaces */
		char *p = line;
		while (isspace((unsigned char)*p)) {
			++p;
		}
		if (*p == '\0') {
			continue;
		}

		char cmd[32] = {0};
		char arg[128] = {0};
		sscanf(p, "%31s %127s", cmd, arg);

		if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
			print_help();
		} else if (strcmp(cmd, "state") == 0 || strcmp(cmd, "status") == 0) {
			print_state(master, &result);
		} else if (strcmp(cmd, "dispatch") == 0 || strcmp(cmd, "d") == 0) {
			mo_ecat_master_dispatch(master);
			print_state(master, &result);
		} else if (strcmp(cmd, "config") == 0) {
			const char *ifname = (arg[0] != '\0') ? arg : default_ifname;
			strncpy(ifname_buf, ifname, sizeof(ifname_buf) - 1);
			ifname_buf[sizeof(ifname_buf) - 1] = '\0';
			config.interface_name = ifname_buf;

			/* backend ctx 可能在之前的 reset/destroy 中被释放，重新初始化 */
			if (backend.ctx) {
				if (backend.ops && backend.ops->close) {
					backend.ops->close(&backend);
				}
			}
			if (mo_ecat_soem_backend_init(&backend, &soem_options) < 0) {
				printf("SOEM backend init failed\n");
				continue;
			}

			int rc = mo_ecat_master_configure(master, &config, &backend);
			printf("configure('%s') submit: %d\n", ifname_buf, rc);
		} else if (strcmp(cmd, "activate") == 0) {
			int rc = mo_ecat_master_activate(master);
			printf("activate submit: %d\n", rc);
		} else if (strcmp(cmd, "deactivate") == 0) {
			int rc = mo_ecat_master_deactivate(master);
			printf("deactivate submit: %d\n", rc);
		} else if (strcmp(cmd, "reset") == 0) {
			int rc = mo_ecat_master_reset(master);
			printf("reset submit: %d\n", rc);
		} else if (strcmp(cmd, "cycle") == 0 || strcmp(cmd, "c") == 0) {
			int rc1 = mo_ecat_master_cycle_begin(master, &result);
			int rc2 = mo_ecat_master_cycle_end(master, &result);
			printf("cycle: begin=%d, end=%d\n", rc1, rc2);
			print_state(master, &result);
		} else if (strcmp(cmd, "diag") == 0) {
			print_diagnostics(master);
		} else if (strcmp(cmd, "pdo") == 0) {
			if (arg[0] == '\0') {
				printf("usage: pdo <idx>\n");
				continue;
			}
			print_pdo_ref(master, (size_t)atoi(arg));
		} else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
			break;
		} else {
			printf("unknown command: %s\n", cmd);
		}
	}

	printf("\nStopping...\n");
	mo_ecat_master_destroy(master);

	return 0;
}
