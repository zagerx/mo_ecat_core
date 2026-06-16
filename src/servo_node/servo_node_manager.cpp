#include "servo_node/servo_node_manager.h"

namespace mo_ecat {

bool ServoNodeManager::Initialize(EcMaster &master, int slave_count)
{
    Clear();
    for (int i = 1; i <= slave_count; ++i) {
        nodes_.emplace_back(std::make_unique<ServoNode>(master, MakeDefaultSlaveConfig(i)));
    }
    return true;
}

void ServoNodeManager::Clear()
{
    nodes_.clear();
}

size_t ServoNodeManager::GetNodeCount() const
{
    return nodes_.size();
}

ServoNode *ServoNodeManager::GetNode(size_t index)
{
    if (index >= nodes_.size()) {
        return nullptr;
    }
    return nodes_[index].get();
}

ServoNode *ServoNodeManager::GetNode(const std::string &name)
{
    for (const auto &node : nodes_) {
        if (node->GetConfig().name == name) {
            return node.get();
        }
    }
    return nullptr;
}

void ServoNodeManager::UpdateAllOutputs()
{
    for (const auto &node : nodes_) {
        node->UpdatePdoOutput();
    }
}

void ServoNodeManager::UpdateAllInputs()
{
    for (const auto &node : nodes_) {
        node->UpdatePdoInput();
    }
}

bool ServoNodeManager::RequestAllState(uint16_t state)
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
