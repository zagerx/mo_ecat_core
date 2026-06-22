#pragma once

#include <cstdint>
#include <string>

#include "activity/activity.h"

namespace mo_ecat
{

class SlaveNode;

// SDO 参数写入 Activity
// 向指定从站写入一个 32 位参数，可选回读校验。
// 失败策略默认 kEnterError，因为参数写入失败可能意味着通信或配置异常。
class SdoParameterActivity : public EcatActivity
{
public:
	SdoParameterActivity(SlaveNode &node, uint16_t index, uint8_t subindex,
			     uint32_t value, bool verify = true);

	const char *GetName() const override;
	bool CanStart(ControllerState state) const override;
	bool Execute() override;
	ActivityFailurePolicy GetFailurePolicy() const override;

private:
	bool WriteParameter();
	bool ReadBackAndVerify(uint32_t &read_back);

	SlaveNode &node_;
	uint16_t index_;
	uint8_t subindex_;
	uint32_t value_;
	bool verify_;
};

} // namespace mo_ecat
