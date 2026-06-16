#include "servo_axis/joint_manager.h"

namespace mo_ecat
{

bool JointManager::Initialize(EcMaster &master, int slave_count)
{
	Clear();
	for (int i = 1; i <= slave_count; ++i) {
		axes_.emplace_back(std::make_unique<ServoAxis>(master, MakeDefaultAxisConfig(i)));
	}
	return true;
}

void JointManager::Clear()
{
	axes_.clear();
}

size_t JointManager::GetAxisCount() const
{
	return axes_.size();
}

ServoAxis *JointManager::GetAxis(size_t index)
{
	if (index >= axes_.size()) {
		return nullptr;
	}
	return axes_[index].get();
}

ServoAxis *JointManager::GetAxis(const std::string &name)
{
	for (const auto &axis : axes_) {
		if (axis->GetConfig().name == name) {
			return axis.get();
		}
	}
	return nullptr;
}

void JointManager::UpdateAllOutputs()
{
	for (const auto &axis : axes_) {
		axis->UpdatePdoOutput();
	}
}

void JointManager::UpdateAllInputs()
{
	for (const auto &axis : axes_) {
		axis->UpdatePdoInput();
	}
}

} // namespace mo_ecat
