#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mo_ecat/ecat_master.h"

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    const char *ifname = (argc > 1) ? argv[1] : "eth0";

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("EtherCAT CLI example\n");
    printf("Interface: %s\n", ifname);

    struct ec_master *master = ec_master_create(ifname);
    if (!master) {
        fprintf(stderr, "Failed to create master\n");
        return -1;
    }

    /* 启动主站：触发扫描与配置 */
    ec_master_start(master);

    int print_counter = 0;
    while (g_running) {
        ec_master_run_cycle(master);

        /* 每秒打印一次状态 */
        if (++print_counter >= 1000) {
            print_counter = 0;
            enum ec_master_state state = ec_master_get_state(master);
            int64_t dc_time = ec_master_get_dc_time(master);
            int slave_count = ec_master_get_slave_count(master);

            printf("State: %d | DC time: %lld | Slaves: %d\n",
                   state, (long long)dc_time, slave_count);

            for (int i = 1; i <= slave_count; ++i) {
                struct ec_slave_info info;
                if (ec_master_get_slave_info(master, (uint16_t)i, &info) == 0) {
                    printf("  Slave[%d]: %s, state=0x%04X\n",
                           i, info.name, info.state);
                }
            }
        }

        usleep(1000); /* 1ms 周期 */
    }

    printf("\nStopping...\n");
    ec_master_stop(master);
    ec_master_destroy(master);

    return 0;
}
