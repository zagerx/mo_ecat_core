#pragma once
#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

#include "soem/soem.h"

namespace mo_ecat
{

struct EcMasterConfig {
	std::string ifname = "eth0";
	int cycle_time_us = 1000;
	bool use_dc = true;
};

struct CyclicStats {
	uint32_t cycle_count = 0;
	uint32_t wkc_mismatch_count = 0;
	int64_t last_dc_time = 0;
};

class EcMaster
{
      public:
	EcMaster();
	~EcMaster();

	bool Initialize(const EcMasterConfig &config);
	void Close();

	bool ScanAndConfigure();
	int GetSlaveCount() const;

	bool RequestOperationalState();
	bool RequestSafeOpState();
	bool RequestInitState();

	bool StartCyclicThread();
	bool StopCyclicThread();

	// PDO 读写（按字节偏移）
	void WriteOutput(int slave, int offset, const uint8_t *data, int len);
	void ReadInput(int slave, int offset, uint8_t *data, int len);

	// 直接读写 IOmap（更高效）
	uint8_t *GetGroupOutputs();
	uint8_t *GetGroupInputs();

	const CyclicStats &GetStats() const;

      private:
	void CyclicTask();
	void StateMonitorTask();

	ecx_contextt ctx_{};
	uint8_t iomap_[4096] = {0};

	EcMasterConfig config_;
	std::atomic<bool> running_{false};
	std::thread cyclic_thread_;
	std::thread monitor_thread_;

	int expected_wkc_ = 0;
	CyclicStats stats_;
};

} // namespace mo_ecat