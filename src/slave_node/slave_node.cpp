#include "slave_node/slave_node.h"

#include <algorithm>

#include "utils/logger.h"

namespace mo_ecat
{

SlaveNode::SlaveNode(EcMaster &master, const SlaveConfig &config, const SlaveInfo &info)
	: master_(master), config_(config), info_(info)
{
	output_buffer_.resize(ComputeOutputSize(config_.mapping), 0);
	input_buffer_.resize(ComputeInputSize(config_.mapping), 0);

	profile_ = CreateDeviceProfile(config_.type, config_.axis_config);
}

const SlaveConfig &SlaveNode::GetConfig() const
{
	return config_;
}

const SlaveInfo &SlaveNode::GetInfo() const
{
	return info_;
}

SlaveType SlaveNode::GetType() const
{
	return config_.type;
}

DeviceProfile *SlaveNode::GetProfile() const
{
	return profile_.get();
}

ServoAxis *SlaveNode::GetServoAxis() const
{
	if (config_.type != SlaveType::Servo) {
		return nullptr;
	}
	auto *servo_profile = dynamic_cast<ServoProfile *>(profile_.get());
	if (servo_profile == nullptr) {
		return nullptr;
	}
	return &servo_profile->GetAxis();
}

size_t SlaveNode::ComputeOutputSize(const PdoMapping &mapping)
{
	size_t size = 0;
	for (const auto &entry : mapping.outputs) {
		size = std::max(size, static_cast<size_t>(entry.offset + entry.size));
	}
	if (size == 0) {
		size = static_cast<size_t>(mapping.output_bytes);
	}
	return size;
}

size_t SlaveNode::ComputeInputSize(const PdoMapping &mapping)
{
	size_t size = 0;
	for (const auto &entry : mapping.inputs) {
		size = std::max(size, static_cast<size_t>(entry.offset + entry.size));
	}
	if (size == 0) {
		size = static_cast<size_t>(mapping.input_bytes);
	}
	return size;
}

bool SlaveNode::RequestState(uint16_t state)
{
	if (!master_.RequestState(info_.slave_id, state)) {
		LOG_WARN << "SlaveNode " << info_.name << " failed to request state " << state;
		return false;
	}
	current_state_ = state;
	return true;
}

uint16_t SlaveNode::GetCurrentState() const
{
	return master_.GetCurrentState(info_.slave_id);
}

bool SlaveNode::SdoRead(uint16_t index, uint8_t subindex, void *data, size_t len, int timeout_us)
{
	return master_.SdoRead(static_cast<uint16_t>(info_.slave_id), index, subindex, data,
			       static_cast<int>(len), timeout_us);
}

bool SlaveNode::SdoWrite(uint16_t index, uint8_t subindex, const void *data, size_t len,
			 int timeout_us)
{
	return master_.SdoWrite(static_cast<uint16_t>(info_.slave_id), index, subindex, data,
				static_cast<int>(len), timeout_us);
}

bool SlaveNode::ConfigurePdoMapping()
{
	// 框架阶段，后续通过 SDO 配置 PDO 映射
	LOG_INFO << "SlaveNode " << info_.name << " ConfigurePdoMapping() called (framework only)";
	return true;
}

void SlaveNode::SetPdoMapping(const PdoMapping &mapping)
{
	config_.mapping = mapping;
	output_buffer_.resize(ComputeOutputSize(config_.mapping), 0);
	input_buffer_.resize(ComputeInputSize(config_.mapping), 0);
}

void SlaveNode::UpdatePdoOutput()
{
	if (profile_ != nullptr) {
		profile_->EncodePdoOutput(config_.mapping, output_buffer_);
	}

	if (!output_buffer_.empty()) {
		master_.WriteOutput(info_.slave_id, 0, output_buffer_.data(),
				    static_cast<int>(output_buffer_.size()));
	}
}

void SlaveNode::UpdatePdoInput()
{
	if (!input_buffer_.empty()) {
		master_.ReadInput(info_.slave_id, 0, input_buffer_.data(),
				  static_cast<int>(input_buffer_.size()));
	}

	if (profile_ != nullptr) {
		profile_->DecodePdoInput(config_.mapping, input_buffer_);
	}
}

} // namespace mo_ecat
