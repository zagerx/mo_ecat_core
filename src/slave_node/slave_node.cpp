#include "slave_node/slave_node.h"

#include <algorithm>

#include "utils/logger.h"

namespace mo_ecat
{

namespace
{

PdoEntry MakeOutputEntry(const std::string &name, int offset, int size)
{
	return {name, offset, size};
}

PdoEntry MakeInputEntry(const std::string &name, int offset, int size)
{
	return {name, offset, size};
}

} // namespace

SlaveConfig MakeDefaultSlaveConfig(int slave_id)
{
	SlaveConfig cfg;
	cfg.slave_id = slave_id;
	cfg.name = "axis_" + std::to_string(slave_id);

	// 默认 PDO 映射：只包含位置/速度/力矩数据，不含 CiA402 控制字/状态字
	cfg.pdo_mapping.outputs = {
		MakeOutputEntry("target_position", 0, 4),
		MakeOutputEntry("target_velocity", 4, 4),
		MakeOutputEntry("target_torque", 8, 2),
	};

	cfg.pdo_mapping.inputs = {
		MakeInputEntry("position_actual_value", 0, 4),
		MakeInputEntry("velocity_actual_value", 4, 4),
		MakeInputEntry("torque_actual_value", 8, 2),
	};

	cfg.axis_config.name = cfg.name;
	return cfg;
}

SlaveNode::SlaveNode(EcMaster &master, const SlaveConfig &config)
	: master_(master), config_(config), axis_(config.axis_config)
{
	output_buffer_.resize(ComputeBufferSize(config_.pdo_mapping.outputs), 0);
	input_buffer_.resize(ComputeBufferSize(config_.pdo_mapping.inputs), 0);
}

const SlaveConfig &SlaveNode::GetConfig() const
{
	return config_;
}

ServoAxis &SlaveNode::GetAxis()
{
	return axis_;
}

const PdoEntry *SlaveNode::FindEntry(const std::vector<PdoEntry> &entries, const std::string &name)
{
	for (const auto &entry : entries) {
		if (entry.name == name) {
			return &entry;
		}
	}
	return nullptr;
}

size_t SlaveNode::ComputeBufferSize(const std::vector<PdoEntry> &entries)
{
	size_t size = 0;
	for (const auto &entry : entries) {
		size = std::max(size, static_cast<size_t>(entry.offset + entry.size));
	}
	return size;
}

bool SlaveNode::RequestState(uint16_t state)
{
	if (!master_.RequestState(config_.slave_id, state)) {
		LOG_WARN << "SlaveNode " << config_.name << " failed to request state " << state;
		return false;
	}
	current_state_ = state;
	return true;
}

uint16_t SlaveNode::GetCurrentState() const
{
	return master_.GetCurrentState(config_.slave_id);
}

bool SlaveNode::SdoRead(uint16_t index, uint8_t subindex, void *data, size_t len, int timeout_us)
{
	return master_.SdoRead(static_cast<uint16_t>(config_.slave_id), index, subindex, data,
			       static_cast<int>(len), timeout_us);
}

bool SlaveNode::SdoWrite(uint16_t index, uint8_t subindex, const void *data, size_t len,
			 int timeout_us)
{
	return master_.SdoWrite(static_cast<uint16_t>(config_.slave_id), index, subindex, data,
				static_cast<int>(len), timeout_us);
}

bool SlaveNode::ConfigurePdoMapping()
{
	// 框架阶段，后续通过 SDO 配置 PDO 映射
	LOG_INFO << "SlaveNode " << config_.name
		 << " ConfigurePdoMapping() called (framework only)";
	return true;
}

void SlaveNode::SetPdoMapping(const PdoMapping &mapping)
{
	config_.pdo_mapping = mapping;
	output_buffer_.resize(ComputeBufferSize(mapping.outputs), 0);
	input_buffer_.resize(ComputeBufferSize(mapping.inputs), 0);
}

void SlaveNode::UpdatePdoOutput()
{
	const auto &axis_cfg = axis_.GetConfig();

	int32_t target_position_raw = 0;
	int32_t target_velocity_raw = 0;
	int16_t target_torque_raw = 0;

	if (axis_cfg.position_scale != 0.0) {
		target_position_raw = static_cast<int32_t>(
			axis_.GetTargetPosition() / axis_cfg.position_scale * axis_cfg.direction);
	}
	if (axis_cfg.velocity_scale != 0.0) {
		target_velocity_raw = static_cast<int32_t>(
			axis_.GetTargetVelocity() / axis_cfg.velocity_scale * axis_cfg.direction);
	}
	if (axis_cfg.torque_scale != 0.0) {
		target_torque_raw = static_cast<int16_t>(
			axis_.GetTargetTorque() / axis_cfg.torque_scale * axis_cfg.direction);
	}

	WriteOutputRaw("target_position", target_position_raw);
	WriteOutputRaw("target_velocity", target_velocity_raw);
	WriteOutputRaw("target_torque", target_torque_raw);

	if (!output_buffer_.empty()) {
		master_.WriteOutput(config_.slave_id, 0, output_buffer_.data(),
				    static_cast<int>(output_buffer_.size()));
	}
}

void SlaveNode::UpdatePdoInput()
{
	if (!input_buffer_.empty()) {
		master_.ReadInput(config_.slave_id, 0, input_buffer_.data(),
				  static_cast<int>(input_buffer_.size()));
	}

	const auto &axis_cfg = axis_.GetConfig();

	int32_t position_raw = 0;
	int32_t velocity_raw = 0;
	int16_t torque_raw = 0;

	ReadInputRaw("position_actual_value", position_raw);
	ReadInputRaw("velocity_actual_value", velocity_raw);
	ReadInputRaw("torque_actual_value", torque_raw);

	double position =
		static_cast<double>(position_raw) * axis_cfg.position_scale * axis_cfg.direction;
	double velocity =
		static_cast<double>(velocity_raw) * axis_cfg.velocity_scale * axis_cfg.direction;
	double torque =
		static_cast<double>(torque_raw) * axis_cfg.torque_scale * axis_cfg.direction;

	// 当前框架不解析 CiA402 status_word，enabled/fault 暂时固定为 false
	axis_.UpdateCommandReadback(position, velocity, torque, false, false);
}

} // namespace mo_ecat
