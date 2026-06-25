#pragma once

#include <chrono>

#include "ec_controller/ec_controller.h"
#include "ec_master/ec_master.h"
#include "slave_node/slave_node_manager.h"

namespace mo_ecat
{

// 周期数据处理引擎。
// 职责：根据当前 ControllerState 执行 PDO 周期收发和从站状态监控。
// 不发起状态迁移，发现异常时通过 RequestFault() 通知 EcatController。
class ProcessDataEngine
{
public:
	ProcessDataEngine(EcatController &controller, EcMaster &master,
			  SlaveNodeManager &node_manager);

	// 执行一次 PDO 周期（仅在 kOperational 下执行）
	void RunOnce();

	// 检查从站状态（在 kMaintenance / kOperational 下执行）
	void CheckSlaveStates();

private:
	EcatController &controller_;
	EcMaster &master_;
	SlaveNodeManager &node_manager_;

	// 从站状态检查节流间隔。
	static constexpr std::chrono::milliseconds kStateCheckInterval{500};
	std::chrono::steady_clock::time_point last_state_check_time_;
};

} // namespace mo_ecat
