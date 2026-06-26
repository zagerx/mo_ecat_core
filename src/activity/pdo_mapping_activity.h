#pragma once

#include <cstdint>
#include <vector>

#include "activity/activity.h"

namespace mo_ecat
{

class SlaveNode;

// 读取并打印从站 PDO 映射信息的 Activity。
// 通过 SDO 读取 0x1C12 / 0x1C13 / 0x1600~ / 0x1A00~。
class PdoMappingActivity : public EcatActivity
{
public:
	explicit PdoMappingActivity(SlaveNode &node);

	const char *GetName() const override;
	bool CanStart(const MasterRuntimeState &state) const override;
	ActivityFailurePolicy GetFailurePolicy() const override;
	bool Execute() override;

private:
	// 读取 Sync Manager 分配对象（0x1C12 或 0x1C13），返回分配的 PDO 索引列表。
	bool ReadSmAssignment(uint16_t assign_index, std::vector<uint16_t> &pdo_indices);

	// 读取单个 PDO 映射对象（0x1600~ 或 0x1A00~），返回条目列表。
	bool ReadPdoMapping(uint16_t pdo_index, std::vector<uint32_t> &entries);

	// 打印某一方向的 PDO 映射。
	bool LogPdoMapping(const char *direction, uint16_t assign_index);

	SlaveNode &node_;
};

} // namespace mo_ecat
