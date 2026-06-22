#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ec_controller/ec_controller.h"

namespace mo_ecat
{

// 命令到状态迁移 / Activity 的映射器。
// 不维护独立应用状态，直接基于 ControllerState 判断命令合法性。
class EcatApplication
{
public:
	EcatApplication();
	~EcatApplication();

	// 初始化网卡/SOEM，到达 InitDone
	bool Initialize(const EcMasterConfig &config);

	// 安全停止并清理资源
	void Shutdown();

	// 处理一条命令（由 main 线程调用）
	void HandleCommand(const std::string &command);

private:
	void OnScan();
	void OnConfig();
	void OnStop();
	void OnDiagnose();
	void OnParam(const std::vector<std::string> &args);
	void OnInspect();
	void OnHelp();

	void ExecuteActivityForAllNodes(
		const std::function<std::unique_ptr<EcatActivity>(SlaveNode &)> &factory);

	EcatController controller_;
};

} // namespace mo_ecat
