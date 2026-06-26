#include "activity/sdo_diagnostics_activity.h"

#include <iomanip>
#include <sstream>

#include "slave_node/slave_node.h"
#include "utils/logger.h"

namespace mo_ecat
{

SdoDiagnosticsActivity::SdoDiagnosticsActivity(SlaveNode &node) : node_(node)
{
}

const char *SdoDiagnosticsActivity::GetName() const
{
	return "SdoDiagnostics";
}

bool SdoDiagnosticsActivity::CanStart(const MasterRuntimeState &state) const
{
	return CanRunMaintenanceActivity(state);
}

ActivityFailurePolicy SdoDiagnosticsActivity::GetFailurePolicy() const
{
	// 诊断失败不破坏主站状态，只记录日志
	return ActivityFailurePolicy::kKeepControllerState;
}

bool SdoDiagnosticsActivity::Execute()
{
	const auto &info = node_.GetInfo();
	LOG_INFO << "SdoDiagnostics started for slave " << info.slave_id << " ["
		 << info.name << "]";

	uint32_t device_type = 0;
	uint32_t vendor_id = 0;
	uint32_t product_code = 0;
	uint32_t revision_number = 0;

	bool ok = true;

	if (!ReadDeviceType(device_type)) {
		LOG_ERROR << "Failed to read device type from slave " << info.slave_id;
		ok = false;
	}

	if (!ReadIdentity(vendor_id, product_code, revision_number)) {
		LOG_ERROR << "Failed to read identity from slave " << info.slave_id;
		ok = false;
	}

	if (ok) {
		LOG_INFO << "SdoDiagnostics passed for slave " << info.slave_id << " ["
			 << info.name << "]:"
			 << " device_type=0x" << std::hex << device_type << std::dec
			 << ", vendor_id=0x" << std::hex << vendor_id << std::dec
			 << ", product_code=0x" << std::hex << product_code << std::dec
			 << ", revision=0x" << std::hex << revision_number << std::dec;
	} else {
		LOG_WARN << "SdoDiagnostics failed for slave " << info.slave_id << " ["
			 << info.name << "]";
	}

	return ok;
}

bool SdoDiagnosticsActivity::ReadDeviceType(uint32_t &device_type)
{
	return node_.SdoRead(0x1000, 0x00, &device_type, sizeof(device_type), 10000);
}

bool SdoDiagnosticsActivity::ReadIdentity(uint32_t &vendor_id, uint32_t &product_code,
					  uint32_t &revision_number)
{
	bool ok = true;
	ok &= node_.SdoRead(0x1018, 0x01, &vendor_id, sizeof(vendor_id), 10000);
	ok &= node_.SdoRead(0x1018, 0x02, &product_code, sizeof(product_code), 10000);
	ok &= node_.SdoRead(0x1018, 0x03, &revision_number, sizeof(revision_number),
			    10000);
	return ok;
}

} // namespace mo_ecat
