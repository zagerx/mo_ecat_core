#include "ec_controller/ec_controller.h"

#include <iostream>
#include <thread>
#include <chrono>

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

	initialized_ = true;
	return true;
}

bool EcatController::Start()
{
	if (!initialized_) {
		std::cerr << "EcatController not initialized, call Initialize() first\n";
		return false;
	}

	if (running_) {
		std::cerr << "EcatController already running\n";
		return false;
	}

	if (!master_.RequestSafeOpState()) {
		std::cerr << "Failed to enter SAFE_OP\n";
		return false;
	}

	master_.StartCyclicThread();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	if (!master_.RequestOperationalState()) {
		std::cerr << "Failed to enter OPERATIONAL\n";
		master_.StopCyclicThread();
		return false;
	}

	running_ = true;
	std::cout << "EcatController started, EtherCAT operational\n";
	return true;
}

void EcatController::Stop()
{
	if (!initialized_) {
		return;
	}

	if (running_) {
		master_.StopCyclicThread();
		running_ = false;
	}

	master_.RequestSafeOpState();
	master_.RequestInitState();
	initialized_ = false;
}

bool EcatController::IsInitialized() const
{
	return initialized_;
}

bool EcatController::IsRunning() const
{
	return running_;
}

EcMaster &EcatController::GetMaster()
{
	return master_;
}

} // namespace mo_ecat
