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
	case ControllerState::kOperational:
		return "Operational";
	case ControllerState::kError:
		return "Error";
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

// 统一状态转换入口。
// 1. 相同状态直接返回 true
// 2. kError 状态只允许回到 kUninitialized
// 3. 目标状态必须被 kAllowedTransitions 允许
// 4. Maintenance -> Operational 使用复合转换 DoStartOperation()
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

	// Maintenance -> Operational 是复合转换，不经过公开中间状态。
	if (state_ == ControllerState::kMaintenance &&
	    target == ControllerState::kOperational) {
		return DoStartOperation();
	}

	return DoStepTo(target);
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
	case ControllerState::kOperational:
		return DoStartOperation();
	case ControllerState::kUninitialized:
		return DoShutdown();
	default:
		LOG_ERROR << "Unknown target state " << StateToString(next);
		return false;
	}
}

// Maintenance -> Operational 复合转换。
// 依次执行 PDO 配置、SafeOp、DC 配置、Operational。
// 任一失败调用 RequestErrorState()，state_ 变为 kError。
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
		RequestErrorState("Failed to configure slave nodes");
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
		RequestErrorState("Failed to configure process data");
		return false;
	}

	if (!master_.RequestSafeOpState() || !master_.CheckAllSlavesInState(EC_STATE_SAFE_OP)) {
		RequestErrorState("Failed to enter SAFEOP");
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
		RequestErrorState("Failed to configure DC");
		return false;
	}

	LOG_INFO << "DC configured";
	return true;
}

// 请求所有从站进入 OPERATIONAL 并更新状态（StartOperation 最后子步骤）。
bool EcatController::DoOperational()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_ERROR << "DoOperational called in invalid state " << StateToString(state_);
		return false;
	}

	if (!master_.RequestOperationalState() ||
	    !master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
		RequestErrorState("Failed to enter OPERATIONAL");
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

// 请求进入错误状态，记录原因。供 ActivityScheduler / ProcessDataEngine 调用。
void EcatController::RequestErrorState(const std::string &reason)
{
	LOG_ERROR << "Entering error state: " << reason;
	state_ = ControllerState::kError;
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

// 请求进入 OPERATIONAL：Maintenance -> Operational（复合转换）。
bool EcatController::StartOperation()
{
	if (state_ != ControllerState::kMaintenance) {
		LOG_WARN << "StartOperation called in invalid state " << StateToString(state_);
		return false;
	}

	return TransitionTo(ControllerState::kOperational);
}

// 安全停止：根据当前状态回滚，最终回到 kUninitialized。
void EcatController::Stop()
{
	if (state_ == ControllerState::kUninitialized) {
		return;
	}

	TransitionTo(ControllerState::kUninitialized);
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
