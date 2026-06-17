#include "ec_controller/ec_controller.h"

#include <iomanip>

#include "utils/logger.h"

namespace mo_ecat
{

EcatController::EcatController()
{
}

EcatController::~EcatController()
{
	Stop();
}

bool EcatController::Initialize(const EcMasterConfig &config)
{
	if (initialized_) {
		LOG_WARN << "EcatController already initialized";
		return false;
	}

	if (!master_.Initialize(config)) {
		return false;
	}

	auto slave_infos = master_.ScanSlaves();
	if (slave_infos.empty()) {
		return false;
	}

	for (const auto &info : slave_infos) {
		LOG_INFO << "Slave[" << info.slave_id << "] info:"
			 << " name=\"" << info.name << "\""
			 << " vendor=0x" << std::hex << info.vendor_id
			 << " product=0x" << info.product_id
			 << " revision=0x" << info.revision_id
			 << " serial=0x" << info.serial_id << std::dec
			 << " config_addr=0x" << std::hex << info.config_address
			 << " alias_addr=0x" << info.alias_address << std::dec
			 << " dc=" << (info.supports_dc ? "yes" : "no");
	}

	node_manager_.Initialize(master_, slave_infos);
	LOG_INFO << "Created " << node_manager_.GetNodeCount() << " slave node(s)";

	initialized_ = true;
	return true;
}

bool EcatController::StartOperation()
{
	if (!initialized_) {
		LOG_WARN << "EcatController not initialized, call Initialize() first";
		return false;
	}

	if (operational_) {
		LOG_WARN << "EcatController already operational";
		return false;
	}

	if (!master_.RequestOperationalState()) {
		LOG_ERROR << "Failed to enter OPERATIONAL";
		return false;
	}

	operational_ = true;
	LOG_INFO << "EcatController operational";
	return true;
}

void EcatController::Stop()
{
	if (!initialized_) {
		return;
	}

	master_.RequestSafeOpState();
	master_.RequestInitState();

	node_manager_.Clear();
	operational_ = false;
	initialized_ = false;
}

void EcatController::RunOneCycle()
{
	node_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	node_manager_.UpdateAllInputs();
}

void EcatController::CheckSlaveStates()
{
	master_.CheckSlaveStates();
}

SlaveNodeManager &EcatController::GetSlaveNodeManager()
{
	return node_manager_;
}

bool EcatController::IsInitialized() const
{
	return initialized_;
}

bool EcatController::IsOperational() const
{
	return operational_;
}

} // namespace mo_ecat
