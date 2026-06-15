#include <iostream>
#include <cstring>
#include <string>

#include "soem/soem.h"

// EtherCAT IO 映射缓冲区
static uint8_t IOmap[4096];

// 打印可用网卡
void PrintAvailableAdapters()
{
    ec_adaptert *adapter = nullptr;
    ec_adaptert *head = nullptr;

    std::cout << "Available EtherCAT adapters:\n";
    head = adapter = ec_find_adapters();
    while (adapter != nullptr)
    {
        std::cout << "  - " << adapter->name
                  << "  (" << adapter->desc << ")\n";
        adapter = adapter->next;
    }
    ec_free_adapters(head);
}

// 扫描 EtherCAT 从站
bool ScanSlaves(const std::string& ifname)
{
    ecx_contextt ctx;
    std::memset(&ctx, 0, sizeof(ctx));

    std::cout << "Initializing SOEM on interface: " << ifname << "\n";

    // 1. 初始化网卡
    if (!ecx_init(&ctx, ifname.c_str()))
    {
        std::cerr << "Failed to initialize SOEM on " << ifname
                  << " (try running with sudo)\n";
        return false;
    }

    // 2. 扫描并自动配置从站
    int slave_count = ecx_config_init(&ctx);
    if (slave_count <= 0)
    {
        std::cerr << "No EtherCAT slaves found on " << ifname << "\n";
        ecx_close(&ctx);
        return false;
    }

    std::cout << "Found " << slave_count << " slave(s)\n";

    // 3. 执行 PDO 映射（扫描阶段也建议调用，用于填充 outputs/inputs 指针）
    ecx_config_map_group(&ctx, IOmap, 0);

    // 4. 打印每个从站的基本信息
    for (int i = 1; i <= slave_count; ++i)
    {
        const ec_slavet& slave = ctx.slavelist[i];
        std::cout << "\nSlave " << i << ":\n";
        std::cout << "  Name:        " << slave.name << "\n";
        std::cout << "  Vendor ID:   0x" << std::hex << slave.eep_man << std::dec << "\n";
        std::cout << "  ProductCode: 0x" << std::hex << slave.eep_id << std::dec << "\n";
        std::cout << "  Revision:    0x" << std::hex << slave.eep_rev << std::dec << "\n";
        std::cout << "  Output bits: " << slave.Obits << "\n";
        std::cout << "  Input bits:  " << slave.Ibits << "\n";
        std::cout << "  State:       0x" << std::hex << slave.state << std::dec << "\n";
        std::cout << "  Has DC:      " << (slave.hasdc ? "yes" : "no") << "\n";
    }

    // 5. 关闭 SOEM
    ecx_close(&ctx);
    std::cout << "\nScan complete.\n";

    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "MoDriverPC_EtherCAT - EtherCAT Slave Scanner\n";

    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <interface>\n";
        std::cout << "Example: sudo " << argv[0] << " eth0\n\n";
        PrintAvailableAdapters();
        return 1;
    }

    if (!ScanSlaves(argv[1]))
    {
        return 1;
    }

    return 0;
}