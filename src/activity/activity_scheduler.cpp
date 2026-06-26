#include "activity/activity_scheduler.h"

#include <utility>

#include "ec_controller/ec_controller.h"
#include "utils/logger.h"

namespace mo_ecat
{

// 构造 ActivityScheduler，通过运行时状态提供器检查启动许可，并通过 controller 处理失败策略。
ActivityScheduler::ActivityScheduler(
	EcatController &controller, std::function<MasterRuntimeState()> runtime_state_provider)
	: controller_(controller), runtime_state_provider_(std::move(runtime_state_provider))
{
}

// 同步执行一个 Activity：检查当前状态、保证互斥、根据失败策略处理结果。
bool ActivityScheduler::Execute(std::unique_ptr<EcatActivity> activity)
{
	if (!activity) {
		LOG_ERROR << "Null activity";
		return false;
	}

	// 原子地抢占执行权，防止多线程/重入下并发执行 Activity。
	if (running_.exchange(true)) {
		LOG_ERROR << "Activity already running";
		return false;
	}

	const auto state = runtime_state_provider_ ? runtime_state_provider_() :
						     MasterRuntimeState{};
	if (!activity->CanStart(state)) {
		LOG_ERROR << "Activity " << activity->GetName()
			  << " cannot start in state " << ToDisplayString(state);
		running_.store(false);
		return false;
	}

	LOG_INFO << "Activity started: " << activity->GetName();

	const bool ok = activity->Execute();

	LOG_INFO << "Activity finished: " << activity->GetName()
		 << ", result=" << (ok ? "ok" : "failed");
	running_.store(false);

	if (!ok) {
		switch (activity->GetFailurePolicy()) {
		case ActivityFailurePolicy::kEnterError:
			controller_.RequestFault(std::string("Activity failed: ") +
						 activity->GetName());
			break;
		case ActivityFailurePolicy::kShutdown:
			controller_.Stop();
			break;
		case ActivityFailurePolicy::kKeepControllerState:
		default:
			LOG_WARN << "Activity failed but keeping controller state";
			break;
		}
	}

	return ok;
}

// 是否有 Activity 正在执行。
bool ActivityScheduler::IsRunning() const
{
	return running_.load();
}

} // namespace mo_ecat
