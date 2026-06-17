#include "slave_node/slave_node_manager.h"

#include <iomanip>

#include "slave_node/slave_config_factory.h"
#include "slave_node/slave_type_detector.h"
#include "utils/logger.h"

namespace mo_ecat
{

bool SlaveNodeManager::Initialize(EcMaster &master, const std::vector<SlaveInfo> &slave_infos)
{
	Clear();

	SlaveTypeDetector detector;
	SlaveConfigFactory config_factory;

	for (const auto &info : slave_infos) {
		const SlaveType type = detector.Detect(info);
		SlaveConfig config = config_factory.CreateConfig(info, type);

		LOG_INFO << "Slave " << info.slave_id << " [" << info.name << "] detected as "
			 << static_cast<int>(type) << " (vendor=0x" << std::hex << info.vendor_id
			 << ", product=0x" << info.product_id << std::dec << ")";

		nodes_.emplace_back(std::make_unique<SlaveNode>(master, config, info));
	}

	return true;
}

void SlaveNodeManager::Clear()
{
	nodes_.clear();
}

size_t SlaveNodeManager::GetNodeCount() const
{
	return nodes_.size();
}

SlaveNode *SlaveNodeManager::GetNode(size_t index)
{
	if (index >= nodes_.size()) {
		return nullptr;
	}
	return nodes_[index].get();
}

SlaveNode *SlaveNodeManager::GetNode(const std::string &name)
{
	for (const auto &node : nodes_) {
		if (node->GetInfo().name == name) {
			return node.get();
		}
	}
	return nullptr;
}

void SlaveNodeManager::UpdateAllOutputs()
{
	for (const auto &node : nodes_) {
		node->UpdatePdoOutput();
	}
}

void SlaveNodeManager::UpdateAllInputs()
{
	for (const auto &node : nodes_) {
		node->UpdatePdoInput();
	}
}

bool SlaveNodeManager::RequestAllState(uint16_t state)
{
	bool all_ok = true;
	for (const auto &node : nodes_) {
		if (!node->RequestState(state)) {
			all_ok = false;
		}
	}
	return all_ok;
}

} // namespace mo_ecat
