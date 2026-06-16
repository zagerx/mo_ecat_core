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
	bool Initialize(EcMaster &master, int slave_count);

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
