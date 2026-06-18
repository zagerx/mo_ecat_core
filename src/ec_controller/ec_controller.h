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

	// 初始化 EtherCAT 主站：绑定网卡 + 扫描从站 + 进入 PREOP
	// 当前阶段不到 SAFEOP/OP；PDO 映射、DC 配置放到后续 PDO 阶段。
	bool Initialize(const EcMasterConfig &config);

	// 请求进入 OPERATIONAL 状态（PDO 阶段使用；当前 main 不调用）
	bool StartOperation();

	// 安全停止：根据当前状态回滚，最终回到 INIT 并清理资源
	void Stop();

	// 周期任务：执行一次 PDO 收发（PDO 阶段由周期线程调用）
	void RunOneCycle();

	// 状态监控：读取一次从站状态（PDO 阶段由监控线程调用）
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
