#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "activity/activity_scheduler.h"
#include "ecat_application.h"
#include "stdin_command_reader.h"
#include "cyclic/process_data_engine.h"
#include "ec_master/ec_master.h"
#include "utils/logger.h"

namespace
{

// 全局运行标志。SIGINT/SIGTERM 会将其置为 false，通知主循环退出。
std::atomic<bool> g_running{true};

void OnSignal(int /*signal*/)
{
	g_running.store(false);
}

void RegisterShutdownSignals()
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc < 2) {
		LOG_ERROR << "Usage: " << argv[0] << " <interface>";
		return 1;
	}

	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	// 核心对象：
	// Controller 只做状态迁移；
	// Scheduler 负责 Activity 调度；
	// Engine 负责 PDO 周期和状态监控。
	mo_ecat::EcatController controller;
	mo_ecat::ActivityScheduler scheduler(controller);
	mo_ecat::ProcessDataEngine engine(controller, controller.GetEcMaster(),
					 controller.GetSlaveNodeManager());

	auto app = std::make_unique<mo_ecat::EcatApplication>(
		std::make_unique<mo_ecat::StdinCommandReader>(), controller, scheduler,
		engine);
	if (!app->Initialize(config)) {
		LOG_ERROR << "Failed to initialize application";
		return 1;
	}

	RegisterShutdownSignals();

	LOG_INFO << "Application running. Type 'help' for commands, 'exit' to quit.";

	// 周期调用 EcatApplication::Run()，默认 1ms 周期。
	// Run() 返回 false 表示收到 exit/quit/EOF，也应退出循环。
	while (g_running.load()) {
		if (!app->Run()) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	LOG_INFO << "Shutting down...";
	app->Shutdown();

	return 0;
}
