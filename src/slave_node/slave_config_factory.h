#pragma once

#include <string>

#include "ec_master/ec_master.h"
#include "slave_node/slave_node_types.h"

namespace mo_ecat
{

// 根据识别到的从站类型生成对应的 SlaveConfig。
// 包含默认 PDO 映射与业务参数（如 ServoAxis 的缩放系数）。
class SlaveConfigFactory
{
      public:
	SlaveConfigFactory();

	// 生成配置
	SlaveConfig CreateConfig(const SlaveInfo &info, SlaveType type) const;

	// 设置伺服默认缩放系数
	void SetServoDefaultScale(double position_scale, double velocity_scale,
				  double torque_scale);

      private:
	SlaveConfig MakeServoConfig(const std::string &name) const;
	SlaveConfig MakeDigitalIoConfig(const std::string &name) const;
	SlaveConfig MakeAnalogIoConfig(const std::string &name) const;
	SlaveConfig MakeUnknownConfig(const std::string &name) const;

	double servo_position_scale_ = 0.001;
	double servo_velocity_scale_ = 0.001;
	double servo_torque_scale_ = 0.001;
};

} // namespace mo_ecat
