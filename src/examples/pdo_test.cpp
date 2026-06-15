#include <chrono>
#include <iostream>
#include <thread>

#include "cyclic/cyclic_runner.h"
#include "ec_controller/ec_controller.h"

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

	// 1. 初始化
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

	// 给周期线程一点时间稳定
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// 3. 进入 OPERATIONAL
	if (!controller.StartOperation()) {
		cyclic_runner.Stop();
		controller.Stop();
		return 1;
	}

	std::cout << "Running PDO test...\n";

	// 4. 每 500ms 向 OutputCounter (0x7000) 写一个递增的值
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

	// 5. 停止
	cyclic_runner.Stop();
	controller.Stop();

	return 0;
}
