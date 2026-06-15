#pragma once

#include "ec_master/ec_master.h"

namespace mo_ecat
{

class EcatController
{
public:
	EcatController();
	~EcatController();

	// 初始化 EtherCAT 主站：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置
	bool Initialize(const EcMasterConfig &config);

	// 启动周期通信：进入 SAFE_OP → 启动周期线程 → 进入 OPERATIONAL
	bool Start();

	// 安全停止：停止周期线程 → 进入 SAFE_OP → 进入 INIT
	void Stop();

	bool IsInitialized() const;
	bool IsRunning() const;

	EcMaster &GetMaster();

private:
	EcMaster master_;
	bool initialized_ = false;
	bool running_ = false;
};

} // namespace mo_ecat
