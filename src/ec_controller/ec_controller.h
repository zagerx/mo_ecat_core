#pragma once

#include "ec_master/ec_master.h"

namespace mo_ecat
{

class EcatController
{
public:
	EcatController();
	~EcatController();

	// 初始化 EtherCAT 主站：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置 + 进入 SAFE_OP
	bool Initialize(const EcMasterConfig &config);

	// 进入 OPERATIONAL 状态（调用前应先启动周期线程）
	bool StartOperation();

	// 安全停止：进入 SAFE_OP + 进入 INIT
	void Stop();

	bool IsInitialized() const;
	bool IsOperational() const;

	EcMaster &GetMaster();

private:
	EcMaster master_;
	bool initialized_ = false;
	bool operational_ = false;
};

} // namespace mo_ecat
