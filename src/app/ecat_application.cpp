#include "app/ecat_application.h"

#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "activity/pdo_mapping_activity.h"
#include "activity/sdo_diagnostics_activity.h"
#include "activity/sdo_parameter_activity.h"
#include "activity/state_inspection_activity.h"
#include "slave_node/slave_node.h"
#include "slave_node/slave_node_manager.h"
#include "utils/logger.h"

namespace mo_ecat
{

EcatApplication::EcatApplication(std::unique_ptr<CommandReader> command_reader,
					 EcatController &controller,
					 ActivityScheduler &scheduler,
					 ProcessDataEngine &engine)
	: command_reader_(std::move(command_reader)),
	  controller_(controller),
	  scheduler_(scheduler),
	  engine_(engine)
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

	// loglevel 是全局命令，不依赖于具体 ControllerState。
	if (has_command && command.rfind("loglevel", 0) == 0) {
		std::istringstream iss(command);
		std::string token;
		std::vector<std::string> args;
		while (iss >> token) {
			args.push_back(token);
		}
		OnLogLevel(args);
		return true;
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
	case ControllerState::kReadyToRun:
		HandleReadyToRunState(has_command ? &command : nullptr);
		break;
	case ControllerState::kOperational:
		HandleOperationalState(has_command ? &command : nullptr);
		break;
	case ControllerState::kError:
		HandleErrorState(has_command ? &command : nullptr);
		break;
	case ControllerState::kEmergencyStop:
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

// Maintenance 状态：周期性检查从站状态，允许 SDO 维护活动和 prepare/stop。
// 状态检查频率由 ProcessDataEngine 内部控制。
void EcatApplication::HandleMaintenanceState(const std::string *command)
{
	engine_.CheckSlaveStates();

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
	} else if (command->rfind("pdo", 0) == 0) {
		LOG_INFO << "Command: pdo";
		std::istringstream iss(*command);
		std::string token;
		std::vector<std::string> args;
		while (iss >> token) {
			args.push_back(token);
		}
		OnPdo(args);
	} else if (*command == "prepare") {
		LOG_INFO << "Command: prepare";
		if (!controller_.PrepareRun()) {
			LOG_ERROR << "Failed to prepare operation";
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

// ReadyToRun 状态：PDO/DC/SafeOp 已准备，只允许 start / back / stop / help。
void EcatApplication::HandleReadyToRunState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "start") {
		LOG_INFO << "Command: start";
		if (!controller_.StartOperation()) {
			LOG_ERROR << "Failed to start operation";
		}
	} else if (*command == "back") {
		LOG_INFO << "Command: back";
		if (!controller_.BackToMaintenance()) {
			LOG_ERROR << "Failed to back to Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in ReadyToRun state";
	}
}

// Operational 状态：周期性 PDO 收发 + 状态检查，只允许 stop。
// 状态检查频率由 ProcessDataEngine 内部控制。
void EcatApplication::HandleOperationalState(const std::string *command)
{
	engine_.RunOnce();
	engine_.CheckSlaveStates();

	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	}
}

// Fault / EmergencyStop 状态：只能 stop，其他命令拒绝。
void EcatApplication::HandleErrorState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		controller_.Stop();
	} else if (command != nullptr) {
		LOG_ERROR << "In Fault/EmergencyStop state, only 'stop' is allowed";
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

	uint16_t index = 0;
	uint8_t subindex = 0;
	uint32_t value = 0;

	try {
		const unsigned long raw_index = std::stoul(args[1], nullptr, 0);
		const unsigned long raw_subindex = std::stoul(args[2], nullptr, 0);
		const unsigned long raw_value = std::stoul(args[3], nullptr, 0);

		if (raw_index > 0xFFFF) {
			LOG_ERROR << "Index out of range: " << args[1]
				  << " (must be <= 0xFFFF)";
			return;
		}
		if (raw_subindex > 0xFF) {
			LOG_ERROR << "Subindex out of range: " << args[2]
				  << " (must be <= 0xFF)";
			return;
		}
		if (raw_value > 0xFFFFFFFFULL) {
			LOG_ERROR << "Value out of range: " << args[3]
				  << " (must be <= 0xFFFFFFFF)";
			return;
		}

		index = static_cast<uint16_t>(raw_index);
		subindex = static_cast<uint8_t>(raw_subindex);
		value = static_cast<uint32_t>(raw_value);
	} catch (const std::exception &e) {
		LOG_ERROR << "Invalid param argument: " << e.what()
			  << "\nUsage: param <index> <subindex> <value>";
		return;
	}

	LOG_INFO << "Writing parameter 0x" << std::hex << index << ":"
		 << static_cast<int>(subindex) << " = 0x" << value << std::dec;

	controller_.ForEachSlaveNode([this, index, subindex, value](SlaveNode &node) {
		scheduler_.Execute(
			std::make_unique<SdoParameterActivity>(node, index, subindex, value));
	});
}

// 对所有从站执行状态巡检 Activity。
void EcatApplication::OnInspect()
{
	LOG_INFO << "Running state inspection...";
	ExecuteActivityForAllNodes([](SlaveNode &node) {
		return std::make_unique<StateInspectionActivity>(node);
	});
}

// 读取并打印 PDO 映射：pdo [slave_id]，省略 slave_id 则对所有从站执行。
void EcatApplication::OnPdo(const std::vector<std::string> &args)
{
	if (args.size() == 1) {
		LOG_INFO << "Reading PDO mapping for all slaves...";
		ExecuteActivityForAllNodes([](SlaveNode &node) {
			return std::make_unique<PdoMappingActivity>(node);
		});
		return;
	}

	if (args.size() != 2) {
		LOG_ERROR << "Usage: pdo [slave_id]";
		return;
	}

	int target_id = 0;
	try {
		target_id = std::stoi(args[1]);
	} catch (const std::exception &e) {
		LOG_ERROR << "Invalid slave_id: " << args[1]
			  << "\nUsage: pdo [slave_id]";
		return;
	}

	bool found = false;
	controller_.ForEachSlaveNode([this, target_id, &found](SlaveNode &node) {
		if (static_cast<int>(node.GetInfo().slave_id) == target_id) {
			scheduler_.Execute(std::make_unique<PdoMappingActivity>(node));
			found = true;
		}
	});

	if (!found) {
		LOG_ERROR << "Slave " << target_id << " not found";
	}
}

void EcatApplication::OnLogLevel(const std::vector<std::string> &args)
{
	if (args.size() != 2) {
		LOG_ERROR << "Usage: loglevel debug|info|warn|error";
		return;
	}

	LogLevel level;
	if (args[1] == "debug") {
		level = LogLevel::Debug;
	} else if (args[1] == "info") {
		level = LogLevel::Info;
	} else if (args[1] == "warn") {
		level = LogLevel::Warn;
	} else if (args[1] == "error") {
		level = LogLevel::Error;
	} else {
		LOG_ERROR << "Unknown log level: " << args[1]
			  << "\nUsage: loglevel debug|info|warn|error";
		return;
	}

	Logger::GetInstance().SetLogLevel(level);
	LOG_INFO << "Log level set to " << args[1];
}

void EcatApplication::OnHelp()
{
	LOG_INFO << "Available commands by state:\n"
		 << "  [AdapterReady] scan                   - Scan slaves on the bus\n"
		 << "  [Scanned]      config                 - Enter Maintenance\n"
		 << "  [Maintenance]  diagnose               - Run SDO diagnostics\n"
		 << "  [Maintenance]  param <idx> <sub> <val> - Write a parameter\n"
		 << "  [Maintenance]  inspect                - Inspect slave states\n"
		 << "  [Maintenance]  pdo [slave_id]         - Show PDO mapping (all or single slave)\n"
		 << "  [Maintenance]  prepare                - Prepare PDO/DC/SafeOp and enter ReadyToRun\n"
		 << "  [ReadyToRun]   start                  - Enter OPERATIONAL\n"
		 << "  [ReadyToRun]   back                   - Return to Maintenance\n"
		 << "  [Any]          loglevel <level>       - Set log level (debug/info/warn/error)\n"
		 << "  [Any]          stop                   - Stop controller\n"
		 << "  [Any]          exit / quit            - Quit program";
}

// 遍历所有 SlaveNode，为每个节点创建并执行 Activity。
void EcatApplication::ExecuteActivityForAllNodes(
	const std::function<std::unique_ptr<EcatActivity>(SlaveNode &)> &factory)
{
	controller_.ForEachSlaveNode([this, &factory](SlaveNode &node) {
		scheduler_.Execute(factory(node));
	});
}

} // namespace mo_ecat
