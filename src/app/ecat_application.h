#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/command_reader.h"
#include "ec_controller/ec_controller.h"

namespace mo_ecat
{

// 应用层状态机单步执行器。
// 不维护独立应用状态，直接基于 ControllerState 判断命令合法性。
// 不拥有自己的线程，由上层按周期调用 Run()。
class EcatApplication
{
public:
	explicit EcatApplication(std::unique_ptr<CommandReader> command_reader);
	~EcatApplication();

	// 初始化网卡/SOEM，到达 AdapterReady
	bool Initialize(const EcMasterConfig &config);

	// 安全停止并清理资源
	void Shutdown();

	// 单步执行一次状态机迭代。
	// 返回 true：继续运行；返回 false：收到退出请求（exit/quit/EOF）。
	bool Run();

private:
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
	std::unique_ptr<CommandReader> command_reader_;
};

} // namespace mo_ecat
