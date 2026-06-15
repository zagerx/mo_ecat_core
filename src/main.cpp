#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "ec_controller/ec_controller.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <interface>\n";
		return 1;
	}

	mo_ecat::EcatController controller;
	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	// 1. 初始化：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置 + 进入 SAFE_OP
	if (!controller.Initialize(config)) {
		return 1;
	}

	auto &master = controller.GetMaster();
	std::atomic<bool> running{true};

	// 2. 创建 EtherCAT 周期通信线程
	std::thread cyclic_thread([&]() {
		auto next_time = std::chrono::steady_clock::now();
		auto interval = std::chrono::microseconds(config.cycle_time_us);

		while (running) {
			master.RunOneCycle();
			next_time += interval;
			std::this_thread::sleep_until(next_time);
		}
	});

	// 3. 创建从站状态监控线程
	std::thread monitor_thread([&]() {
		while (running) {
			master.CheckSlaveStates();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

	// 给周期线程一点时间进入稳定循环
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// 4. 进入 OPERATIONAL 状态
	if (!controller.StartOperation()) {
		running = false;
		cyclic_thread.join();
		monitor_thread.join();
		return 1;
	}

	std::cout << "Running PDO test...\n";

	// 5. 每 500ms 向 OutputCounter (0x7000) 写一个递增的值
	uint32_t output_counter = 0;
	for (int i = 0; i < 20; ++i) {
		output_counter++;

		master.WriteOutput(1, 0, reinterpret_cast<uint8_t *>(&output_counter), 4);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		uint32_t input_counter = 0;
		master.ReadInput(1, 0, reinterpret_cast<uint8_t *>(&input_counter), 4);

		std::cout << "Write OutputCounter: " << output_counter
			  << ", Read InputCounter: " << input_counter
			  << ", Cycles: " << master.GetStats().cycle_count
			  << ", WKC errors: " << master.GetStats().wkc_mismatch_count << "\n";
	}

	// 6. 停止线程
	running = false;
	cyclic_thread.join();
	monitor_thread.join();

	// 7. 关闭 EtherCAT
	controller.Stop();

	return 0;
}
