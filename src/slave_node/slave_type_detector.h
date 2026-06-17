#pragma once

#include "ec_master/ec_master.h"
#include "slave_node/slave_node_types.h"

namespace mo_ecat
{

// 根据从站静态信息识别设备类型。
// 当前使用 vendor_id + product_id 主键表，并结合 name 关键字做兜底判断。
class SlaveTypeDetector
{
      public:
	SlaveTypeDetector();

	// 根据 SlaveInfo 返回识别结果
	SlaveType Detect(const SlaveInfo &info) const;

	// 注册一个自定义识别规则（vendor_id == 0 表示通配）
	void RegisterRule(uint32_t vendor_id, uint32_t product_id, SlaveType type);

      private:
	struct Rule {
		uint32_t vendor_id;
		uint32_t product_id;
		SlaveType type;
	};

	std::vector<Rule> rules_;
};

} // namespace mo_ecat
