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

EcatApplication::EcatApplication(std::unique_ptr<CommandReader> command_reader)
	: command_reader_(std::move(command_reader))
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

// 单步执行一次状态机迭代。
// 非阻塞读取命令（timeout=0），根据当前 ControllerState 执行对应逻辑和周期任务。
// 返回 false 表示收到 exit/quit/EOF，上层应退出循环。
bool EcatApplication::Run()
{
	std::string command;
	const auto result = command_reader_->Read(command, 0);

	if (result == ReadResult::kEof) {
		LOG_INFO << "EOF detected, requesting shutdown";
		return false;
	}

	const bool has_command = (result == ReadResult::kOk);

	if (has_command && (command == "exit" || command == "quit")) {
		LOG_INFO << "Exit requested";
		return false;
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

	return true;
}

// AdapterReady 状态：允许 scan / stop / help。
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

// Scanned 状态：允许 config / stop / help。
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

// Maintenance 状态：周期性检查从站状态，允许 SDO 维护活动和 start/stop。
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

// Operational 状态：周期性 PDO 收发 + 状态检查，只允许 stop。
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

// Error 状态：只能 stop，其他命令拒绝。
void EcatApplication::HandleErrorState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (command != nullptr) {
		LOG_ERROR << "In Error state, only 'stop' is allowed";
	}
}

// 对所有从站执行 SDO 诊断 Activity。
void EcatApplication::OnDiagnose()
{
	LOG_INFO << "Running SDO diagnostics...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<SdoDiagnosticsActivity>(node);
	});
}

// 对所有从站执行 SDO 参数写入 Activity。
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

// 对所有从站执行状态巡检 Activity。
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

// 遍历所有 SlaveNode，为每个节点创建并执行 Activity。
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
