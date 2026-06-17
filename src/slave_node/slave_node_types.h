#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "servo_axis/servo_axis.h"

namespace mo_ecat
{

// EtherCAT 从站类型
enum class SlaveType {
	Unknown,   // 未识别/不支持的设备
	Servo,     // 伺服驱动器（CiA402）
	DigitalIo, // 数字量 IO
	AnalogIo,  // 模拟量 IO
};

// PDO 条目描述
struct PdoEntry {
	std::string name; // 语义名称，例如 "target_position"
	int offset = 0;   // 在 PDO buffer 中的字节偏移
	int size = 0;     // 字节数
};

// 单个从站的 PDO 映射
struct PdoMapping {
	int output_bytes = 0;
	int input_bytes = 0;
	std::vector<PdoEntry> outputs;
	std::vector<PdoEntry> inputs;
};

// 从站配置：类型 + 业务参数
struct SlaveConfig {
	SlaveType type = SlaveType::Unknown;
	ServoAxis::Config axis_config;
	PdoMapping mapping;
};

} // namespace mo_ecat
