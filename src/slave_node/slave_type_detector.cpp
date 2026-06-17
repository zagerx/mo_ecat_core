#include "slave_node/slave_type_detector.h"

#include <algorithm>
#include <cctype>

#include "utils/logger.h"

namespace mo_ecat
{

namespace
{

std::string ToLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		       [](unsigned char c) { return std::tolower(c); });
	return s;
}

} // namespace

SlaveTypeDetector::SlaveTypeDetector()
{
	// 可在此扩展常见设备识别表。
	// 示例规则（占位，后续根据实际硬件填充真实 vendor/product）：
	// RegisterRule(0x00000002, 0x00000001, SlaveType::Servo);
}

void SlaveTypeDetector::RegisterRule(uint32_t vendor_id, uint32_t product_id, SlaveType type)
{
	rules_.push_back({vendor_id, product_id, type});
}

SlaveType SlaveTypeDetector::Detect(const SlaveInfo &info) const
{
	// 1. 精确匹配 vendor_id + product_id
	for (const auto &rule : rules_) {
		if (rule.vendor_id != 0 && rule.vendor_id != info.vendor_id) {
			continue;
		}
		if (rule.product_id != 0 && rule.product_id != info.product_id) {
			continue;
		}
		return rule.type;
	}

	// 2. 按 name 关键字兜底判断
	const std::string name_lower = ToLower(info.name);
	if (name_lower.find("servo") != std::string::npos ||
	    name_lower.find("motor") != std::string::npos ||
	    name_lower.find("drive") != std::string::npos) {
		return SlaveType::Servo;
	}
	if (name_lower.find("digital") != std::string::npos ||
	    name_lower.find("dio") != std::string::npos) {
		return SlaveType::DigitalIo;
	}
	if (name_lower.find("analog") != std::string::npos ||
	    name_lower.find("aio") != std::string::npos) {
		return SlaveType::AnalogIo;
	}

	return SlaveType::Unknown;
}

} // namespace mo_ecat
