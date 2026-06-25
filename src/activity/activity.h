#pragma once

#include <memory>
#include <string>

namespace mo_ecat
{

// 前向声明，避免与 ec_controller.h 循环包含
enum class ControllerState;

// Activity 执行失败后的主站处理策略
enum class ActivityFailurePolicy {
	kKeepControllerState, // 只记录日志，保持当前主站状态
	kEnterError,          // 进入 Fault，等待 Stop 后恢复
	kShutdown,            // 自动调用 Stop() 回到 Uninitialized
};

// 维护活动基类。
// 维护活动是横向操作：Activity 可以改变单个从站的 EtherCAT 状态，
// 但不得直接修改 EcatController 的 ControllerState。
class EcatActivity
{
      public:
	virtual ~EcatActivity() = default;

	// 活动名称，用于日志
	virtual const char *GetName() const = 0;

	// 是否允许在给定主站状态下启动
	virtual bool CanStart(ControllerState state) const = 0;

	// 执行活动，返回是否成功
	virtual bool Execute() = 0;

	// 活动失败后的主站处理策略
	virtual ActivityFailurePolicy GetFailurePolicy() const
	{
		return ActivityFailurePolicy::kEnterError;
	}
};

} // namespace mo_ecat
