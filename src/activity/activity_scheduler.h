#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "activity/activity.h"

namespace mo_ecat
{

class EcatController;

// Activity 调度器。
// 职责：检查 Activity 是否允许在当前 MasterRuntimeState 下启动，同步执行 Activity，
// 并根据 Activity 声明的失败策略处理结果。
// 不直接访问任何从站/EcMaster，只通过 EcatController 查询状态或请求状态变更。
class ActivityScheduler
{
public:
	ActivityScheduler(EcatController &controller,
			  std::function<MasterRuntimeState()> runtime_state_provider);

	// 执行一个 Activity。Activity 为 nullptr 或当前有 Activity 正在执行时返回 false。
	bool Execute(std::unique_ptr<EcatActivity> activity);

	// 查询是否有 Activity 正在执行。
	bool IsRunning() const;

private:
	EcatController &controller_;
	std::function<MasterRuntimeState()> runtime_state_provider_;
	std::atomic<bool> running_{false};
};

} // namespace mo_ecat
