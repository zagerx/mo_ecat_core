#include "mo_ecat/types.h"

namespace mo_ecat
{

bool IsValidRuntimeState(const MasterRuntimeState &state)
{
	switch (state.transition) {
	case TransitionStage::kEntering:
	case TransitionStage::kStable:
	case TransitionStage::kExiting:
		break;
	}

	switch (state.mode) {
	case MasterMode::kInit:
	case MasterMode::kRun:
	case MasterMode::kFault:
	case MasterMode::kEmergencyStop:
		return state.prepare_stage == PrepareStage::kNone;

	case MasterMode::kPrepare:
		switch (state.prepare_stage) {
		case PrepareStage::kNone:
		case PrepareStage::kAdapterReady:
		case PrepareStage::kTopologyDiscovered:
		case PrepareStage::kPreOpMaintenance:
		case PrepareStage::kSafeOpReady:
			return true;
		}
	}

	return false;
}

bool CanRunMaintenanceActivity(const MasterRuntimeState &state)
{
	return state.mode == MasterMode::kPrepare &&
	       state.transition == TransitionStage::kStable &&
	       state.prepare_stage == PrepareStage::kPreOpMaintenance;
}

bool CanRunProcessData(const MasterRuntimeState &state)
{
	return state.mode == MasterMode::kRun &&
	       state.transition == TransitionStage::kStable &&
	       state.prepare_stage == PrepareStage::kNone;
}

bool ShouldCheckPreOpSlaves(const MasterRuntimeState &state)
{
	return CanRunMaintenanceActivity(state);
}

bool ShouldCheckOperationalSlaves(const MasterRuntimeState &state)
{
	return CanRunProcessData(state);
}

std::string ToDisplayString(const MasterRuntimeState &state)
{
	const auto transition_prefix = [](TransitionStage transition) -> const char * {
		switch (transition) {
		case TransitionStage::kEntering:
			return "进入";
		case TransitionStage::kExiting:
			return "退出";
		case TransitionStage::kStable:
			return "";
		}
		return "";
	};

	const auto prepare_stage = [](PrepareStage stage) -> const char * {
		switch (stage) {
		case PrepareStage::kAdapterReady:
			return "网卡就绪";
		case PrepareStage::kTopologyDiscovered:
			return "拓扑已发现";
		case PrepareStage::kPreOpMaintenance:
			return "PREOP维护";
		case PrepareStage::kSafeOpReady:
			return "SAFEOP就绪";
		case PrepareStage::kNone:
			return "";
		}
		return "";
	};

	std::string text = transition_prefix(state.transition);
	switch (state.mode) {
	case MasterMode::kInit:
		text += "初始化态";
		break;
	case MasterMode::kPrepare: {
		text += "准备态";
		const std::string stage = prepare_stage(state.prepare_stage);
		if (!stage.empty()) {
			text += " / ";
			text += stage;
		}
		break;
	}
	case MasterMode::kRun:
		text += "运行态";
		break;
	case MasterMode::kFault:
		text += "故障态";
		break;
	case MasterMode::kEmergencyStop:
		text += "紧急停止态";
		break;
	}

	if (!IsValidRuntimeState(state)) {
		text += "（非法组合）";
	}
	return text;
}

} // namespace mo_ecat
