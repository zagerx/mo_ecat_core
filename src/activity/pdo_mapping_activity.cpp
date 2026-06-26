#include "activity/pdo_mapping_activity.h"

#include <iomanip>
#include <sstream>

#include "slave_node/slave_node.h"
#include "utils/logger.h"

namespace mo_ecat
{

PdoMappingActivity::PdoMappingActivity(SlaveNode &node) : node_(node)
{
}

const char *PdoMappingActivity::GetName() const
{
	return "PdoMapping";
}

bool PdoMappingActivity::CanStart(const MasterRuntimeState &state) const
{
	return CanRunMaintenanceActivity(state);
}

ActivityFailurePolicy PdoMappingActivity::GetFailurePolicy() const
{
	// 只读诊断，失败不破坏主站状态。
	return ActivityFailurePolicy::kKeepControllerState;
}

bool PdoMappingActivity::ReadSmAssignment(uint16_t assign_index,
					  std::vector<uint16_t> &pdo_indices)
{
	pdo_indices.clear();

	uint8_t count = 0;
	if (!node_.SdoRead(assign_index, 0x00, &count, sizeof(count), 10000)) {
		LOG_WARN << "Failed to read PDO assignment count at 0x" << std::hex
			 << assign_index << std::dec;
		return false;
	}

	for (uint8_t i = 1; i <= count; ++i) {
		uint16_t pdo_index = 0;
		if (!node_.SdoRead(assign_index, i, &pdo_index, sizeof(pdo_index), 10000)) {
			LOG_WARN << "Failed to read PDO assignment entry " << static_cast<int>(i)
				 << " at 0x" << std::hex << assign_index << std::dec;
			return false;
		}
		pdo_indices.push_back(pdo_index);
	}

	return true;
}

bool PdoMappingActivity::ReadPdoMapping(uint16_t pdo_index,
					std::vector<uint32_t> &entries)
{
	entries.clear();

	uint8_t count = 0;
	if (!node_.SdoRead(pdo_index, 0x00, &count, sizeof(count), 10000)) {
		LOG_WARN << "Failed to read PDO mapping count at 0x" << std::hex
			 << pdo_index << std::dec;
		return false;
	}

	for (uint8_t i = 1; i <= count; ++i) {
		uint32_t entry = 0;
		if (!node_.SdoRead(pdo_index, i, &entry, sizeof(entry), 10000)) {
			LOG_WARN << "Failed to read PDO mapping entry " << static_cast<int>(i)
				 << " at 0x" << std::hex << pdo_index << std::dec;
			return false;
		}
		entries.push_back(entry);
	}

	return true;
}

bool PdoMappingActivity::LogPdoMapping(const char *direction, uint16_t assign_index)
{
	std::vector<uint16_t> pdo_indices;
	if (!ReadSmAssignment(assign_index, pdo_indices)) {
		return false;
	}

	const auto &info = node_.GetInfo();
	std::ostringstream oss;
	oss << "PDO mapping for slave " << info.slave_id << " [" << info.name
	    << "] " << direction << " (0x" << std::hex << assign_index << std::dec
	    << "):";

	if (pdo_indices.empty()) {
		oss << "\n  (empty)";
		LOG_INFO << oss.str();
		return true;
	}

	for (uint16_t pdo_index : pdo_indices) {
		std::vector<uint32_t> entries;
		if (!ReadPdoMapping(pdo_index, entries)) {
			continue;
		}

		oss << "\n  PDO 0x" << std::hex << pdo_index << std::dec << ":";
		if (entries.empty()) {
			oss << " (empty)";
			continue;
		}

		for (size_t i = 0; i < entries.size(); ++i) {
			const uint32_t entry = entries[i];
			const uint16_t index = static_cast<uint16_t>((entry >> 16) & 0xFFFF);
			const uint8_t subindex = static_cast<uint8_t>((entry >> 8) & 0xFF);
			const uint8_t bit_length = static_cast<uint8_t>(entry & 0xFF);
			oss << "\n    [" << i << "] 0x" << std::hex << index << ":"
			    << static_cast<int>(subindex) << std::dec << " ("
			    << static_cast<int>(bit_length) << " bits)";
		}
	}

	LOG_INFO << oss.str();
	return true;
}

bool PdoMappingActivity::Execute()
{
	bool ok = true;
	ok &= LogPdoMapping("RxPDO", 0x1C12);
	ok &= LogPdoMapping("TxPDO", 0x1C13);
	return ok;
}

} // namespace mo_ecat
