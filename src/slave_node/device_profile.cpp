#include "slave_node/device_profile.h"

#include <algorithm>
#include <cstring>

#include "utils/logger.h"

namespace mo_ecat
{

namespace
{

const PdoEntry *FindEntry(const std::vector<PdoEntry> &entries, const std::string &name)
{
	for (const auto &entry : entries) {
		if (entry.name == name) {
			return &entry;
		}
	}
	return nullptr;
}

template <typename T>
bool WriteRaw(std::vector<uint8_t> &buffer, const PdoEntry *entry, const T &value)
{
	if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
		return false;
	}
	std::memcpy(buffer.data() + entry->offset, &value, sizeof(T));
	return true;
}

template <typename T>
bool ReadRaw(const std::vector<uint8_t> &buffer, const PdoEntry *entry, T &value)
{
	if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
		return false;
	}
	std::memcpy(&value, buffer.data() + entry->offset, sizeof(T));
	return true;
}

} // namespace

DeviceProfile::~DeviceProfile() = default;

std::unique_ptr<DeviceProfile> CreateDeviceProfile(SlaveType type,
						   const ServoAxis::Config &axis_config)
{
	switch (type) {
	case SlaveType::Servo:
		return std::make_unique<ServoProfile>(axis_config);
	case SlaveType::DigitalIo:
		return std::make_unique<DigitalIoProfile>();
	case SlaveType::AnalogIo:
		return std::make_unique<AnalogIoProfile>();
	default:
		return std::make_unique<UnknownProfile>();
	}
}

// ---------- ServoProfile ----------

ServoProfile::ServoProfile(const ServoAxis::Config &config) : axis_(config)
{
}

SlaveType ServoProfile::GetType() const
{
	return SlaveType::Servo;
}

ServoAxis &ServoProfile::GetAxis()
{
	return axis_;
}

void ServoProfile::EncodePdoOutput(const PdoMapping &mapping, std::vector<uint8_t> &output_buffer)
{
	const auto &cfg = axis_.GetConfig();

	int32_t target_position_raw = 0;
	int32_t target_velocity_raw = 0;
	int16_t target_torque_raw = 0;

	if (cfg.position_scale != 0.0) {
		target_position_raw = static_cast<int32_t>(axis_.GetTargetPosition() /
							   cfg.position_scale * cfg.direction);
	}
	if (cfg.velocity_scale != 0.0) {
		target_velocity_raw = static_cast<int32_t>(axis_.GetTargetVelocity() /
							   cfg.velocity_scale * cfg.direction);
	}
	if (cfg.torque_scale != 0.0) {
		target_torque_raw = static_cast<int16_t>(axis_.GetTargetTorque() /
							 cfg.torque_scale * cfg.direction);
	}

	WriteRaw(output_buffer, FindEntry(mapping.outputs, "target_position"), target_position_raw);
	WriteRaw(output_buffer, FindEntry(mapping.outputs, "target_velocity"), target_velocity_raw);
	WriteRaw(output_buffer, FindEntry(mapping.outputs, "target_torque"), target_torque_raw);
}

void ServoProfile::DecodePdoInput(const PdoMapping &mapping,
				  const std::vector<uint8_t> &input_buffer)
{
	const auto &cfg = axis_.GetConfig();

	int32_t position_raw = 0;
	int32_t velocity_raw = 0;
	int16_t torque_raw = 0;

	ReadRaw(input_buffer, FindEntry(mapping.inputs, "position_actual_value"), position_raw);
	ReadRaw(input_buffer, FindEntry(mapping.inputs, "velocity_actual_value"), velocity_raw);
	ReadRaw(input_buffer, FindEntry(mapping.inputs, "torque_actual_value"), torque_raw);

	double position = static_cast<double>(position_raw) * cfg.position_scale * cfg.direction;
	double velocity = static_cast<double>(velocity_raw) * cfg.velocity_scale * cfg.direction;
	double torque = static_cast<double>(torque_raw) * cfg.torque_scale * cfg.direction;

	// 当前框架不解析 CiA402 status_word，enabled/fault 暂时固定为 false
	axis_.UpdateCommandReadback(position, velocity, torque, false, false);
}

// ---------- DigitalIoProfile ----------

DigitalIoProfile::DigitalIoProfile() = default;

SlaveType DigitalIoProfile::GetType() const
{
	return SlaveType::DigitalIo;
}

void DigitalIoProfile::EncodePdoOutput(const PdoMapping & /*mapping*/,
				       std::vector<uint8_t> & /*output_buffer*/)
{
	// 框架保留，后续实现数字量输出
}

void DigitalIoProfile::DecodePdoInput(const PdoMapping & /*mapping*/,
				      const std::vector<uint8_t> & /*input_buffer*/)
{
	// 框架保留，后续实现数字量输入
}

// ---------- AnalogIoProfile ----------

AnalogIoProfile::AnalogIoProfile() = default;

SlaveType AnalogIoProfile::GetType() const
{
	return SlaveType::AnalogIo;
}

void AnalogIoProfile::EncodePdoOutput(const PdoMapping & /*mapping*/,
				      std::vector<uint8_t> & /*output_buffer*/)
{
	// 框架保留，后续实现模拟量输出
}

void AnalogIoProfile::DecodePdoInput(const PdoMapping & /*mapping*/,
				     const std::vector<uint8_t> & /*input_buffer*/)
{
	// 框架保留，后续实现模拟量输入
}

// ---------- UnknownProfile ----------

UnknownProfile::UnknownProfile() = default;

SlaveType UnknownProfile::GetType() const
{
	return SlaveType::Unknown;
}

void UnknownProfile::EncodePdoOutput(const PdoMapping & /*mapping*/,
				     std::vector<uint8_t> & /*output_buffer*/)
{
	// 未知设备不做语义转换
}

void UnknownProfile::DecodePdoInput(const PdoMapping & /*mapping*/,
				    const std::vector<uint8_t> & /*input_buffer*/)
{
	// 未知设备不做语义转换
}

} // namespace mo_ecat
