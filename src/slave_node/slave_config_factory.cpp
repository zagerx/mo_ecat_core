#include "slave_node/slave_config_factory.h"

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

SlaveConfigFactory::SlaveConfigFactory() = default;

void SlaveConfigFactory::SetServoDefaultScale(double position_scale, double velocity_scale,
					      double torque_scale)
{
	servo_position_scale_ = position_scale;
	servo_velocity_scale_ = velocity_scale;
	servo_torque_scale_ = torque_scale;
}

SlaveConfig SlaveConfigFactory::CreateConfig(const SlaveInfo &info, SlaveType type) const
{
	const std::string name =
		info.name.empty() ? ("slave_" + std::to_string(info.slave_id)) : info.name;

	switch (type) {
	case SlaveType::Servo:
		return MakeServoConfig(name);
	case SlaveType::DigitalIo:
		return MakeDigitalIoConfig(name);
	case SlaveType::AnalogIo:
		return MakeAnalogIoConfig(name);
	default:
		return MakeUnknownConfig(name);
	}
}

SlaveConfig SlaveConfigFactory::MakeServoConfig(const std::string &name) const
{
	SlaveConfig cfg;
	cfg.type = SlaveType::Servo;
	cfg.axis_config.name = name;
	cfg.axis_config.position_scale = servo_position_scale_;
	cfg.axis_config.velocity_scale = servo_velocity_scale_;
	cfg.axis_config.torque_scale = servo_torque_scale_;

	// 默认 PDO 映射：位置/速度/力矩数据，不含 CiA402 控制字/状态字
	cfg.mapping.output_bytes = 10;
	cfg.mapping.outputs = {
		MakeOutputEntry("target_position", 0, 4),
		MakeOutputEntry("target_velocity", 4, 4),
		MakeOutputEntry("target_torque", 8, 2),
	};

	cfg.mapping.input_bytes = 10;
	cfg.mapping.inputs = {
		MakeInputEntry("position_actual_value", 0, 4),
		MakeInputEntry("velocity_actual_value", 4, 4),
		MakeInputEntry("torque_actual_value", 8, 2),
	};

	return cfg;
}

SlaveConfig SlaveConfigFactory::MakeDigitalIoConfig(const std::string &name) const
{
	SlaveConfig cfg;
	cfg.type = SlaveType::DigitalIo;
	cfg.axis_config.name = name;

	cfg.mapping.output_bytes = 1;
	cfg.mapping.input_bytes = 1;
	// 具体条目后续根据实际硬件扩展

	return cfg;
}

SlaveConfig SlaveConfigFactory::MakeAnalogIoConfig(const std::string &name) const
{
	SlaveConfig cfg;
	cfg.type = SlaveType::AnalogIo;
	cfg.axis_config.name = name;

	cfg.mapping.output_bytes = 4;
	cfg.mapping.input_bytes = 4;
	// 具体条目后续根据实际硬件扩展

	return cfg;
}

SlaveConfig SlaveConfigFactory::MakeUnknownConfig(const std::string &name) const
{
	SlaveConfig cfg;
	cfg.type = SlaveType::Unknown;
	cfg.axis_config.name = name;

	cfg.mapping.output_bytes = 0;
	cfg.mapping.input_bytes = 0;

	return cfg;
}

} // namespace mo_ecat
