#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "ecat_application.h"
#include "stdin_command_reader.h"
#include "cli_logger.h"
#include "mo_ecat/master.h"

namespace
{

std::atomic<bool> g_running{true};

void OnSignal(int /*signal*/) { g_running.store(false); }

void RegisterShutdownSignals()
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
}

} // namespace

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

	mo_ecat::MoEcatMaster master;
	master.on_log_message = [](const std::string &level, const std::string &source,
				   const std::string &message) {
		if (level == "error" || level == "fatal") {
			LOG_ERROR << "[" << source << "] " << message;
		} else if (level == "warn") {
			LOG_WARN << "[" << source << "] " << message;
		} else if (level == "debug") {
			LOG_DEBUG << "[" << source << "] " << message;
		} else {
			LOG_INFO << "[" << source << "] " << message;
		}
	};

	auto app = std::make_unique<mo_ecat::EcatApplication>(
		std::make_unique<mo_ecat::StdinCommandReader>(), master);

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
