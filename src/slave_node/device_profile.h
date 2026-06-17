#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "slave_node/slave_node_types.h"

namespace mo_ecat
{

// 设备业务抽象。
// SlaveNode 负责 EtherCAT 通信状态、SDO、PDO 映射；
// DeviceProfile 负责把 PDO 字节和具体设备语义互相转换。
class DeviceProfile
{
      public:
	virtual ~DeviceProfile();

	virtual SlaveType GetType() const = 0;

	// 周期调用：把业务命令编码进输出 buffer
	virtual void EncodePdoOutput(const PdoMapping &mapping,
				     std::vector<uint8_t> &output_buffer) = 0;

	// 周期调用：从输入 buffer 解码出业务反馈
	virtual void DecodePdoInput(const PdoMapping &mapping,
				    const std::vector<uint8_t> &input_buffer) = 0;
};

// 创建对应类型的 DeviceProfile
std::unique_ptr<DeviceProfile> CreateDeviceProfile(SlaveType type,
						   const ServoAxis::Config &axis_config);

// 伺服驱动器业务对象
class ServoProfile: public DeviceProfile
{
      public:
	explicit ServoProfile(const ServoAxis::Config &config);

	SlaveType GetType() const override;
	ServoAxis &GetAxis();

	void EncodePdoOutput(const PdoMapping &mapping,
			     std::vector<uint8_t> &output_buffer) override;

	void DecodePdoInput(const PdoMapping &mapping,
			    const std::vector<uint8_t> &input_buffer) override;

      private:
	ServoAxis axis_;
};

// 数字 IO 业务对象（当前为框架）
class DigitalIoProfile: public DeviceProfile
{
      public:
	DigitalIoProfile();

	SlaveType GetType() const override;

	void EncodePdoOutput(const PdoMapping &mapping,
			     std::vector<uint8_t> &output_buffer) override;

	void DecodePdoInput(const PdoMapping &mapping,
			    const std::vector<uint8_t> &input_buffer) override;
};

// 模拟量 IO 业务对象（当前为框架）
class AnalogIoProfile: public DeviceProfile
{
      public:
	AnalogIoProfile();

	SlaveType GetType() const override;

	void EncodePdoOutput(const PdoMapping &mapping,
			     std::vector<uint8_t> &output_buffer) override;

	void DecodePdoInput(const PdoMapping &mapping,
			    const std::vector<uint8_t> &input_buffer) override;
};

// 未知设备业务对象（不做任何语义转换）
class UnknownProfile: public DeviceProfile
{
      public:
	UnknownProfile();

	SlaveType GetType() const override;

	void EncodePdoOutput(const PdoMapping &mapping,
			     std::vector<uint8_t> &output_buffer) override;

	void DecodePdoInput(const PdoMapping &mapping,
			    const std::vector<uint8_t> &input_buffer) override;
};

} // namespace mo_ecat
