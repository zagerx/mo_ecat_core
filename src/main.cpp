#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "app/ecat_application.h"
#include "app/stdin_command_reader.h"
#include "ec_master/ec_master.h"
#include "utils/logger.h"

namespace
{

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

	auto app = std::make_unique<mo_ecat::EcatApplication>(
		std::make_unique<mo_ecat::StdinCommandReader>());
	if (!app->Initialize(config)) {
		LOG_ERROR << "Failed to initialize application";
		return 1;
	}

	RegisterShutdownSignals();

	LOG_INFO << "Application running. Type 'help' for commands, 'exit' to quit.";

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
