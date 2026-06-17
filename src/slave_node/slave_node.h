#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "device_profile.h"
#include "ec_master/ec_master.h"
#include "slave_node_types.h"

namespace mo_ecat
{

// 单个从站节点：负责 EtherCAT 通信状态、PDO 映射、SDO，
// 并挂载对应的 DeviceProfile 处理具体设备语义。
class SlaveNode
{
      public:
	SlaveNode(EcMaster &master, const SlaveConfig &config, const SlaveInfo &info);

	// ---------- EtherCAT 通信状态 ----------
	bool RequestState(uint16_t state);
	uint16_t GetCurrentState() const;

	// ---------- SDO 通信（当前为框架，功能后续实现） ----------
	bool SdoRead(uint16_t index, uint8_t subindex, void *data, size_t len,
		     int timeout_us = 10000);
	bool SdoWrite(uint16_t index, uint8_t subindex, const void *data, size_t len,
		      int timeout_us = 10000);

	// ---------- PDO 映射配置 ----------
	bool ConfigurePdoMapping();
	void SetPdoMapping(const PdoMapping &mapping);

	// ---------- 周期调用：PDO 数据 <=> DeviceProfile 语义 ----------
	void UpdatePdoOutput();
	void UpdatePdoInput();

	// ---------- 访问接口 ----------
	SlaveType GetType() const;
	DeviceProfile *GetProfile() const;
	ServoAxis *GetServoAxis() const;

	const SlaveConfig &GetConfig() const;
	const SlaveInfo &GetInfo() const;

      private:
	static size_t ComputeOutputSize(const PdoMapping &mapping);
	static size_t ComputeInputSize(const PdoMapping &mapping);

	EcMaster &master_;
	SlaveConfig config_;
	SlaveInfo info_;

	std::vector<uint8_t> output_buffer_;
	std::vector<uint8_t> input_buffer_;

	std::unique_ptr<DeviceProfile> profile_;
	uint16_t current_state_ = 0; // EC_STATE_INIT
};

} // namespace mo_ecat
