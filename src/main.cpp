#include <iostream>
#include <memory>
#include <string>

#include "app/ecat_application.h"
#include "ec_master/ec_master.h"
#include "utils/logger.h"

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

	auto app = std::make_unique<mo_ecat::EcatApplication>();
	if (!app->Initialize(config)) {
		LOG_ERROR << "Failed to initialize application";
		return 1;
	}

	LOG_INFO << "Ecat application running. Type 'help' for commands, 'exit' to quit.";

	// 最简单模式：从 stdin 读取命令
	std::string command;
	while (true) {
		std::cout << "> " << std::flush;
		if (!std::getline(std::cin, command)) {
			break;  // Ctrl+D 或 EOF
		}

		if (command.empty()) {
			continue;
		}

		if (command == "exit" || command == "quit") {
			break;
		}

		app->HandleCommand(command);
	}

	LOG_INFO << "Shutting down...";
	app->Shutdown();

	return 0;
}
