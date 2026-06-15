#include "ec_master/ec_master.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

namespace mo_ecat
{

EcMaster::EcMaster()
{
	std::memset(&ctx_, 0, sizeof(ctx_));
	std::memset(iomap_, 0, sizeof(iomap_));
}

EcMaster::~EcMaster()
{
	Close();
}

bool EcMaster::Initialize(const EcMasterConfig &config)
{
	config_ = config;

	if (!ecx_init(&ctx_, config_.ifname.c_str())) {
		std::cerr << "ecx_init failed on " << config_.ifname << " (try sudo)\n";
		return false;
	}
	std::cout << "SOEM initialized on " << config_.ifname << "\n";
	return true;
}

void EcMaster::Close()
{
	StopCyclicThread();
	ecx_close(&ctx_);
	std::cout << "SOEM closed.\n";
}

bool EcMaster::ScanAndConfigure()
{
	int slave_count = ecx_config_init(&ctx_);
	if (slave_count <= 0) {
		std::cerr << "No slaves found\n";
		return false;
	}

	ecx_config_map_group(&ctx_, iomap_, 0);
	expected_wkc_ = (ctx_.grouplist[0].outputsWKC * 2) + ctx_.grouplist[0].inputsWKC;

	if (config_.use_dc) {
		ecx_configdc(&ctx_);
	}

	std::cout << slave_count << " slave(s) found and configured.\n";
	std::cout << "Outputs: " << ctx_.grouplist[0].Obytes
		  << " bytes, Inputs: " << ctx_.grouplist[0].Ibytes
		  << " bytes, Expected WKC: " << expected_wkc_ << "\n";
	return true;
}

int EcMaster::GetSlaveCount() const
{
	return ctx_.slavecount;
}

bool EcMaster::RequestOperationalState()
{
	ctx_.slavelist[0].state = EC_STATE_OPERATIONAL;
	ecx_writestate(&ctx_, 0);
	ecx_statecheck(&ctx_, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
	return ctx_.slavelist[0].state == EC_STATE_OPERATIONAL;
}

bool EcMaster::RequestSafeOpState()
{
	ctx_.slavelist[0].state = EC_STATE_SAFE_OP;
	ecx_writestate(&ctx_, 0);
	ecx_statecheck(&ctx_, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);
	return ctx_.slavelist[0].state == EC_STATE_SAFE_OP;
}

bool EcMaster::RequestInitState()
{
	ctx_.slavelist[0].state = EC_STATE_INIT;
	ecx_writestate(&ctx_, 0);
	ecx_statecheck(&ctx_, 0, EC_STATE_INIT, EC_TIMEOUTSTATE);
	return ctx_.slavelist[0].state == EC_STATE_INIT;
}

void EcMaster::CyclicTask()
{
	using namespace std::chrono;

	auto next_time = steady_clock::now();
	auto interval = microseconds(config_.cycle_time_us);

	while (running_) {
		ecx_send_processdata(&ctx_);
		int wkc = ecx_receive_processdata(&ctx_, EC_TIMEOUTRET);

		stats_.cycle_count++;
		if (wkc != expected_wkc_) {
			stats_.wkc_mismatch_count++;
		}
		stats_.last_dc_time = ctx_.DCtime;

		next_time += interval;
		std::this_thread::sleep_until(next_time);
	}
}

void EcMaster::StateMonitorTask()
{
	while (running_) {
		ecx_readstate(&ctx_);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

bool EcMaster::StartCyclicThread()
{
	if (running_) {
		return false;
	}
	running_ = true;
	cyclic_thread_ = std::thread(&EcMaster::CyclicTask, this);
	monitor_thread_ = std::thread(&EcMaster::StateMonitorTask, this);
	return true;
}

bool EcMaster::StopCyclicThread()
{
	if (!running_) {
		return true;
	}
	running_ = false;
	if (cyclic_thread_.joinable()) {
		cyclic_thread_.join();
	}
	if (monitor_thread_.joinable()) {
		monitor_thread_.join();
	}
	return true;
}

void EcMaster::WriteOutput(int slave, int offset, const uint8_t *data, int len)
{
	if (ctx_.slavelist[slave].outputs == nullptr) {
		return;
	}
	std::memcpy(ctx_.slavelist[slave].outputs + offset, data, len);
}

void EcMaster::ReadInput(int slave, int offset, uint8_t *data, int len)
{
	if (ctx_.slavelist[slave].inputs == nullptr) {
		return;
	}
	std::memcpy(data, ctx_.slavelist[slave].inputs + offset, len);
}

uint8_t *EcMaster::GetGroupOutputs()
{
	return ctx_.grouplist[0].outputs;
}

uint8_t *EcMaster::GetGroupInputs()
{
	return ctx_.grouplist[0].inputs;
}

const CyclicStats &EcMaster::GetStats() const
{
	return stats_;
}

} // namespace mo_ecat