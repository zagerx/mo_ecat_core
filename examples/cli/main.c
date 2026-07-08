#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/soem_backend.h"
#include "mo_ecat/mock_backend.h"

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static int run_with_backend(struct mo_ecat_backend *backend,
                            const struct mo_ecat_config *config)
{
    struct mo_ecat_master *master = mo_ecat_master_create();
    if (!master) {
        fprintf(stderr, "Failed to create master\n");
        return -1;
    }

    if (mo_ecat_master_configure(master, config, backend) < 0) {
        fprintf(stderr, "Failed to configure master\n");
        mo_ecat_master_destroy(master);
        return -1;
    }

    if (mo_ecat_master_activate(master) < 0) {
        fprintf(stderr, "Failed to activate master\n");
        mo_ecat_master_destroy(master);
        return -1;
    }

    struct mo_ecat_cycle_result result;
    int print_counter = 0;

    while (g_running) {
        (void)mo_ecat_master_cycle_begin(master, &result);
        (void)mo_ecat_master_cycle_end(master, &result);

        if (++print_counter >= 1000) {
            print_counter = 0;

            mo_ecat_master_read_diagnostics(master);

            enum mo_ecat_master_state state = mo_ecat_master_get_state(master);
            size_t slave_count = mo_ecat_master_get_slave_count(master);

            printf("State: %d | Slaves: %zu | WKC: %u/%u | DC: %lld\n",
                   state, slave_count,
                   result.actual_wkc, result.expected_wkc,
                   (long long)result.dc_time_ns);

            for (size_t i = 0; i < slave_count; ++i) {
                const struct mo_ecat_slave *slave =
                    mo_ecat_master_get_slave(master, i);
                if (slave) {
                    printf("  Slave[%zu]: %s, al_state=%d, op=%d\n",
                           i, slave->name,
                           slave->state.al_state,
                           slave->state.operational);
                }
            }
        }

        usleep(1000); /* 1ms 周期 */
    }

    printf("\nStopping...\n");
    mo_ecat_master_destroy(master);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *ifname = (argc > 1) ? argv[1] : "eth0";
    int use_mock = (strcmp(ifname, "mock") == 0);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("EtherCAT CLI example (decoupled backend)\n");
    printf("Interface: %s\n", ifname);

    if (use_mock) {
        struct mo_ecat_pdo_entry_config slave0_pdos[] = {
            { 0x7000, 0x01, 16, MO_ECAT_PDO_OUTPUT },
            { 0x6000, 0x01, 16, MO_ECAT_PDO_INPUT  },
        };
        struct mo_ecat_pdo_entry_config slave1_pdos[] = {
            { 0x7010, 0x01, 8, MO_ECAT_PDO_OUTPUT },
            { 0x6010, 0x01, 8, MO_ECAT_PDO_INPUT  },
        };

        struct mo_ecat_slave_config slaves[] = {
            {
                .alias = 0, .position = 0,
                .vendor_id = 0, .product_code = 0, .revision_number = 0,
                .pdo_entries = slave0_pdos, .pdo_entry_count = 2,
                .dc_active = 0,
            },
            {
                .alias = 0, .position = 1,
                .vendor_id = 0, .product_code = 0, .revision_number = 0,
                .pdo_entries = slave1_pdos, .pdo_entry_count = 2,
                .dc_active = 0,
            },
        };

        struct mo_ecat_config config = {
            .interface_name = "mock0",
            .slaves = slaves,
            .slave_count = 2,
        };

        struct mo_ecat_backend backend;
        if (mo_ecat_mock_backend_init(&backend) < 0) {
            fprintf(stderr, "Failed to init mock backend\n");
            return -1;
        }

        return run_with_backend(&backend, &config);
    }

    struct mo_ecat_pdo_entry_config slave0_pdos[] = {
        { 0x7000, 0x01, 16, MO_ECAT_PDO_OUTPUT },
        { 0x6000, 0x01, 16, MO_ECAT_PDO_INPUT  },
    };

    struct mo_ecat_slave_config slaves[] = {
        {
            .alias = 0, .position = 0,
            .vendor_id = 0, .product_code = 0, .revision_number = 0,
            .pdo_entries = slave0_pdos, .pdo_entry_count = 2,
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

    return run_with_backend(&backend, &config);
}
