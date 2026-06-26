#include "activity/sdo_parameter_activity.h"

#include <iomanip>

#include "slave_node/slave_node.h"
#include "utils/logger.h"

namespace mo_ecat
{

SdoParameterActivity::SdoParameterActivity(SlaveNode &node, uint16_t index,
					   uint8_t subindex, uint32_t value,
					   bool verify)
	: node_(node), index_(index), subindex_(subindex), value_(value), verify_(verify)
{
}

const char *SdoParameterActivity::GetName() const
{
	return "SdoParameter";
}

bool SdoParameterActivity::CanStart(const MasterRuntimeState &state) const
{
	return CanRunMaintenanceActivity(state);
}

ActivityFailurePolicy SdoParameterActivity::GetFailurePolicy() const
{
	// 参数写入失败相对严重，默认进入 Error
	return ActivityFailurePolicy::kEnterError;
}

bool SdoParameterActivity::Execute()
{
	const auto &info = node_.GetInfo();
	LOG_INFO << "SdoParameter started for slave " << info.slave_id << " ["
		 << info.name << "]: 0x" << std::hex << index_ << ":"
		 << static_cast<int>(subindex_) << " = 0x" << value_ << std::dec;

	if (!WriteParameter()) {
		LOG_ERROR << "Failed to write parameter to slave " << info.slave_id;
		return false;
	}

	if (verify_) {
		uint32_t read_back = 0;
		if (!ReadBackAndVerify(read_back)) {
			LOG_ERROR << "Parameter verification failed for slave " << info.slave_id
				  << ": expected 0x" << std::hex << value_ << ", got 0x"
				  << read_back << std::dec;
			return false;
		}
		LOG_INFO << "SdoParameter verified for slave " << info.slave_id
			 << ": read_back=0x" << std::hex << read_back << std::dec;
	}

	LOG_INFO << "SdoParameter finished successfully for slave " << info.slave_id;
	return true;
}

bool SdoParameterActivity::WriteParameter()
{
	return node_.SdoWrite(index_, subindex_, &value_, sizeof(value_), 10000);
}

bool SdoParameterActivity::ReadBackAndVerify(uint32_t &read_back)
{
	if (!node_.SdoRead(index_, subindex_, &read_back, sizeof(read_back), 10000)) {
		return false;
	}
	return read_back == value_;
}

} // namespace mo_ecat
