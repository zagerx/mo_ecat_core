#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"
#include "servo_axis/servo_axis.h"

namespace mo_ecat
{

// 管理所有从站对应的 ServoAxis。
// 当前只负责创建、索引、批量更新 PDO，不做 CiA402 状态机。
class JointManager
{
      public:
	JointManager() = default;
	~JointManager() = default;

	// 根据从站数量创建默认配置的 ServoAxis
	bool Initialize(EcMaster &master, int slave_count);

	// 清空所有轴
	void Clear();

	size_t GetAxisCount() const;
	ServoAxis *GetAxis(size_t index);
	ServoAxis *GetAxis(const std::string &name);

	// 周期调用：批量更新所有轴的 PDO
	void UpdateAllOutputs();
	void UpdateAllInputs();

      private:
	std::vector<std::unique_ptr<ServoAxis>> axes_;
};

} // namespace mo_ecat
