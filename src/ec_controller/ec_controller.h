#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "activity/activity.h"
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

	// 状态监控：读取一次从站状态，运行中掉线时进入 kError
	void CheckSlaveStates();

	// 获取节点管理器，供上层按名字/索引访问从站
	SlaveNodeManager &GetSlaveNodeManager();

	// 执行同步维护活动（第一阶段采用同步模型，后续可扩展异步）
	bool ExecuteActivity(std::unique_ptr<EcatActivity> activity);

	// 查询是否有活动正在执行
	bool IsActivityRunning() const;

	bool IsInitialized() const;
	bool IsOperational() const;

      private:
	// 统一状态转换入口
	bool TransitionTo(ControllerState target);

	// 状态转换表：当前状态 -> 允许的目标状态（用于后退/特殊转换）
	static const std::map<ControllerState, std::vector<ControllerState>> kAllowedTransitions;

	// 执行单个状态转换步骤
	bool DoStepTo(ControllerState next);

	// 各状态对应的具体实现
	bool DoInit();
	bool DoScan();
	bool DoPreOp();
	bool DoPdoConfigure();
	bool DoSafeOp();
	bool DoDcConfigure();
	bool DoOperational();
	bool DoShutdown();

	// 进入错误状态
	void EnterErrorState(const std::string &reason);

	std::vector<SlaveInfo> RefreshSlaveInfos() const;
	static const char *StateToString(ControllerState state);

	ControllerState state_ = ControllerState::kUninitialized;
	EcMaster master_;
	SlaveNodeManager node_manager_;

	EcMasterConfig config_;
	std::vector<SlaveInfo> slave_infos_;

	std::atomic<bool> activity_running_{false};
};

} // namespace mo_ecat
