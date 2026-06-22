#include "ec_controller/ec_controller.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "utils/logger.h"

namespace
{

std::string MbxProtoToString(uint16_t proto)
{
	std::string s;
	if (proto & 0x01) {
		s += "AoE ";
	}
	if (proto & 0x02) {
		s += "EoE ";
	}
	if (proto & 0x04) {
		s += "CoE ";
	}
	if (proto & 0x08) {
		s += "FoE ";
	}
	if (proto & 0x10) {
		s += "SoE ";
	}
	if (proto & 0x20) {
		s += "VoE ";
	}
	if (s.empty()) {
		return "none";
	}
	s.pop_back();
	return s;
}

std::string CoEDetailsToString(uint8_t details)
{
	std::string s;
	if (details & 0x01) {
		s += "SDO ";
	}
	if (details & 0x02) {
		s += "SDOINFO ";
	}
	if (details & 0x04) {
		s += "PDOASSIGN ";
	}
	if (details & 0x08) {
		s += "PDOCONFIG ";
	}
	if (details & 0x10) {
		s += "UPLOAD ";
	}
	if (details & 0x20) {
		s += "SDOCA ";
	}
	if (s.empty()) {
		return "none";
	}
	s.pop_back();
	return s;
}

std::string FormatSlaveInfo(const mo_ecat::SlaveInfo &info)
{
	std::ostringstream oss;
	oss << "Slave[" << info.slave_id << "] info:\n";

	auto line = [&](const char *key, const std::string &value) {
		oss << "  " << std::left << std::setw(18) << key << " = " << value << "\n";
	};

	auto hex_u32 = [](uint32_t v) {
		std::ostringstream s;
		s << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
		return s.str();
	};
	auto hex_u16 = [](uint16_t v) {
		std::ostringstream s;
		s << "0x" << std::hex << std::setfill('0') << std::setw(4) << v;
		return s.str();
	};
	auto hex_u8 = [](uint8_t v) {
		std::ostringstream s;
		s << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(v);
		return s.str();
	};

	line("name", "\"" + info.name + "\"");
	line("vendor_id", hex_u32(info.vendor_id));
	line("product_id", hex_u32(info.product_id));
	line("revision_id", hex_u32(info.revision_id));
	line("serial_id", hex_u32(info.serial_id));
	line("config_address", hex_u16(info.config_address));
	line("alias_address", hex_u16(info.alias_address));
	line("supports_dc", info.supports_dc ? "yes" : "no");

	line("mbx_l", std::to_string(info.mbx_l));
	line("mbx_wo", hex_u16(info.mbx_wo));
	line("mbx_ro", hex_u16(info.mbx_ro));
	line("mbx_proto", hex_u16(info.mbx_proto) + " (" + MbxProtoToString(info.mbx_proto) + ")");
	line("mbx_cnt", std::to_string(info.mbx_cnt));

	line("state", hex_u16(info.state));
	line("al_status_code", hex_u16(info.al_status_code));
	line("coe_details",
	     hex_u8(info.coe_details) + " (" + CoEDetailsToString(info.coe_details) + ")");
	line("output_bytes", std::to_string(info.output_bytes));
	line("input_bytes", std::to_string(info.input_bytes));

	return oss.str();
}

} // namespace

namespace mo_ecat
{

const std::map<ControllerState, std::vector<ControllerState>> EcatController::kAllowedTransitions =
	{
		{ControllerState::kUninitialized, {ControllerState::kAdapterReady}},
		{ControllerState::kAdapterReady,
		 {ControllerState::kScanned, ControllerState::kUninitialized}},
		{ControllerState::kScanned,
		 {ControllerState::kMaintenance, ControllerState::kUninitialized}},
		{ControllerState::kMaintenance,
		 {ControllerState::kOperational, ControllerState::kUninitialized}},
		{ControllerState::kOperational, {ControllerState::kUninitialized}},
		{ControllerState::kError, {ControllerState::kUninitialized}},
};

EcatController::EcatController()
{
}

EcatController::~EcatController()
{
	Stop();
}

const char *EcatController::StateToString(ControllerState state)
{
	switch (state) {
	case ControllerState::kUninitialized:
		return "Uninitialized";
	case ControllerState::kAdapterReady:
		return "AdapterReady";
	case ControllerState::kScanned:
		return "Scanned";
	case ControllerState::kMaintenance:
		return "Maintenance";
	case ControllerState::kOperational:
		return "Operational";
	case ControllerState::kError:
		return "Error";
	}
	return "Unknown";
}

std::vector<SlaveInfo> EcatController::RefreshSlaveInfos() const
{
	std::vector<SlaveInfo> slave_infos;
	const int slave_count = master_.GetSlaveCount();
	slave_infos.reserve(static_cast<size_t>(slave_count));
	for (int slave_id = 1; slave_id <= slave_count; ++slave_id) {
		slave_infos.push_back(master_.GetSlaveInfo(slave_id));
	}
	return slave_infos;
}

bool EcatController::TransitionTo(ControllerState target)
{
	if (state_ == target) {
		return true;
	}

	// 错误状态只能 Stop（回到 Uninitialized）
	if (state_ == ControllerState::kError) {
		if (target != ControllerState::kUninitialized) {
			LOG_ERROR << "In error state, only Stop() is allowed (target="
				  << StateToString(target) << ")";
			return false;
		}
		return DoStepTo(target);
	}

	// 辅助函数：查转换表
	auto is_allowed = [this](ControllerState from, ControllerState to) -> bool {
		auto it = kAllowedTransitions.find(from);
		if (it == kAllowedTransitions.end()) {
			return false;
		}
		const auto &allowed = it->second;
		return std::find(allowed.begin(), allowed.end(), to) != allowed.end();
	};

	// 目标状态必须被允许
	if (!is_allowed(state_, target)) {
		LOG_ERROR << "Invalid transition: " << StateToString(state_) << " -> "
			  << StateToString(target);
		return false;
	}

	// Maintenance -> Operational 是复合转换，内部顺序执行 PDO/SafeOp/DC/Op
	if (state_ == ControllerState::kMaintenance &&
	    target == ControllerState::kOperational) {
		return DoStartOperation();
	}

	return DoStepTo(target);
}

bool EcatController::ExecuteActivity(std::unique_ptr<EcatActivity> activity)
{
	if (!activity) {
		LOG_ERROR << "Null activity";
		return false;
	}

	if (activity_running_.load()) {
		LOG_ERROR << "Activity already running";
		return false;
	}

	if (!activity->CanStart(state_)) {
		LOG_ERROR << "Activity " << activity->GetName()
			  << " cannot start in state " << StateToString(state_);
		return false;
	}

	activity_running_.store(true);
	LOG_INFO << "Activity started: " << activity->GetName();

	const bool ok = activity->Execute();

	LOG_INFO << "Activity finished: " << activity->GetName()
		 << ", result=" << (ok ? "ok" : "failed");
	activity_running_.store(false);

	if (!ok) {
		switch (activity->GetFailurePolicy()) {
		case ActivityFailurePolicy::kEnterError:
			EnterErrorState(std::string("Activity failed: ") + activity->GetName());
			break;
		case ActivityFailurePolicy::kShutdown:
			Stop();
			break;
		case ActivityFailurePolicy::kKeepControllerState:
		default:
			LOG_WARN << "Activity failed but keeping controller state";
			break;
		}
	}

	return ok;
}

bool EcatController::IsActivityRunning() const
{
	return activity_running_.load();
}

bool EcatController::DoStepTo(ControllerState next)
{
	switch (next) {
	case ControllerState::kAdapterReady:
		return DoInit();
	case ControllerState::kScanned:
		return DoScan();
	case ControllerState::kMaintenance:
		return DoEnterMaintenance();
	case ControllerState::kOperational:
		return DoStartOperation();
	case ControllerState::kUninitialized:
		return DoShutdown();
	default:
		LOG_ERROR << "Unknown target state " << StateToString(next);
		return false;
	}
}

bool EcatController::DoStartOperation()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoStartOperation called in invalid state "
			  << StateToString(state_);
		return false;
	}

	LOG_INFO << "Starting operation from Maintenance...";

	if (!DoPdoConfigure()) {
		return false;
	}
	if (!DoSafeOp()) {
		return false;
	}
	if (!DoDcConfigure()) {
		return false;
	}
	if (!DoOperational()) {
		return false;
	}

	return true;
}

bool EcatController::DoInit()
{
	if (state_ != ControllerState::kUninitialized) {
		LOG_ERROR << "DoInit called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.Initialize(config_)) {
		return false;
	}
	state_ = ControllerState::kAdapterReady;
	LOG_INFO << "EcatController -> AdapterReady";
	return true;
}

bool EcatController::DoScan()
{
	if (state_ != ControllerState::kAdapterReady) {
		LOG_ERROR << "DoScan called in invalid state " << StateToString(state_);
		return false;
	}

	slave_infos_ = master_.ScanSlaves();
	if (slave_infos_.empty()) {
		LOG_ERROR << "No slaves found";
		DoShutdown();
		return false;
	}

	for (const auto &info : slave_infos_) {
		LOG_INFO << FormatSlaveInfo(info);
	}

	state_ = ControllerState::kScanned;
	LOG_INFO << "EcatController -> Scanned, " << slave_infos_.size() << " slave(s)";
	return true;
}

bool EcatController::DoEnterMaintenance()
{
	if (state_ != ControllerState::kScanned) {
		LOG_ERROR << "DoEnterMaintenance called in invalid state "
			  << StateToString(state_);
		return false;
	}

	if (!master_.RequestPreOpState()) {
		LOG_ERROR << "Failed to request PREOP";
		DoShutdown();
		return false;
	}
	if (!master_.CheckAllSlavesInState(EC_STATE_PRE_OP)) {
		LOG_ERROR << "Not all slaves reached PREOP";
		DoShutdown();
		return false;
	}

	state_ = ControllerState::kMaintenance;

	// 刷新从站信息，获取 PREOP 后的 mailbox 等状态
	slave_infos_ = RefreshSlaveInfos();

	// 创建占位 SlaveNode，当前不涉及 PDO 配置
	if (!node_manager_.Initialize(master_, slave_infos_)) {
		DoShutdown();
		return false;
	}

	LOG_INFO << "EcatController -> Maintenance, " << node_manager_.GetNodeCount()
		 << " slave node(s) placeholder created";
	return true;
}

bool EcatController::DoPdoConfigure()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoPdoConfigure called in invalid state " << StateToString(state_);
		return false;
	}

	if (!node_manager_.ConfigureAll()) {
		EnterErrorState("Failed to configure slave nodes");
		return false;
	}

	LOG_INFO << "PDO configured";
	return true;
}

bool EcatController::DoSafeOp()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoSafeOp called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.ConfigureProcessData()) {
		EnterErrorState("Failed to configure process data");
		return false;
	}

	if (!master_.RequestSafeOpState() || !master_.CheckAllSlavesInState(EC_STATE_SAFE_OP)) {
		EnterErrorState("Failed to enter SAFEOP");
		return false;
	}

	LOG_INFO << "SAFEOP reached";
	return true;
}

bool EcatController::DoDcConfigure()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoDcConfigure called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.ConfigureDc()) {
		EnterErrorState("Failed to configure DC");
		return false;
	}

	LOG_INFO << "DC configured";
	return true;
}

bool EcatController::DoOperational()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoOperational called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.RequestOperationalState() ||
	    !master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
		EnterErrorState("Failed to enter OPERATIONAL");
		return false;
	}

	state_ = ControllerState::kOperational;
	LOG_INFO << "EcatController -> Operational";
	return true;
}

bool EcatController::DoShutdown()
{
	LOG_INFO << "EcatController shutting down from state " << StateToString(state_);

	if (state_ == ControllerState::kOperational) {
		if (!master_.RequestSafeOpState()) {
			LOG_WARN << "Failed to request SAFE_OP during shutdown";
		}
	}

	if (state_ == ControllerState::kOperational ||
	    state_ == ControllerState::kMaintenance) {
		if (!master_.RequestPreOpState()) {
			LOG_WARN << "Failed to request PRE_OP during shutdown";
		}
	}

	if (state_ != ControllerState::kUninitialized &&
	    state_ != ControllerState::kAdapterReady && state_ != ControllerState::kScanned) {
		if (!master_.RequestInitState()) {
			LOG_WARN << "Failed to request INIT during shutdown";
		}
	}

	node_manager_.Clear();
	master_.Close();
	state_ = ControllerState::kUninitialized;
	return true;
}

void EcatController::EnterErrorState(const std::string &reason)
{
	LOG_ERROR << "Entering error state: " << reason;
	state_ = ControllerState::kError;
}

bool EcatController::Initialize(const EcMasterConfig &config)
{
	if (state_ != ControllerState::kUninitialized) {
		LOG_WARN << "Already initialized, state=" << StateToString(state_);
		return false;
	}

	if (!InitializeAdapter(config)) {
		return false;
	}
	if (!Scan()) {
		return false;
	}
	return EnterMaintenance();
}

bool EcatController::InitializeAdapter(const EcMasterConfig &config)
{
	if (state_ != ControllerState::kUninitialized) {
		LOG_WARN << "Already initialized, state=" << StateToString(state_);
		return false;
	}

	config_ = config;
	return TransitionTo(ControllerState::kAdapterReady);
}

bool EcatController::Scan()
{
	if (state_ != ControllerState::kAdapterReady) {
		LOG_WARN << "Scan called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kScanned);
}

bool EcatController::EnterMaintenance()
{
	if (state_ != ControllerState::kScanned) {
		LOG_WARN << "EnterMaintenance called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kMaintenance);
}

bool EcatController::StartOperation()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_WARN << "StartOperation called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kOperational);
}

void EcatController::Stop()
{
	if (state_ == ControllerState::kUninitialized) {
		return;
	}

	TransitionTo(ControllerState::kUninitialized);
}

void EcatController::RunOneCycle()
{
	if (state_ != ControllerState::kOperational) {
		LOG_WARN << "RunOneCycle ignored: not operational";
		return;
	}

	node_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	node_manager_.UpdateAllInputs();
}

void EcatController::CheckSlaveStates()
{
	master_.CheckSlaveStates();

	switch (state_) {
	case ControllerState::kMaintenance:
		if (!master_.CheckAllSlavesInState(EC_STATE_PRE_OP)) {
			EnterErrorState("Slave dropped out of PREOP");
		}
		break;
	case ControllerState::kOperational:
		if (!master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
			EnterErrorState("Slave dropped out of OPERATIONAL");
		}
		break;
	default:
		break;
	}
}

SlaveNodeManager &EcatController::GetSlaveNodeManager()
{
	return node_manager_;
}

size_t EcatController::GetSlaveCount() const
{
	return slave_infos_.size();
}

ControllerState EcatController::GetState() const
{
	return state_;
}

bool EcatController::IsInitialized() const
{
	return state_ != ControllerState::kUninitialized;
}

bool EcatController::IsOperational() const
{
	return state_ == ControllerState::kOperational;
}

} // namespace mo_ecat
