#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "ec_master/ec_master.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <interface>\n";
		return 1;
	}

	mo_ecat::EcMaster master;
	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	// 1. 初始化
	if (!master.Initialize(config)) {
		return 1;
	}

	// 2. 扫描并配置
	if (!master.ScanAndConfigure()) {
		return 1;
	}

	// 3. 进入 SAFE_OP（此时可以开始发过程数据）
	if (!master.RequestSafeOpState()) {
		std::cerr << "Failed to enter SAFE_OP\n";
		return 1;
	}

	// 4. 启动周期通信线程
	master.StartCyclicThread();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// 5. 进入 OPERATIONAL
	if (!master.RequestOperationalState()) {
		std::cerr << "Failed to enter OPERATIONAL\n";
		return 1;
	}

	std::cout << "Entered OPERATIONAL, running PDO test...\n";

	// 6. 每 500ms 向 OutputCounter (0x7000) 写一个递增的值
	uint32_t output_counter = 0;
	for (int i = 0; i < 20; ++i) {
		output_counter++;

		// 写入 SM2 outputs，偏移 0 字节
		master.WriteOutput(1, 0, reinterpret_cast<uint8_t *>(&output_counter), 4);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// 读取 SM3 inputs，偏移 0 字节
		uint32_t input_counter = 0;
		master.ReadInput(1, 0, reinterpret_cast<uint8_t *>(&input_counter), 4);

		std::cout << "Write OutputCounter: " << output_counter
			  << ", Read InputCounter: " << input_counter
			  << ", Cycles: " << master.GetStats().cycle_count
			  << ", WKC errors: " << master.GetStats().wkc_mismatch_count << "\n";
	}

	// 7. 停止
	master.StopCyclicThread();
	master.RequestSafeOpState();
	master.RequestInitState();

	return 0;
}