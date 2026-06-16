#include "slave_node/slave_node_manager.h"

namespace mo_ecat
{

bool SlaveNodeManager::Initialize(EcMaster &master, int slave_count)
{
	Clear();
	for (int i = 1; i <= slave_count; ++i) {
		nodes_.emplace_back(std::make_unique<SlaveNode>(master, MakeDefaultSlaveConfig(i)));
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
		if (node->GetConfig().name == name) {
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
