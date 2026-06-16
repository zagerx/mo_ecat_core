#pragma once
#include <cstdint>
#include <mutex>
#include <string>

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

	// 全局状态切换（广播到所有从站）
	bool RequestOperationalState();
	bool RequestSafeOpState();
	bool RequestInitState();

	// 单站状态切换
	bool RequestState(int slave, uint16_t state);
	uint16_t GetCurrentState(int slave) const;

	// 单步运行：执行一次 PDO 收发
	void RunOneCycle();

	// 单步运行：检查一次从站状态
	void CheckSlaveStates();

	// PDO 读写（按字节偏移）
	void WriteOutput(int slave, int offset, const uint8_t *data, int len);
	void ReadInput(int slave, int offset, uint8_t *data, int len);

	// SDO 通信（当前为框架接口，功能可后续补齐）
	bool SdoRead(uint16_t slave, uint16_t index, uint8_t subindex, void *data, int len, int timeout_us);
	bool SdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, const void *data, int len, int timeout_us);

	const CyclicStats &GetStats() const;

      private:
	ecx_contextt ctx_{};
	uint8_t iomap_[4096] = {0};

	EcMasterConfig config_;

	int expected_wkc_ = 0;
	CyclicStats stats_;

	// 保护 SOEM 上下文，防止多线程同时访问
	mutable std::mutex soem_mutex_;
};

} // namespace mo_ecat
