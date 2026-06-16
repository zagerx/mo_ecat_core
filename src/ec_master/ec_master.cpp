#include "ec_master/ec_master.h"

#include <cstring>

#include "utils/logger.h"

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
		LOG_ERROR << "ecx_init failed on " << config_.ifname << " (try sudo)";
		return false;
	}
	LOG_INFO << "SOEM initialized on " << config_.ifname;
	return true;
}

void EcMaster::Close()
{
	ecx_close(&ctx_);
	LOG_INFO << "SOEM closed";
}

bool EcMaster::ScanAndConfigure()
{
	int slave_count = ecx_config_init(&ctx_);
	if (slave_count <= 0) {
		LOG_ERROR << "No slaves found";
		return false;
	}

	ecx_config_map_group(&ctx_, iomap_, 0);
	expected_wkc_ = (ctx_.grouplist[0].outputsWKC * 2) + ctx_.grouplist[0].inputsWKC;

	if (config_.use_dc) {
		ecx_configdc(&ctx_);
	}

	LOG_INFO << slave_count << " slave(s) found and configured";
	LOG_INFO << "Outputs: " << ctx_.grouplist[0].Obytes
		 << " bytes, Inputs: " << ctx_.grouplist[0].Ibytes
		 << " bytes, Expected WKC: " << expected_wkc_;
	return true;
}

int EcMaster::GetSlaveCount() const
{
	return ctx_.slavecount;
}

bool EcMaster::RequestOperationalState()
{
	return RequestState(0, EC_STATE_OPERATIONAL);
}

bool EcMaster::RequestSafeOpState()
{
	return RequestState(0, EC_STATE_SAFE_OP);
}

bool EcMaster::RequestInitState()
{
	return RequestState(0, EC_STATE_INIT);
}

bool EcMaster::RequestState(int slave, uint16_t state)
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	ctx_.slavelist[slave].state = state;
	ecx_writestate(&ctx_, slave);
	ecx_statecheck(&ctx_, slave, state, EC_TIMEOUTSTATE);
	return ctx_.slavelist[slave].state == state;
}

uint16_t EcMaster::GetCurrentState(int slave) const
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	return ctx_.slavelist[slave].state;
}

void EcMaster::RunOneCycle()
{
	std::lock_guard<std::mutex> lock(soem_mutex_);

	ecx_send_processdata(&ctx_);
	int wkc = ecx_receive_processdata(&ctx_, EC_TIMEOUTRET);

	stats_.cycle_count++;
	if (wkc != expected_wkc_) {
		stats_.wkc_mismatch_count++;
	}
	stats_.last_dc_time = ctx_.DCtime;
}

void EcMaster::CheckSlaveStates()
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	ecx_readstate(&ctx_);
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

bool EcMaster::SdoRead(uint16_t slave, uint16_t index, uint8_t subindex, void *data, int len, int timeout_us)
{
    std::lock_guard<std::mutex> lock(soem_mutex_);
    int size = len;
    int wkc = ecx_SDOread(&ctx_, slave, index, subindex, FALSE, &size, data, timeout_us);
    return wkc > 0;
}

bool EcMaster::SdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, const void *data, int len, int timeout_us)
{
    std::lock_guard<std::mutex> lock(soem_mutex_);
    int wkc = ecx_SDOwrite(&ctx_, slave, index, subindex, FALSE, len, data, timeout_us);
    return wkc > 0;
}

const CyclicStats &EcMaster::GetStats() const
{
	return stats_;
}

} // namespace mo_ecat
