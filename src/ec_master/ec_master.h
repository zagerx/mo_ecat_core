#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

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

// 从站静态信息（由 ecx_config_init 扫描后填充）
struct SlaveInfo {
	int slave_id = 0;
	uint16_t config_address = 0;
	uint16_t alias_address = 0;

	uint32_t vendor_id = 0;
	uint32_t product_id = 0;
	uint32_t revision_id = 0;
	uint32_t serial_id = 0;

	std::string name;
	bool supports_dc = false;

	// Mailbox 基本信息（从 SII/EEPROM 读取）
	uint16_t mbx_l = 0;     // 邮箱长度（字节），0 表示无邮箱
	uint16_t mbx_wo = 0;    // 邮箱写偏移
	uint16_t mbx_ro = 0;    // 邮箱读偏移
	uint16_t mbx_proto = 0; // 支持的邮箱协议位掩码
	uint8_t mbx_cnt = 0;    // 邮箱链路层计数器

	// 运行状态与能力（扫描时或 PDO/DC 配置后可用）
	uint16_t state = 0;          // 当前 EtherCAT 状态
	uint16_t al_status_code = 0; // AL 状态码
	uint8_t coe_details = 0;     // CoE 能力位
	uint32_t output_bytes = 0;   // 输出字节数（PDO 映射后有效）
	uint32_t input_bytes = 0;    // 输入字节数（PDO 映射后有效）
};

class EcMaster
{
      public:
	EcMaster();
	~EcMaster();

	bool Initialize(const EcMasterConfig &config);
	void Close();

	// 扫描从站，填充 ctx_.slavelist，返回所有从站的静态信息
	std::vector<SlaveInfo> ScanSlaves();

	// 配置 PDO 映射和 IOmap
	bool ConfigureProcessData();

	// 配置 Distributed Clock
	bool ConfigureDc();

	int GetSlaveCount() const;
	SlaveInfo GetSlaveInfo(int slave_id) const;

	// 全局状态切换（广播到所有从站）
	bool RequestOperationalState();
	bool RequestSafeOpState();
	bool RequestInitState();
	bool RequestPreOpState();
	bool RequestStateWithRetry(int slave, uint16_t state, int max_retries = 3);
	bool CheckAllSlavesInState(uint16_t state);
	uint16_t ReadActualState(int slave);

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

	// SDO 通信（基于 SOEM ecx_SDOread / ecx_SDOwrite）
	bool SdoRead(uint16_t slave, uint16_t index, uint8_t subindex, void *data, int len,
		     int timeout_us);
	bool SdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, const void *data, int len,
		      int timeout_us);

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
