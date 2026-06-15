#include "ec_controller/ec_controller.h"

#include <iostream>

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
		std::cerr << "EcatController already initialized\n";
		return false;
	}

	if (!master_.Initialize(config)) {
		return false;
	}

	if (!master_.ScanAndConfigure()) {
		return false;
	}

	if (!master_.RequestSafeOpState()) {
		std::cerr << "Failed to enter SAFE_OP\n";
		return false;
	}

	initialized_ = true;
	return true;
}

bool EcatController::StartOperation()
{
	if (!initialized_) {
		std::cerr << "EcatController not initialized, call Initialize() first\n";
		return false;
	}

	if (operational_) {
		std::cerr << "EcatController already operational\n";
		return false;
	}

	if (!master_.RequestOperationalState()) {
		std::cerr << "Failed to enter OPERATIONAL\n";
		return false;
	}

	operational_ = true;
	std::cout << "EcatController operational\n";
	return true;
}

void EcatController::Stop()
{
	if (!initialized_) {
		return;
	}

	master_.RequestSafeOpState();
	master_.RequestInitState();
	operational_ = false;
	initialized_ = false;
}

bool EcatController::IsInitialized() const
{
	return initialized_;
}

bool EcatController::IsOperational() const
{
	return operational_;
}

EcMaster &EcatController::GetMaster()
{
	return master_;
}

} // namespace mo_ecat
