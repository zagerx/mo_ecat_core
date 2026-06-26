#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mo_ecat
{

// 主站生命周期状态。
// 与内部 ControllerState 保持一一对应，便于无转换映射。
enum class MasterState {
	kUninitialized,
	kAdapterReady,
	kScanned,
	kMaintenance,
	kReadyToRun,
	kOperational,
	kFault,
	kEmergencyStop,
};

// 主站运行时主状态。用于替代旧扁平状态在 UI/调度层直接暴露。
enum class MasterMode {
	kInit,
	kPrepare,
	kRun,
	kFault,
	kEmergencyStop,
};

// 状态迁移阶段。当前版本先以稳定态映射旧状态，后续再接入进入/退出过程。
enum class TransitionStage {
	kEntering,
	kStable,
	kExiting,
};

// 准备态内部阶段，仅在 MasterMode::kPrepare 下有实际含义。
enum class PrepareStage {
	kNone,
	kAdapterReady,
	kTopologyDiscovered,
	kPreOpMaintenance,
	kSafeOpReady,
};

struct MasterRuntimeState {
	MasterMode mode = MasterMode::kInit;
	TransitionStage transition = TransitionStage::kStable;
	PrepareStage prepare_stage = PrepareStage::kNone;
};

bool IsValidRuntimeState(const MasterRuntimeState &state);
bool CanRunMaintenanceActivity(const MasterRuntimeState &state);
bool CanRunProcessData(const MasterRuntimeState &state);
bool ShouldCheckPreOpSlaves(const MasterRuntimeState &state);
bool ShouldCheckOperationalSlaves(const MasterRuntimeState &state);
std::string ToDisplayString(const MasterRuntimeState &state);

// 从站身份标识，用于扫描后身份/拓扑校验。
struct SlaveIdentity {
	uint32_t vendor_id = 0;
	uint32_t product_id = 0;
	uint32_t revision_id = 0;
	uint32_t serial_id = 0;
	std::string name;
};

// 从站静态信息，由扫描结果填充。
struct SlaveInfo {
	int slave_id = 0;
	uint16_t config_address = 0;
	uint16_t alias_address = 0;

	uint32_t vendor_id = 0;
	uint32_t product_id = 0;
	uint32_t revision_id = 0;
	uint32_t serial_id = 0;

	std::string name;
	bool supports_dc = false;

	// Mailbox 基本信息（从 SII/EEPROM 读取）
	uint16_t mbx_l = 0;
	uint16_t mbx_wo = 0;
	uint16_t mbx_ro = 0;
	uint16_t mbx_proto = 0;
	uint8_t mbx_cnt = 0;

	// 运行状态与能力
	uint16_t state = 0;
	uint16_t al_status_code = 0;
	uint8_t coe_details = 0;
	uint32_t output_bytes = 0;
	uint32_t input_bytes = 0;
};

// 单个从站的周期反馈数据。
struct SlaveFeedback {
	int slave_id = 0;
	std::string name;
	uint16_t status_word = 0;
	uint16_t error_code = 0;
	int32_t actual_position = 0;
	int32_t actual_velocity = 0;
	int16_t actual_torque = 0;
};

// 主站快照，用于 GUI/CLI 一次性获取当前状态。
struct MasterSnapshot {
	MasterState state = MasterState::kUninitialized;
	MasterRuntimeState runtime_state;
	std::vector<SlaveInfo> slaves;
	std::string last_fault;
};

} // namespace mo_ecat
