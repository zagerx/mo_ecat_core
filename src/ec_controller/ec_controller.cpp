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
		 {ControllerState::kReadyToRun, ControllerState::kUninitialized}},
		{ControllerState::kReadyToRun,
		 {ControllerState::kOperational, ControllerState::kMaintenance,
		  ControllerState::kUninitialized}},
		{ControllerState::kOperational,
		 {ControllerState::kMaintenance, ControllerState::kUninitialized}},
		{ControllerState::kFault, {ControllerState::kUninitialized}},
		{ControllerState::kEmergencyStop, {ControllerState::kUninitialized}},
};

const std::map<ControllerState, std::vector<MasterAction>> EcatController::kAllowedActions =
	{
		{ControllerState::kUninitialized, {MasterAction::kInitializeAdapter}},
		{ControllerState::kAdapterReady,
		 {MasterAction::kScanSlaves, MasterAction::kStop}},
		{ControllerState::kScanned,
		 {MasterAction::kEnterMaintenance, MasterAction::kStop}},
		{ControllerState::kMaintenance,
		 {MasterAction::kPrepareRun, MasterAction::kStop}},
		{ControllerState::kReadyToRun,
		 {MasterAction::kStartOperation, MasterAction::kBackToMaintenance,
		  MasterAction::kStop}},
		{ControllerState::kOperational,
		 {MasterAction::kBackToMaintenance, MasterAction::kStop}},
		{ControllerState::kFault, {MasterAction::kStop}},
		{ControllerState::kEmergencyStop, {MasterAction::kStop}},
};

EcatController::EcatController()
{
}

EcatController::~EcatController()
{
	Stop();
}

// 将 ControllerState 转换为可读字符串。
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
	case ControllerState::kReadyToRun:
		return "ReadyToRun";
	case ControllerState::kOperational:
		return "Operational";
	case ControllerState::kFault:
		return "Fault";
	case ControllerState::kEmergencyStop:
		return "EmergencyStop";
	}
	return "Unknown";
}

// 从 EcMaster 刷新所有从站信息。
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

// 兼容旧目标状态入口。
// 1. 相同状态直接返回 true
// 2. 目标状态映射为 MasterAction
// 3. 由 Dispatch() 统一校验和执行
bool EcatController::TransitionTo(ControllerState target)
{
	if (state_ == target) {
		return true;
	}

	MasterAction action;
	switch (target) {
	case ControllerState::kAdapterReady:
		action = MasterAction::kInitializeAdapter;
		break;
	case ControllerState::kScanned:
		action = MasterAction::kScanSlaves;
		break;
	case ControllerState::kMaintenance:
		action = MasterAction::kBackToMaintenance;
		if (state_ == ControllerState::kScanned) {
			action = MasterAction::kEnterMaintenance;
		}
		break;
	case ControllerState::kReadyToRun:
		action = MasterAction::kPrepareRun;
		break;
	case ControllerState::kOperational:
		action = MasterAction::kStartOperation;
		break;
	case ControllerState::kUninitialized:
		action = MasterAction::kStop;
		break;
	case ControllerState::kFault:
		action = MasterAction::kRequestFault;
		break;
	case ControllerState::kEmergencyStop:
		action = MasterAction::kRequestEmergencyStop;
		break;
	default:
		LOG_ERROR << "Unknown target state " << StateToString(target);
		return false;
	}

	return Dispatch(action);
}

bool EcatController::Dispatch(MasterAction action, const std::string &reason)
{
	if (action == MasterAction::kRequestEmergencyStop) {
		EnterEmergencyStop(reason.empty() ? "Emergency stop requested" : reason);
		return true;
	}

	if (action == MasterAction::kRequestFault) {
		EnterFault(reason.empty() ? "Fault requested" : reason);
		return true;
	}

	auto is_allowed = [this](ControllerState state, MasterAction candidate) -> bool {
		auto it = kAllowedActions.find(state);
		if (it == kAllowedActions.end()) {
			return false;
		}
		const auto &allowed = it->second;
		return std::find(allowed.begin(), allowed.end(), candidate) != allowed.end();
	};

	if (!is_allowed(state_, action)) {
		LOG_ERROR << "Action is not allowed in state " << StateToString(state_);
		return false;
	}

	return DoAction(action);
}

// 执行单个公开状态转换步骤。
bool EcatController::DoStepTo(ControllerState next)
{
	switch (next) {
	case ControllerState::kAdapterReady:
		return DoInit();
	case ControllerState::kScanned:
		return DoScan();
	case ControllerState::kMaintenance:
		return DoEnterMaintenance();
	case ControllerState::kReadyToRun:
		return DoPrepareRun();
	case ControllerState::kOperational:
		return DoStartOperation();
	case ControllerState::kUninitialized:
		return DoShutdown();
	default:
		LOG_ERROR << "Unknown target state " << StateToString(next);
		return false;
	}
}

bool EcatController::DoAction(MasterAction action)
{
	switch (action) {
	case MasterAction::kInitializeAdapter:
		return DoStepTo(ControllerState::kAdapterReady);
	case MasterAction::kScanSlaves:
		return DoStepTo(ControllerState::kScanned);
	case MasterAction::kEnterMaintenance:
		return DoStepTo(ControllerState::kMaintenance);
	case MasterAction::kPrepareRun:
		return DoStepTo(ControllerState::kReadyToRun);
	case MasterAction::kStartOperation:
		return DoStepTo(ControllerState::kOperational);
	case MasterAction::kBackToMaintenance:
		if (state_ == ControllerState::kReadyToRun) {
			state_ = ControllerState::kMaintenance;
			LOG_INFO << "EcatController -> Maintenance";
			return true;
		}
		if (state_ == ControllerState::kOperational) {
			if (!master_.RequestSafeOpState()) {
				LOG_WARN << "Failed to request SAFE_OP before Maintenance";
			}
			if (!master_.RequestPreOpState()) {
				return false;
			}
			state_ = ControllerState::kMaintenance;
			LOG_INFO << "EcatController -> Maintenance";
			return true;
		}
		return false;
	case MasterAction::kStop:
		return DoStepTo(ControllerState::kUninitialized);
	default:
		LOG_ERROR << "Unsupported action";
		return false;
	}
}

// Maintenance -> ReadyToRun 复合转换。
// 依次执行 PDO 配置、SafeOp、DC 配置。
// 任一失败调用 RequestFault()，state_ 变为 kFault。
bool EcatController::DoPrepareRun()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoPrepareRun called in invalid state " << StateToString(state_);
		return false;
	}

	LOG_INFO << "Preparing operation from Maintenance...";

	if (!DoPdoConfigure()) {
		return false;
	}
	if (!DoSafeOp()) {
		return false;
	}
	if (!DoDcConfigure()) {
		return false;
	}

	state_ = ControllerState::kReadyToRun;
	LOG_INFO << "EcatController -> ReadyToRun";
	return true;
}

// ReadyToRun -> Operational。
bool EcatController::DoStartOperation()
{
	if (state_ != ControllerState::kReadyToRun) {
		LOG_ERROR << "DoStartOperation called in invalid state "
			  << StateToString(state_);
		return false;
	}

	LOG_INFO << "Starting operation from " << StateToString(state_) << "...";

	if (!DoOperational()) {
		return false;
	}

	return true;
}

// 初始化网卡/SOEM：Uninitialized -> AdapterReady。
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

// 扫描从站：AdapterReady -> Scanned。
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

	// 扫描后拓扑/身份校验：防止在错误拓扑上继续配置。
	if (config_.expected_slave_count > 0 &&
	    slave_infos_.size() != static_cast<size_t>(config_.expected_slave_count)) {
		LOG_ERROR << "Slave count mismatch: expected " << config_.expected_slave_count
			  << ", found " << slave_infos_.size();
		DoShutdown();
		return false;
	}

	for (size_t i = 0; i < slave_infos_.size() && i < config_.expected_identities.size(); ++i) {
		const auto &expected = config_.expected_identities[i];
		const auto &actual = slave_infos_[i];
		if (expected.vendor_id != 0 && actual.vendor_id != expected.vendor_id) {
			LOG_ERROR << "Slave[" << (i + 1) << "] vendor_id mismatch: expected 0x"
				  << std::hex << expected.vendor_id << ", got 0x" << actual.vendor_id
				  << std::dec;
			DoShutdown();
			return false;
		}
		if (expected.product_id != 0 && actual.product_id != expected.product_id) {
			LOG_ERROR << "Slave[" << (i + 1) << "] product_id mismatch: expected 0x"
				  << std::hex << expected.product_id << ", got 0x" << actual.product_id
				  << std::dec;
			DoShutdown();
			return false;
		}
	}

	for (const auto &info : slave_infos_) {
		LOG_INFO << FormatSlaveInfo(info);
	}

	state_ = ControllerState::kScanned;
	LOG_INFO << "EcatController -> Scanned, " << slave_infos_.size() << " slave(s)";
	return true;
}

// 请求所有从站进入 PREOP，并创建占位 SlaveNode：Scanned -> Maintenance。
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

// 配置所有从站 PDO 映射（StartOperation 内部子步骤）。
bool EcatController::DoPdoConfigure()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoPdoConfigure called in invalid state " << StateToString(state_);
		return false;
	}

	if (!node_manager_.ConfigureAll()) {
		RequestFault("Failed to configure slave nodes");
		return false;
	}

	LOG_INFO << "PDO configured";
	return true;
}

// 配置过程数据并请求 SAFEOP（StartOperation 内部子步骤）。
bool EcatController::DoSafeOp()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoSafeOp called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.ConfigureProcessData()) {
		RequestFault("Failed to configure process data");
		return false;
	}

	if (!master_.RequestSafeOpState() || !master_.CheckAllSlavesInState(EC_STATE_SAFE_OP)) {
		RequestFault("Failed to enter SAFEOP");
		return false;
	}

	LOG_INFO << "SAFEOP reached";
	return true;
}

// 配置分布式时钟（StartOperation 内部子步骤）。
bool EcatController::DoDcConfigure()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoDcConfigure called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.ConfigureDc()) {
		RequestFault("Failed to configure DC");
		return false;
	}

	LOG_INFO << "DC configured";
	return true;
}

// 请求所有从站进入 OPERATIONAL 并更新状态（StartOperation 最后子步骤）。
bool EcatController::DoOperational()
{
	if (state_ != ControllerState::kReadyToRun) {
		LOG_ERROR << "DoOperational called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.RequestOperationalState() ||
	    !master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
		RequestFault("Failed to enter OPERATIONAL");
		return false;
	}

	state_ = ControllerState::kOperational;
	LOG_INFO << "EcatController -> Operational";
	return true;
}

// 安全停止：根据当前状态逐级回滚，最终回到 kUninitialized。
// Operational -> SafeOp -> PreOp -> Init -> Close。
// Maintenance -> PreOp -> Init -> Close。
bool EcatController::DoShutdown()
{
	LOG_INFO << "EcatController shutting down from state " << StateToString(state_);

	if (state_ == ControllerState::kOperational) {
		if (!master_.RequestSafeOpState()) {
			LOG_WARN << "Failed to request SAFE_OP during shutdown";
		}
	}

	if (state_ == ControllerState::kOperational ||
	    state_ == ControllerState::kReadyToRun ||
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

// 进入错误状态时打印所有从站的实时状态和 AL status code，便于现场定位。
void EcatController::LogSlaveSnapshot()
{
	const int slave_count = static_cast<int>(master_.GetSlaveCount());
	if (slave_count <= 0) {
		return;
	}

	LOG_ERROR << "Slave snapshot:";
	for (int i = 1; i <= slave_count; ++i) {
		const uint16_t state = master_.ReadActualState(i);
		const uint16_t al = master_.ReadAlStatusCode(i);
		LOG_ERROR << "  Slave[" << i << "] state=0x" << std::hex << state
			  << ", al_status_code=0x" << al << std::dec;
	}
}

void EcatController::EnterFault(const std::string &reason)
{
	LOG_ERROR << "Entering error state: " << reason;
	LogSlaveSnapshot();
	state_ = ControllerState::kFault;
}

void EcatController::EnterEmergencyStop(const std::string &reason)
{
	LOG_ERROR << "Entering emergency stop state: " << reason;
	LogSlaveSnapshot();
	state_ = ControllerState::kEmergencyStop;
}

// 请求进入普通故障状态，记录原因。供 ActivityScheduler / ProcessDataEngine 调用。
void EcatController::RequestFault(const std::string &reason)
{
	Dispatch(MasterAction::kRequestFault, reason);
}

// 兼容旧接口。
void EcatController::RequestErrorState(const std::string &reason)
{
	RequestFault(reason);
}

void EcatController::RequestEmergencyStop(const std::string &reason)
{
	Dispatch(MasterAction::kRequestEmergencyStop, reason);
}

// 便捷接口：Uninitialized -> AdapterReady -> Scanned -> Maintenance。
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

// 仅初始化网卡/SOEM：Uninitialized -> AdapterReady。
bool EcatController::InitializeAdapter(const EcMasterConfig &config)
{
	if (state_ != ControllerState::kUninitialized) {
		LOG_WARN << "Already initialized, state=" << StateToString(state_);
		return false;
	}

	config_ = config;
	return TransitionTo(ControllerState::kAdapterReady);
}

// 扫描从站：AdapterReady -> Scanned。
bool EcatController::Scan()
{
	if (state_ != ControllerState::kAdapterReady) {
		LOG_WARN << "Scan called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kScanned);
}

// 请求进入 PREOP：Scanned -> Maintenance。
bool EcatController::EnterMaintenance()
{
	if (state_ != ControllerState::kScanned) {
		LOG_WARN << "EnterMaintenance called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kMaintenance);
}

// 请求进入 OPERATIONAL：ReadyToRun -> Operational。
bool EcatController::StartOperation()
{
	if (state_ != ControllerState::kReadyToRun) {
		LOG_WARN << "StartOperation called in invalid state " << StateToString(state_);
		return false;
	}

	return Dispatch(MasterAction::kStartOperation);
}

bool EcatController::PrepareRun()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_WARN << "PrepareRun called in invalid state " << StateToString(state_);
		return false;
	}

	return Dispatch(MasterAction::kPrepareRun);
}

bool EcatController::BackToMaintenance()
{
	if (state_ != ControllerState::kReadyToRun &&
	    state_ != ControllerState::kOperational) {
		LOG_WARN << "BackToMaintenance called in invalid state "
			 << StateToString(state_);
		return false;
	}

	return Dispatch(MasterAction::kBackToMaintenance);
}

// 安全停止：根据当前状态回滚，最终回到 kUninitialized。
void EcatController::Stop()
{
	if (state_ == ControllerState::kUninitialized) {
		return;
	}

	Dispatch(MasterAction::kStop);
}

// 安全遍历所有 SlaveNode，不对外暴露 SlaveNodeManager。
void EcatController::ForEachSlaveNode(const std::function<void(SlaveNode &)> &callback)
{
	for (size_t i = 0; i < node_manager_.GetNodeCount(); ++i) {
		SlaveNode *node = node_manager_.GetNode(i);
		if (node != nullptr) {
			callback(*node);
		}
	}
}

// 获取 SlaveNodeManager（仅供 ProcessDataEngine 等底层周期模块使用）。
SlaveNodeManager &EcatController::GetSlaveNodeManager()
{
	return node_manager_;
}

// 获取 EcMaster（仅供 ProcessDataEngine 等底层周期模块使用）。
EcMaster &EcatController::GetEcMaster()
{
	return master_;
}

// 获取扫描到的从站数量。
size_t EcatController::GetSlaveCount() const
{
	return slave_infos_.size();
}

// 获取当前 ControllerState。
ControllerState EcatController::GetState() const
{
	return state_;
}

// 是否已完成初始化（state_ != kUninitialized）。
bool EcatController::IsInitialized() const
{
	return state_ != ControllerState::kUninitialized;
}

// 是否处于 OPERATIONAL 状态。
bool EcatController::IsOperational() const
{
	return state_ == ControllerState::kOperational;
}

} // namespace mo_ecat
