#include "activity/state_inspection_activity.h"

#include <iomanip>
#include <sstream>

#include "ec_controller/ec_controller.h"
#include "slave_node/slave_node.h"
#include "utils/logger.h"

namespace mo_ecat
{

namespace
{

const char *EtherCatStateToString(uint16_t state)
{
	switch (state) {
	case EC_STATE_INIT:
		return "INIT";
	case EC_STATE_PRE_OP:
		return "PREOP";
	case EC_STATE_BOOT:
		return "BOOT";
	case EC_STATE_SAFE_OP:
		return "SAFEOP";
	case EC_STATE_OPERATIONAL:
		return "OP";
	default:
		return "UNKNOWN";
	}
}

} // namespace

StateInspectionActivity::StateInspectionActivity(SlaveNode &node) : node_(node)
{
}

const char *StateInspectionActivity::GetName() const
{
	return "StateInspection";
}

bool StateInspectionActivity::CanStart(const MasterRuntimeState &state) const
{
	return CanRunMaintenanceActivity(state);
}

ActivityFailurePolicy StateInspectionActivity::GetFailurePolicy() const
{
	// 只读巡检失败不破坏主站状态
	return ActivityFailurePolicy::kKeepControllerState;
}

bool StateInspectionActivity::Execute()
{
	const auto &info = node_.GetInfo();

	// 从总线刷新一次真实状态和 AL status code（避免使用扫描时的缓存值）
	const uint16_t actual_state = node_.RefreshActualState();
	const uint16_t al_status_code = node_.ReadAlStatusCode();

	LOG_INFO << "StateInspection for slave " << info.slave_id << " [" << info.name
		 << "]: state=0x" << std::hex << actual_state << " ("
		 << EtherCatStateToString(actual_state) << "), al_status_code=0x"
		 << al_status_code << std::dec;

	return true;
}

} // namespace mo_ecat
