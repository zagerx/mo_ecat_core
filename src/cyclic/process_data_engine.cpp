#include "cyclic/process_data_engine.h"

#include "utils/logger.h"

namespace mo_ecat
{

// 构造周期任务引擎。
ProcessDataEngine::ProcessDataEngine(EcatController &controller, EcMaster &master,
				     SlaveNodeManager &node_manager)
	: controller_(controller), master_(master), node_manager_(node_manager)
{
}

// 执行一次 PDO 周期：更新输出 -> PDO 收发 -> 更新输入（仅 kOperational 有效）。
void ProcessDataEngine::RunOnce()
{
	if (controller_.GetState() != ControllerState::kOperational) {
		LOG_WARN << "RunOnce ignored: not operational";
		return;
	}

	node_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	node_manager_.UpdateAllInputs();
}

// 检查从站状态：Maintenance/Operational 阶段检测到掉线则请求 kError。
// 按 kStateCheckInterval 节流，避免每周期频繁读取总线。
void ProcessDataEngine::CheckSlaveStates()
{
	const auto now = std::chrono::steady_clock::now();
	if (now - last_state_check_time_ < kStateCheckInterval) {
		return;
	}
	last_state_check_time_ = now;

	switch (controller_.GetState()) {
	case ControllerState::kMaintenance:
		if (!master_.CheckAllSlavesInState(EC_STATE_PRE_OP)) {
			controller_.RequestErrorState("Slave dropped out of PREOP");
		}
		break;
	case ControllerState::kOperational:
		if (!master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
			controller_.RequestErrorState("Slave dropped out of OPERATIONAL");
		}
		break;
	default:
		break;
	}
}

} // namespace mo_ecat
