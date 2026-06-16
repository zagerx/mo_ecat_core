#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"
#include "servo_node/servo_node.h"

namespace mo_ecat {

// 管理所有 ServoNode
class ServoNodeManager {
public:
    bool Initialize(EcMaster &master, int slave_count);

    size_t GetNodeCount() const;
    ServoNode *GetNode(size_t index);
    ServoNode *GetNode(const std::string &name);

    void UpdateAllOutputs();
    void UpdateAllInputs();

    bool RequestAllState(uint16_t state);

    void Clear();

private:
    std::vector<std::unique_ptr<ServoNode>> nodes_;
};

} // namespace mo_ecat
