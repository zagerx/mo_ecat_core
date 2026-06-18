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

std::vector<SlaveInfo> EcMaster::ScanSlaves()
{
	std::vector<SlaveInfo> infos;

	int slave_count = ecx_config_init(&ctx_);
	if (slave_count <= 0) {
		LOG_ERROR << "No slaves found";
		return infos;
	}

	infos.reserve(slave_count);
	for (int i = 1; i <= slave_count; ++i) {
		infos.push_back(GetSlaveInfo(i));
	}

	LOG_INFO << slave_count << " slave(s) found";
	return infos;
}

bool EcMaster::ConfigureProcessData()
{
	if (ctx_.slavecount <= 0) {
		LOG_ERROR << "ConfigureProcessData called before slaves are scanned";
		return false;
	}

	const int iomap_bytes = ecx_config_map_group(&ctx_, iomap_, 0);
	expected_wkc_ = (ctx_.grouplist[0].outputsWKC * 2) + ctx_.grouplist[0].inputsWKC;

	if (expected_wkc_ <= 0) {
		LOG_WARN << "Process data configured but expected WKC is " << expected_wkc_;
	}

	LOG_INFO << "Outputs: " << ctx_.grouplist[0].Obytes
		 << " bytes, Inputs: " << ctx_.grouplist[0].Ibytes
		 << " bytes, IOmap: " << iomap_bytes << " bytes, Expected WKC: " << expected_wkc_;
	return true;
}

bool EcMaster::ConfigureDc()
{
	if (!config_.use_dc) {
		return true;
	}

	ecx_configdc(&ctx_);
	LOG_INFO << "Distributed Clock configured";
	return true;
}

int EcMaster::GetSlaveCount() const
{
	return ctx_.slavecount;
}

SlaveInfo EcMaster::GetSlaveInfo(int slave_id) const
{
	SlaveInfo info;
	if (slave_id <= 0 || slave_id > ctx_.slavecount) {
		LOG_ERROR << "Invalid slave_id " << slave_id << " for GetSlaveInfo";
		return info;
	}

	const auto &slave = ctx_.slavelist[slave_id];
	info.slave_id = slave_id;
	info.config_address = slave.configadr;
	info.alias_address = slave.aliasadr;
	info.vendor_id = slave.eep_man;
	info.product_id = slave.eep_id;
	info.revision_id = slave.eep_rev;
	info.serial_id = slave.eep_ser;
	info.name = slave.name;
	info.supports_dc = slave.hasdc != 0;

	info.mbx_l = slave.mbx_l;
	info.mbx_wo = slave.mbx_wo;
	info.mbx_ro = slave.mbx_ro;
	info.mbx_proto = slave.mbx_proto;
	info.mbx_cnt = slave.mbx_cnt;

	info.state = slave.state;
	info.al_status_code = slave.ALstatuscode;
	info.coe_details = slave.CoEdetails;
	info.output_bytes = slave.Obytes;
	info.input_bytes = slave.Ibytes;

	return info;
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
bool EcMaster::RequestPreOpState()
{
	return RequestStateWithRetry(0, EC_STATE_PRE_OP);
}

uint16_t EcMaster::ReadActualState(int slave)
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	ecx_readstate(&ctx_);
	return ctx_.slavelist[slave].state;
}

bool EcMaster::RequestStateWithRetry(int slave, uint16_t state, int max_retries)
{
	std::lock_guard<std::mutex> lock(soem_mutex_);

	for (int i = 0; i <= max_retries; ++i) {
		ecx_readstate(&ctx_);
		uint16_t actual = ctx_.slavelist[slave].state;

		if (actual == state) {
			return true;
		}

		if (actual & EC_STATE_ERROR) {
			ctx_.slavelist[slave].state = (actual & 0x0f) | EC_STATE_ACK;
			ecx_writestate(&ctx_, slave);
			ecx_statecheck(&ctx_, slave, (actual & 0x0f), EC_TIMEOUTSTATE);
			continue;
		}

		ctx_.slavelist[slave].state = state;
		ecx_writestate(&ctx_, slave);
		ecx_statecheck(&ctx_, slave, state, EC_TIMEOUTSTATE);

		if (ctx_.slavelist[slave].state == state) {
			return true;
		}
	}
	return false;
}

bool EcMaster::CheckAllSlavesInState(uint16_t state)
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	ecx_readstate(&ctx_);
	for (int i = 1; i <= ctx_.slavecount; ++i) {
		if (ctx_.slavelist[i].state != state) {
			LOG_ERROR << "Slave " << i << " [" << ctx_.slavelist[i].name << "] state=0x"
				  << std::hex << ctx_.slavelist[i].state << ", expected=0x"
				  << state;
			return false;
		}
	}
	return true;
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

bool EcMaster::SdoRead(uint16_t slave, uint16_t index, uint8_t subindex, void *data, int len,
		       int timeout_us)
{
	std::lock_guard<std::mutex> lock(soem_mutex_);
	int size = len;
	int wkc = ecx_SDOread(&ctx_, slave, index, subindex, FALSE, &size, data, timeout_us);
	return wkc > 0;
}

bool EcMaster::SdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, const void *data, int len,
			int timeout_us)
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
