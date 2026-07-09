#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/soem_backend.h"

static volatile int g_running = 1;

static void signal_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

static int run_master_loop(struct mo_ecat_backend *backend, const struct mo_ecat_config *config)
{
	enum {
		WAIT_IDLE,
		WAIT_READY,
		WAIT_RUNNING,
		RUNNING,
		FAILED
	} phase = WAIT_IDLE;
	const int startup_timeout_cycles = 10000;
	int startup_wait_cycles = 0;
	int rc = 0;

	struct mo_ecat_master *master = mo_ecat_master_create();
	if (!master) {
		fprintf(stderr, "Failed to create master\n");
		return -1;
	}

	struct mo_ecat_cycle_result result = {0};
	int print_counter = 0;

	while (g_running) {
		mo_ecat_master_dispatch(master);

		enum mo_ecat_master_state state = mo_ecat_master_get_state(master);
		if (state == MO_ECAT_MASTER_STATE_FAULT) {
			fprintf(stderr, "Master entered FAULT state\n");
			rc = -1;
			phase = FAILED;
		}

		switch (phase) {
		case WAIT_IDLE:
			if (state == MO_ECAT_MASTER_STATE_IDLE) {
				if (mo_ecat_master_configure(master, config, backend) < 0) {
					fprintf(stderr, "Failed to submit configure command\n");
					rc = -1;
					phase = FAILED;
					break;
				}
				startup_wait_cycles = 0;
				phase = WAIT_READY;
			} else if (++startup_wait_cycles >= startup_timeout_cycles) {
				fprintf(stderr, "Timed out waiting for master IDLE state\n");
				rc = -1;
				phase = FAILED;
			}
			break;

		case WAIT_READY:
			if (state == MO_ECAT_MASTER_STATE_READY) {
				if (mo_ecat_master_activate(master) < 0) {
					fprintf(stderr, "Failed to submit activate command\n");
					rc = -1;
					phase = FAILED;
					break;
				}
				startup_wait_cycles = 0;
				phase = WAIT_RUNNING;
			} else if (++startup_wait_cycles >= startup_timeout_cycles) {
				fprintf(stderr, "Timed out waiting for master READY state\n");
				rc = -1;
				phase = FAILED;
			}
			break;

		case WAIT_RUNNING:
			if (state == MO_ECAT_MASTER_STATE_RUNNING) {
				startup_wait_cycles = 0;
				phase = RUNNING;
			} else if (++startup_wait_cycles >= startup_timeout_cycles) {
				fprintf(stderr, "Timed out waiting for master RUNNING state\n");
				rc = -1;
				phase = FAILED;
			}
			break;

		case RUNNING:
			(void)mo_ecat_master_cycle_begin(master, &result);
			(void)mo_ecat_master_cycle_end(master, &result);
			break;

		case FAILED:
			g_running = 0;
			break;

		default:
			break;
		}

		if (phase == RUNNING && ++print_counter >= 1000) {
			print_counter = 0;

			mo_ecat_master_read_diagnostics(master);

			size_t slave_count = mo_ecat_master_get_slave_count(master);

			printf("State: %d | Slaves: %zu | WKC: %u/%u | DC: %lld\n", state,
			       slave_count, result.actual_wkc, result.expected_wkc,
			       (long long)result.dc_time_ns);

			for (size_t i = 0; i < slave_count; ++i) {
				const struct mo_ecat_slave *slave =
					mo_ecat_master_get_slave(master, i);
				if (slave) {
					printf("  Slave[%zu]: %s, al_state=%d, op=%d\n", i,
					       slave->name, slave->state.al_state,
					       slave->state.operational);
				}
			}
		}

		usleep(1000); /* 1ms 周期 */
	}

	printf("\nStopping...\n");
	mo_ecat_master_destroy(master);
	return rc;
}

int main(int argc, char *argv[])
{
	const char *ifname = (argc > 1) ? argv[1] : "eth0";

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	printf("EtherCAT CLI example (decoupled backend)\n");
	printf("Interface: %s\n", ifname);

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
		.interface_name = ifname,
		.slaves = slaves,
		.slave_count = 1,
	};

	struct mo_ecat_soem_options soem_options = {
		.process_image_capacity = 4096,
	};

	struct mo_ecat_backend backend;
	if (mo_ecat_soem_backend_init(&backend, &soem_options) < 0) {
		fprintf(stderr, "Failed to init SOEM backend\n");
		return -1;
	}

	return run_master_loop(&backend, &config);
}
