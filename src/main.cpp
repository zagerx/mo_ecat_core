#include <iostream>

#include "cyclic/cyclic_runner.h"
#include "ec_controller/ec_controller.h"
#include "utils/signal_handler.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <interface>\n";
		return 1;
	}

	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	// 1. 初始化 EtherCAT：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置 + 进入 SAFE_OP
	mo_ecat::EcatController controller;
	if (!controller.Initialize(config)) {
		return 1;
	}

	auto &master = controller.GetMaster();

	// 2. 启动周期通信线程
	mo_ecat::CyclicRunner cyclic_runner(config.cycle_time_us);
	if (!cyclic_runner.Start([&master]() { master.RunOneCycle(); })) {
		controller.Stop();
		return 1;
	}

	// 3. 进入 OPERATIONAL 状态
	if (!controller.StartOperation()) {
		cyclic_runner.Stop();
		controller.Stop();
		return 1;
	}

	// 4. 注册退出信号回调
	mo_ecat::SignalHandler::Register([&]() {
		cyclic_runner.Stop();
		controller.Stop();
	});

	std::cout << "System running on " << config.ifname
		  << ". Press Ctrl+C to stop.\n";

	// 5. 主线程阻塞等待退出信号
	mo_ecat::SignalHandler::WaitForShutdown();

	// 6. 正常清理（信号路径也会执行清理）
	cyclic_runner.Stop();
	controller.Stop();

	return 0;
}
