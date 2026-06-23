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

// 单个从站节点。
// 生命周期分为两阶段：
//   1. 占位阶段：扫描后创建，只保存 SlaveInfo，支持 SDO 和状态查询。
//   2. 配置阶段：PDO 阶段调用 Configure()，填充 SlaveConfig、分配 buffer、创建 DeviceProfile。
class SlaveNode
{
      public:
	SlaveNode(EcMaster &master, const SlaveInfo &info);

	// ---------- 配置阶段 ----------
	// 填充 SlaveConfig、分配 PDO buffer、创建 DeviceProfile。
	// 当前阶段（PREOP + SDO）不调用；PDO 阶段再调用。
	bool Configure(const SlaveConfig &config);
	bool IsConfigured() const;

	// ---------- EtherCAT 通信状态 ----------
	bool RequestState(uint16_t state);
	uint16_t GetCurrentState() const;

	// 从主站刷新并返回该从站的真实状态
	uint16_t RefreshActualState();

	// 实时读取该从站的 AL status code
	uint16_t ReadAlStatusCode();

	// ---------- SDO 通信 ----------
	bool SdoRead(uint16_t index, uint8_t subindex, void *data, size_t len,
		     int timeout_us = 10000);
	bool SdoWrite(uint16_t index, uint8_t subindex, const void *data, size_t len,
		      int timeout_us = 10000);

	// ---------- PDO 映射配置（PDO 阶段使用）----------
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
	SlaveInfo info_;

	std::unique_ptr<SlaveConfig> config_;
	std::unique_ptr<DeviceProfile> profile_;
	std::vector<uint8_t> output_buffer_;
	std::vector<uint8_t> input_buffer_;

	bool configured_ = false;
	uint16_t current_state_ = 0; // EC_STATE_INIT
};

} // namespace mo_ecat
