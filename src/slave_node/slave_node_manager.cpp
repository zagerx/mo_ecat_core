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

	for (const auto &info : slave_infos) {
		LOG_INFO << "Slave " << info.slave_id << " [" << info.name << "] placeholder created"
			 << " (vendor=0x" << std::hex << info.vendor_id << ", product=0x"
			 << info.product_id << std::dec << ")";
		nodes_.emplace_back(std::make_unique<SlaveNode>(master, info));
	}

	return true;
}

bool SlaveNodeManager::ConfigureAll()
{
	SlaveTypeDetector detector;
	SlaveConfigFactory config_factory;

	bool all_ok = true;
	for (const auto &node : nodes_) {
		if (node->IsConfigured()) {
			continue;
		}

		const SlaveInfo &info = node->GetInfo();
		const SlaveType type = detector.Detect(info);
		SlaveConfig config = config_factory.CreateConfig(info, type);

		LOG_INFO << "Slave " << info.slave_id << " [" << info.name << "] configuring as type "
			 << static_cast<int>(type) << " ...";

		if (!node->Configure(config)) {
			LOG_ERROR << "Slave " << info.slave_id << " [" << info.name
				  << "] configuration failed";
			all_ok = false;
		}
	}

	return all_ok;
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
		if (node->IsConfigured()) {
			node->UpdatePdoOutput();
		}
	}
}

void SlaveNodeManager::UpdateAllInputs()
{
	for (const auto &node : nodes_) {
		if (node->IsConfigured()) {
			node->UpdatePdoInput();
		}
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
