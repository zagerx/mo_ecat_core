#include "app/ecat_application.h"

#include <functional>
#include <iomanip>
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

void EcatApplication::HandleCommand(const std::string &command)
{
	std::istringstream iss(command);
	std::string token;
	std::vector<std::string> args;

	while (iss >> token) {
		args.push_back(token);
	}

	if (args.empty()) {
		return;
	}

	const std::string &cmd = args[0];
	LOG_INFO << "Handling command: " << cmd;

	if (cmd == "scan") {
		OnScan();
	} else if (cmd == "config") {
		OnConfig();
	} else if (cmd == "stop") {
		OnStop();
	} else if (cmd == "diagnose") {
		OnDiagnose();
	} else if (cmd == "param") {
		OnParam(args);
	} else if (cmd == "inspect") {
		OnInspect();
	} else if (cmd == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Unknown command: " << cmd << ", type 'help' for usage";
	}
}

void EcatApplication::OnScan()
{
	if (controller_.GetState() != ControllerState::kInitDone) {
		LOG_ERROR << "scan requires InitDone state, current="
			  << EcatController::StateToString(controller_.GetState());
		return;
	}

	LOG_INFO << "Scanning slaves...";
	if (!controller_.Scan()) {
		LOG_ERROR << "Scan failed";
		return;
	}

	LOG_INFO << "Scan completed, " << controller_.GetSlaveCount()
		 << " slave(s) found";
}

void EcatApplication::OnConfig()
{
	if (controller_.GetState() != ControllerState::kScanned) {
		LOG_ERROR << "config requires Scanned state, current="
			  << EcatController::StateToString(controller_.GetState());
		return;
	}

	LOG_INFO << "Configuring slaves into PREOP...";
	if (!controller_.EnterPreOp()) {
		LOG_ERROR << "Failed to enter PREOP";
		return;
	}

	LOG_INFO << "Slaves in PREOP, ready for maintenance activities";
}

void EcatApplication::OnStop()
{
	if (controller_.GetState() == ControllerState::kUninitialized) {
		LOG_WARN << "Already stopped";
		return;
	}

	LOG_INFO << "Stopping EtherCAT controller...";
	controller_.Stop();
	LOG_INFO << "Controller stopped";
}

void EcatApplication::OnDiagnose()
{
	if (controller_.GetState() != ControllerState::kPreOp) {
		LOG_ERROR << "diagnose requires PreOp state, current="
			  << EcatController::StateToString(controller_.GetState());
		return;
	}

	LOG_INFO << "Running SDO diagnostics...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<SdoDiagnosticsActivity>(node);
	});
}

void EcatApplication::OnParam(const std::vector<std::string> &args)
{
	if (controller_.GetState() != ControllerState::kPreOp) {
		LOG_ERROR << "param requires PreOp state, current="
			  << EcatController::StateToString(controller_.GetState());
		return;
	}

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
	if (controller_.GetState() != ControllerState::kPreOp) {
		LOG_ERROR << "inspect requires PreOp state, current="
			  << EcatController::StateToString(controller_.GetState());
		return;
	}

	LOG_INFO << "Running state inspection...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<StateInspectionActivity>(node);
	});
}

void EcatApplication::OnHelp()
{
	LOG_INFO << "Available commands:\n"
		 << "  scan                    - Scan slaves on the bus\n"
		 << "  config                  - Enter PREOP and create slave nodes\n"
		 << "  diagnose                - Run SDO diagnostics on all slaves\n"
		 << "  param <idx> <sub> <val> - Write a parameter to all slaves\n"
		 << "  inspect                 - Inspect slave states\n"
		 << "  stop                    - Stop the controller\n"
		 << "  help                    - Show this help";
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
