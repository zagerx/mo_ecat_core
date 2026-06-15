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

	// 1. 初始化：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置
	if (!controller.Initialize(config)) {
		return 1;
	}

	// 2. 启动：进入 SAFE_OP → 启动周期线程 → 进入 OPERATIONAL
	if (!controller.Start()) {
		return 1;
	}

	std::cout << "Running PDO test...\n";

	// 3. 每 500ms 向 OutputCounter (0x7000) 写一个递增的值
	uint32_t output_counter = 0;
	for (int i = 0; i < 20; ++i) {
		output_counter++;

		auto &master = controller.GetMaster();
		master.WriteOutput(1, 0, reinterpret_cast<uint8_t *>(&output_counter), 4);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		uint32_t input_counter = 0;
		master.ReadInput(1, 0, reinterpret_cast<uint8_t *>(&input_counter), 4);

		std::cout << "Write OutputCounter: " << output_counter
			  << ", Read InputCounter: " << input_counter
			  << ", Cycles: " << master.GetStats().cycle_count
			  << ", WKC errors: " << master.GetStats().wkc_mismatch_count << "\n";
	}

	// 4. 停止
	controller.Stop();

	return 0;
}
