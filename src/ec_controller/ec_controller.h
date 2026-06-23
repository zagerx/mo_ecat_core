#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"
#include "slave_node/slave_node_manager.h"

namespace mo_ecat
{

// EtherCAT 主站生命周期状态。
// 只表示稳定工况；PDO 配置 / SafeOp / DC 同步等过渡步骤作为
// StartOperation() 的内部子步骤，不暴露为 ControllerState。
enum class ControllerState {
	kUninitialized,   // 初始状态：未绑定网卡、未扫描从站
	kAdapterReady,    // SOEM 初始化完成，网卡已绑定
	kScanned,         // 从站扫描完成，已获取 SlaveInfo
	kMaintenance,     // 所有从站进入 PREOP，可执行 SDO 维护活动
	kOperational,     // 所有从站进入 OPERATIONAL，PDO 周期可运行
	kError            // 异常状态：只能 Stop() 回到 Uninitialized
};

class EcatController
{
      public:
	EcatController();
	~EcatController();

	// 初始化 EtherCAT 主站：绑定网卡 + 扫描从站 + 进入 Maintenance
	// 这是便捷接口，内部调用 InitializeAdapter -> Scan -> EnterMaintenance。
	bool Initialize(const EcMasterConfig &config);

	// 仅初始化网卡/SOEM，生命周期状态到达 AdapterReady
	bool InitializeAdapter(const EcMasterConfig &config);

	// 从 AdapterReady 扫描从站，到达 Scanned
	bool Scan();

	// 从 Scanned 请求进入 Maintenance（PREOP）
	bool EnterMaintenance();

	// 请求进入 OPERATIONAL 状态（PDO 阶段使用；当前 main 不调用）
	bool StartOperation();

	// 安全停止：根据当前状态回滚，最终回到 INIT 并清理资源
	void Stop();

	// 请求进入错误状态。供 ActivityScheduler / ProcessDataEngine 等调度/监控类调用。
	void RequestErrorState(const std::string &reason);

	// 安全遍历所有 SlaveNode，不暴露 SlaveNodeManager 本身。
	// EcatApplication 应使用此接口，而不是 GetSlaveNodeManager()。
	void ForEachSlaveNode(const std::function<void(SlaveNode &)> &callback);

	// 获取节点管理器，仅供 ProcessDataEngine 等底层周期模块使用。
	// EcatApplication 禁止直接调用。
	SlaveNodeManager &GetSlaveNodeManager();

	// 获取 EcMaster，仅供 ProcessDataEngine 等底层周期模块使用。
	// EcatApplication 禁止直接调用。
	EcMaster &GetEcMaster();

	// 获取扫描到的从站数量（Scanned/Maintenance 等状态有效）
	size_t GetSlaveCount() const;

	ControllerState GetState() const;
	bool IsInitialized() const;
	bool IsOperational() const;

	// 状态转字符串，供日志和上层使用
	static const char *StateToString(ControllerState state);

      private:
	// 统一状态转换入口。负责查表校验、执行单步或复合转换、失败处理。
	bool TransitionTo(ControllerState target);

	// 状态转换表：当前状态 -> 允许的目标状态集合。
	// 只记录公开状态之间的相邻或回退转换。
	static const std::map<ControllerState, std::vector<ControllerState>> kAllowedTransitions;

	// 执行单个公开状态转换步骤（Init/Scan/Maintenance/Operational/Shutdown）。
	bool DoStepTo(ControllerState next);

	// Maintenance -> Operational 的复合转换。
	// 内部顺序执行 PDO 配置 -> SafeOp -> DC 配置 -> Op，任一失败进入 kError。
	bool DoStartOperation();

	// 各公开状态对应的具体实现
	bool DoInit();
	bool DoScan();
	bool DoEnterMaintenance();

	// 以下四个为 StartOperation() 的内部子步骤，不单独更新 ControllerState。
	// 只有 DoOperational() 成功时才会把状态置为 kOperational。
	bool DoPdoConfigure();
	bool DoSafeOp();
	bool DoDcConfigure();
	bool DoOperational();

	// 安全停止：根据当前状态逐级回滚，最终关闭 SOEM 并清理资源。
	bool DoShutdown();

	// 进入错误状态时打印所有从站快照，用于现场定位问题。
	void LogSlaveSnapshot();

	// 从 EcMaster 刷新所有从站信息。
	std::vector<SlaveInfo> RefreshSlaveInfos() const;

	ControllerState state_ = ControllerState::kUninitialized;
	EcMaster master_;
	SlaveNodeManager node_manager_;

	EcMasterConfig config_;
	std::vector<SlaveInfo> slave_infos_;
};

} // namespace mo_ecat
