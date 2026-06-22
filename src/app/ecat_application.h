#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ec_controller/ec_controller.h"

namespace mo_ecat
{

// 应用层命令分发器 + 状态机主循环。
// 不维护独立应用状态，直接基于 ControllerState 判断命令合法性。
class EcatApplication
{
public:
	EcatApplication();
	~EcatApplication();

	// 初始化网卡/SOEM，到达 AdapterReady
	bool Initialize(const EcMasterConfig &config);

	// 安全停止并清理资源
	void Shutdown();

	// 主循环入口：根据当前 ControllerState 执行对应逻辑
	void Run();

	// 请求退出主循环
	void RequestShutdown();

private:
	// 从 stdin 读取一条命令，超时返回 false
	bool ReadCommand(std::string &command, int timeout_ms);

	void HandleAdapterReadyState(const std::string *command);
	void HandleScannedState(const std::string *command);
	void HandleMaintenanceState(const std::string *command);
	void HandleOperationalState(const std::string *command);
	void HandleErrorState(const std::string *command);

	void OnDiagnose();
	void OnParam(const std::vector<std::string> &args);
	void OnInspect();
	void OnHelp();

	void ExecuteActivityForAllNodes(
		const std::function<std::unique_ptr<EcatActivity>(SlaveNode &)> &factory);

	EcatController controller_;
	std::atomic<bool> shutdown_requested_{false};
};

} // namespace mo_ecat
