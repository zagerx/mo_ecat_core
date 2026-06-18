#pragma once

#include <cstdint>
#include <string>

#include "activity/activity.h"

namespace mo_ecat
{

class SlaveNode;

// SDO 诊断 Activity
// 读取从站关键对象字典，验证 SDO 通信并输出设备信息。
// 不改变从站状态，失败时只记录日志，不进入 Error。
class SdoDiagnosticsActivity : public EcatActivity
{
      public:
	explicit SdoDiagnosticsActivity(SlaveNode &node);

	const char *GetName() const override;
	bool CanStart(ControllerState state) const override;
	bool Execute() override;
	ActivityFailurePolicy GetFailurePolicy() const override;

      private:
	bool ReadDeviceType(uint32_t &device_type);
	bool ReadIdentity(uint32_t &vendor_id, uint32_t &product_code,
			  uint32_t &revision_number);

	SlaveNode &node_;
};

} // namespace mo_ecat
