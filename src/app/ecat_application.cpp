#include "app/ecat_application.h"

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "activity/sdo_diagnostics_activity.h"
#include "activity/sdo_parameter_activity.h"
#include "activity/state_inspection_activity.h"
#include "slave_node/slave_node.h"
#include "slave_node/slave_node_manager.h"
#include "utils/logger.h"

namespace mo_ecat
{

EcatApplication::EcatApplication()
{
}

EcatApplication::~EcatApplication()
{
	Shutdown();
}

bool EcatApplication::Initialize(const EcMasterConfig &config)
{
	if (!controller_.InitializeAdapter(config)) {
		LOG_ERROR << "Failed to initialize EtherCAT adapter";
		return false;
	}

	LOG_INFO << "EcatApplication initialized, controller state="
		 << EcatController::StateToString(controller_.GetState());
	return true;
}

void EcatApplication::Shutdown()
{
	controller_.Stop();
}

void EcatApplication::RequestShutdown()
{
	shutdown_requested_.store(true);
}

bool EcatApplication::ReadCommand(std::string &command, int timeout_ms)
{
	struct pollfd pfd {};
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, timeout_ms);
	if (ret < 0) {
		if (errno == EINTR) {
			return false;
		}
		LOG_ERROR << "poll stdin failed: " << std::strerror(errno);
		return false;
	}

	if (ret == 0) {
		return false;
	}

	if (!std::getline(std::cin, command)) {
		// EOF / Ctrl+D
		RequestShutdown();
		return false;
	}

	return true;
}

void EcatApplication::Run()
{
	LOG_INFO << "Main state machine started. Type 'help' for commands, 'exit' to quit.";

	while (!shutdown_requested_.load()) {
		std::string command;
		const bool has_command = ReadCommand(command, 100);

		if (has_command && (command == "exit" || command == "quit")) {
			RequestShutdown();
			break;
		}

		switch (controller_.GetState()) {
		case ControllerState::kAdapterReady:
			HandleAdapterReadyState(has_command ? &command : nullptr);
			break;
		case ControllerState::kScanned:
			HandleScannedState(has_command ? &command : nullptr);
			break;
		case ControllerState::kMaintenance:
			HandleMaintenanceState(has_command ? &command : nullptr);
			break;
		case ControllerState::kOperational:
			HandleOperationalState(has_command ? &command : nullptr);
			break;
		case ControllerState::kError:
			HandleErrorState(has_command ? &command : nullptr);
			break;
		default:
			break;
		}
	}

	LOG_INFO << "Main state machine stopped.";
}

void EcatApplication::HandleAdapterReadyState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "scan") {
		LOG_INFO << "Command: scan";
		if (!controller_.Scan()) {
			LOG_ERROR << "Scan failed";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in AdapterReady state";
	}
}

void EcatApplication::HandleScannedState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "config") {
		LOG_INFO << "Command: config";
		if (!controller_.EnterMaintenance()) {
			LOG_ERROR << "Failed to enter Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in Scanned state";
	}
}

void EcatApplication::HandleMaintenanceState(const std::string *command)
{
	// 周期性任务：检查从站状态
	controller_.CheckSlaveStates();

	if (command == nullptr) {
		return;
	}

	if (*command == "diagnose") {
		LOG_INFO << "Command: diagnose";
		OnDiagnose();
	} else if (command->rfind("param", 0) == 0) {
		std::istringstream iss(*command);
		std::string token;
		std::vector<std::string> args;
		while (iss >> token) {
			args.push_back(token);
		}
		OnParam(args);
	} else if (*command == "inspect") {
		LOG_INFO << "Command: inspect";
		OnInspect();
	} else if (*command == "start") {
		LOG_INFO << "Command: start";
		if (!controller_.StartOperation()) {
			LOG_ERROR << "Failed to start operation";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Unknown command: " << *command;
	}
}

void EcatApplication::HandleOperationalState(const std::string *command)
{
	// 周期性 PDO 周期 + 状态检查
	controller_.RunOneCycle();
	controller_.CheckSlaveStates();

	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	}
}

void EcatApplication::HandleErrorState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (command != nullptr) {
		LOG_ERROR << "In Error state, only 'stop' is allowed";
	}
}

void EcatApplication::OnDiagnose()
{
	LOG_INFO << "Running SDO diagnostics...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<SdoDiagnosticsActivity>(node);
	});
}

void EcatApplication::OnParam(const std::vector<std::string> &args)
{
	if (args.size() != 4) {
		LOG_ERROR << "Usage: param <index> <subindex> <value>";
		return;
	}

	const uint16_t index = static_cast<uint16_t>(std::stoul(args[1], nullptr, 0));
	const uint8_t subindex = static_cast<uint8_t>(std::stoul(args[2], nullptr, 0));
	const uint32_t value = static_cast<uint32_t>(std::stoul(args[3], nullptr, 0));

	LOG_INFO << "Writing parameter 0x" << std::hex << index << ":"
		 << static_cast<int>(subindex) << " = 0x" << value << std::dec;

	auto &node_manager = controller_.GetSlaveNodeManager();
	for (size_t i = 0; i < node_manager.GetNodeCount(); ++i) {
		SlaveNode *node = node_manager.GetNode(i);
		if (node != nullptr) {
			controller_.ExecuteActivity(
				std::make_unique<SdoParameterActivity>(*node, index, subindex,
								       value));
		}
	}
}

void EcatApplication::OnInspect()
{
	LOG_INFO << "Running state inspection...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<StateInspectionActivity>(node);
	});
}

void EcatApplication::OnHelp()
{
	LOG_INFO << "Available commands by state:\n"
		 << "  [AdapterReady] scan                   - Scan slaves on the bus\n"
		 << "  [Scanned]      config                 - Enter Maintenance\n"
		 << "  [Maintenance]  diagnose               - Run SDO diagnostics\n"
		 << "  [Maintenance]  param <idx> <sub> <val> - Write a parameter\n"
		 << "  [Maintenance]  inspect                - Inspect slave states\n"
		 << "  [Maintenance]  start                  - Enter OPERATIONAL\n"
		 << "  [Any]          stop                   - Stop controller\n"
		 << "  [Any]          exit / quit            - Quit program";
}

void EcatApplication::ExecuteActivityForAllNodes(
	const std::function<std::unique_ptr<EcatActivity>(SlaveNode &)> &factory)
{
	auto &node_manager = controller_.GetSlaveNodeManager();
	for (size_t i = 0; i < node_manager.GetNodeCount(); ++i) {
		SlaveNode *node = node_manager.GetNode(i);
		if (node != nullptr) {
			controller_.ExecuteActivity(factory(*node));
		}
	}
}

} // namespace mo_ecat
