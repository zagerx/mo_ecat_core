#pragma once

#include "ec_master/ec_master.h"
#include "slave_node/slave_node_manager.h"

namespace mo_ecat
{
enum class ControllerState {
	kUninitialized,
	kInitDone,
	kScanned,
	kPreOp,
	kPdoConfigured,
	kSafeOp,
	kDcConfigured,
	kOperational,
	kError
};

class EcatController
{
      public:
	EcatController();
	~EcatController();

	// 初始化 EtherCAT 主站：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置 + 进入 SAFE_OP
	bool Initialize(const EcMasterConfig &config);

	// 进入 OPERATIONAL 状态（调用前应先启动周期线程）
	bool StartOperation();

	// 安全停止：进入 SAFE_OP + 进入 INIT
	void Stop();

	// 周期任务：由 main 创建的 PDO 线程调用
	void RunOneCycle();

	// 状态监控：由 main 创建的状态监控线程调用
	void CheckSlaveStates();

	// 获取节点管理器，供上层按名字/索引访问从站
	SlaveNodeManager &GetSlaveNodeManager();

	bool IsInitialized() const;
	bool IsOperational() const;

      private:
	void Shutdown(bool request_states);
	std::vector<SlaveInfo> RefreshSlaveInfos() const;
	ControllerState state_ = ControllerState::kUninitialized;
	EcMaster master_;
	SlaveNodeManager node_manager_;
	bool initialized_ = false;
	bool operational_ = false;
};

} // namespace mo_ecat
