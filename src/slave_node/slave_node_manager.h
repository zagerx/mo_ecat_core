#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"
#include "slave_node/slave_node.h"

namespace mo_ecat
{

// 管理所有 SlaveNode
class SlaveNodeManager
{
      public:
	// 创建占位 SlaveNode（PREOP 阶段）。此时只保存 SlaveInfo，不分配 PDO buffer。
	bool Initialize(EcMaster &master, const std::vector<SlaveInfo> &slave_infos);

	// 对所有从站调用 Configure()，进入 PDO 阶段后使用。
	// 如果节点已经配置过，则跳过。
	bool ConfigureAll();

	size_t GetNodeCount() const;
	SlaveNode *GetNode(size_t index);
	SlaveNode *GetNode(const std::string &name);

	void UpdateAllOutputs();
	void UpdateAllInputs();

	bool RequestAllState(uint16_t state);

	void Clear();

      private:
	std::vector<std::unique_ptr<SlaveNode>> nodes_;
};

} // namespace mo_ecat
