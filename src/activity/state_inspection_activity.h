#pragma once

#include <cstdint>

#include "activity/activity.h"

namespace mo_ecat
{

class SlaveNode;

// 从站状态巡检 Activity
// 读取单个从站的实际 EtherCAT 状态、AL status、AL status code。
// 只读操作，失败时不影响主站状态。
class StateInspectionActivity : public EcatActivity
{
public:
	explicit StateInspectionActivity(SlaveNode &node);

	const char *GetName() const override;
	bool CanStart(const MasterRuntimeState &state) const override;
	bool Execute() override;
	ActivityFailurePolicy GetFailurePolicy() const override;

private:
	SlaveNode &node_;
};

} // namespace mo_ecat
