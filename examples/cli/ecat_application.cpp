#include "ecat_application.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "cli_logger.h"

namespace mo_ecat
{

namespace
{

const char *StateToString(MasterState state)
{
	switch (state) {
	case MasterState::kUninitialized:
		return "Uninitialized";
	case MasterState::kAdapterReady:
		return "AdapterReady";
	case MasterState::kScanned:
		return "Scanned";
	case MasterState::kMaintenance:
		return "Maintenance";
	case MasterState::kReadyToRun:
		return "ReadyToRun";
	case MasterState::kOperational:
		return "Operational";
	case MasterState::kFault:
		return "Fault";
	case MasterState::kEmergencyStop:
		return "EmergencyStop";
	}
	return "Unknown";
}

} // namespace

EcatApplication::EcatApplication(std::unique_ptr<CommandReader> command_reader,
				 MoEcatMaster &master)
	: command_reader_(std::move(command_reader)), master_(master)
{
}

EcatApplication::~EcatApplication() = default;

bool EcatApplication::Initialize(const EcMasterConfig &config)
{
	return master_.InitializeAdapter(config);
}

void EcatApplication::Shutdown()
{
	master_.Stop();
}

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

	// 全局命令：loglevel
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

	// 先执行一次主站周期服务
	master_.Service();

	switch (master_.GetState()) {
	case MasterState::kAdapterReady:
		HandleAdapterReadyState(has_command ? &command : nullptr);
		break;
	case MasterState::kScanned:
		HandleScannedState(has_command ? &command : nullptr);
		break;
	case MasterState::kMaintenance:
		HandleMaintenanceState(has_command ? &command : nullptr);
		break;
	case MasterState::kReadyToRun:
		HandleReadyToRunState(has_command ? &command : nullptr);
		break;
	case MasterState::kOperational:
		HandleOperationalState(has_command ? &command : nullptr);
		break;
	case MasterState::kFault:
	case MasterState::kEmergencyStop:
		HandleErrorState(has_command ? &command : nullptr);
		break;
	default:
		break;
	}

	return true;
}

void EcatApplication::HandleAdapterReadyState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "scan") {
		LOG_INFO << "Command: scan";
		if (!master_.Scan()) {
			LOG_ERROR << "Scan failed";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
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
		if (!master_.EnterMaintenance()) {
			LOG_ERROR << "Failed to enter Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in Scanned state";
	}
}

void EcatApplication::HandleMaintenanceState(const std::string *command)
{
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
		std::istringstream iss(*command);
		std::string token;
		std::vector<std::string> args;
		while (iss >> token) {
			args.push_back(token);
		}
		OnPdo(args);
	} else if (*command == "prepare") {
		LOG_INFO << "Command: prepare";
		if (!master_.PrepareRun()) {
			LOG_ERROR << "Failed to prepare operation";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Unknown command: " << *command;
	}
}

void EcatApplication::HandleReadyToRunState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "start") {
		LOG_INFO << "Command: start";
		if (!master_.StartOperation()) {
			LOG_ERROR << "Failed to start operation";
		}
	} else if (*command == "back") {
		LOG_INFO << "Command: back";
		if (!master_.BackToMaintenance()) {
			LOG_ERROR << "Failed to back to Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in ReadyToRun state";
	}
}

void EcatApplication::HandleOperationalState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	}
}

void EcatApplication::HandleErrorState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (command != nullptr) {
		LOG_ERROR << "In Fault/EmergencyStop state, only 'stop' is allowed";
	}
}

void EcatApplication::OnDiagnose()
{
	const std::size_t count = master_.GetSlaveCount();
	if (count == 0) {
		LOG_INFO << "No slave available for diagnosis";
		return;
	}

	LOG_INFO << "Running SDO diagnostics on " << count << " slave(s)...";
	for (std::size_t i = 1; i <= count; ++i) {
		// 默认读取 CiA 402 状态字 0x6041:0，2 bytes
		std::vector<uint8_t> data(2);
		if (master_.ReadSdo(static_cast<int>(i), 0x6041, 0x00, data, 1000)) {
			const uint16_t value = static_cast<uint16_t>(data[0]) |
					       (static_cast<uint16_t>(data[1]) << 8);
			std::ostringstream oss;
			oss << "Slave " << i << " statusword 0x6041:0 = 0x"
			    << std::hex << value << std::dec;
			LOG_INFO << oss.str();
		} else {
			LOG_ERROR << "Slave " << i << " failed to read 0x6041:0";
		}
	}
}

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

		index = static_cast<uint16_t>(raw_index);
		subindex = static_cast<uint8_t>(raw_subindex);
		value = static_cast<uint32_t>(raw_value);
	} catch (const std::exception &e) {
		LOG_ERROR << "Invalid param argument: " << e.what()
			  << "\nUsage: param <index> <subindex> <value>";
		return;
	}

	std::ostringstream header;
	header << "Writing parameter 0x" << std::hex << index << ":"
	       << static_cast<int>(subindex) << " = 0x" << value << std::dec
	       << " to all slaves";
	LOG_INFO << header.str();

	const std::vector<uint8_t> data = {
		static_cast<uint8_t>(value & 0xFF),
		static_cast<uint8_t>((value >> 8) & 0xFF),
		static_cast<uint8_t>((value >> 16) & 0xFF),
		static_cast<uint8_t>((value >> 24) & 0xFF),
	};

	const std::size_t count = master_.GetSlaveCount();
	for (std::size_t i = 1; i <= count; ++i) {
		if (master_.WriteSdo(static_cast<int>(i), index, subindex, data, 1000)) {
			LOG_INFO << "Slave " << i << " parameter write OK";
		} else {
			LOG_ERROR << "Slave " << i << " parameter write failed";
		}
	}
}

void EcatApplication::OnInspect()
{
	const MasterSnapshot snapshot = master_.GetSnapshot();
	const auto &slaves = snapshot.slaves;

	std::ostringstream oss;
	oss << "Master state: " << StateToString(snapshot.state) << "\n"
	    << "Slave count: " << slaves.size() << "\n";

	for (const auto &slave : slaves) {
		oss << "  Slave " << slave.slave_id << ": " << slave.name
		    << " (0x" << std::hex << slave.vendor_id << ", 0x"
		    << slave.product_id << ", 0x" << slave.revision_id
		    << std::dec << ")\n"
		    << "    config_address=0x" << std::hex << slave.config_address
		    << ", alias_address=" << std::dec << slave.alias_address
		    << ", supports_dc=" << (slave.supports_dc ? "yes" : "no")
		    << ", ec_state=" << slave.state
		    << ", al_status_code=" << slave.al_status_code
		    << ", output_bytes=" << slave.output_bytes
		    << ", input_bytes=" << slave.input_bytes << "\n";
	}

	LOG_INFO << "Inspection result:\n" << oss.str();
}

void EcatApplication::OnPdo(const std::vector<std::string> &args)
{
	if (args.size() == 1) {
		LOG_INFO << "Reading PDO mapping for all slaves...";
		const std::size_t count = master_.GetSlaveCount();
		for (std::size_t i = 1; i <= count; ++i) {
			const std::string mapping = master_.DumpPdoMapping(static_cast<int>(i));
			LOG_INFO << "Slave " << i << " PDO mapping:\n" << mapping;
		}
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

	const std::string mapping = master_.DumpPdoMapping(target_id);
	if (mapping.empty()) {
		LOG_ERROR << "Slave " << target_id << " not available";
		return;
	}
	LOG_INFO << "Slave " << target_id << " PDO mapping:\n" << mapping;
}

void EcatApplication::OnLogLevel(const std::vector<std::string> &args)
{
	if (args.size() != 2) {
		LOG_ERROR << "Usage: loglevel debug|info|warn|error";
		return;
	}

	CliLogLevel level;
	if (args[1] == "debug") {
		level = CliLogLevel::kDebug;
	} else if (args[1] == "info") {
		level = CliLogLevel::kInfo;
	} else if (args[1] == "warn") {
		level = CliLogLevel::kWarn;
	} else if (args[1] == "error") {
		level = CliLogLevel::kError;
	} else {
		LOG_ERROR << "Unknown log level: " << args[1]
			  << "\nUsage: loglevel debug|info|warn|error";
		return;
	}

	SetCliLogLevel(level);
	LOG_INFO << "Log level set to " << args[1];
}

void EcatApplication::OnHelp()
{
	LOG_INFO << "Available commands by state:\n"
		 << "  [AdapterReady] scan                    - Scan slaves on the bus\n"
		 << "  [Scanned]      config                  - Enter Maintenance\n"
		 << "  [Maintenance]  diagnose                - Read statusword from all slaves\n"
		 << "  [Maintenance]  param <idx> <sub> <val> - Write a 32-bit parameter\n"
		 << "  [Maintenance]  inspect                 - Print master/slave snapshot\n"
		 << "  [Maintenance]  pdo [slave_id]          - Show PDO mapping (all or single slave)\n"
		 << "  [Maintenance]  prepare                 - Prepare PDO/DC/SafeOp and enter ReadyToRun\n"
		 << "  [ReadyToRun]   start                   - Enter OPERATIONAL\n"
		 << "  [ReadyToRun]   back                    - Return to Maintenance\n"
		 << "  [Any]          loglevel <level>        - Set log level (debug/info/warn/error)\n"
		 << "  [Any]          stop                    - Stop controller\n"
		 << "  [Any]          exit / quit             - Quit program";
}

} // namespace mo_ecat
