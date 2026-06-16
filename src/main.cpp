#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>

#include "cyclic/cyclic_runner.h"
#include "ec_controller/ec_controller.h"
#include "utils/logger.h"

namespace
{

std::atomic<bool> g_shutdown_requested{false};
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;

void OnSignal(int /*signum*/)
{
	g_shutdown_requested.store(true);
	g_shutdown_cv.notify_all();
}

void RegisterShutdownSignals()
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
}

void WaitForShutdown()
{
	std::unique_lock<std::mutex> lock(g_shutdown_mutex);
	g_shutdown_cv.wait(lock, []() { return g_shutdown_requested.load(); });
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc < 2) {
		LOG_ERROR << "Usage: " << argv[0] << " <interface>";
		return 1;
	}

	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	// 1. 初始化 EtherCAT：绑定网卡 + 扫描从站 + PDO 映射 + DC 配置 + 进入 SAFE_OP
	mo_ecat::EcatController controller;
	if (!controller.Initialize(config)) {
		return 1;
	}

	// 2. 创建并配置 PDO 周期线程（高优先级）
	mo_ecat::CyclicRunner pdo_runner(config.cycle_time_us);
	pdo_runner.SetRealtimePriority(80);
	if (!pdo_runner.Start([&controller]() { controller.RunOneCycle(); })) {
		controller.Stop();
		return 1;
	}

	// 3. 创建并配置从站状态监控线程（普通优先级）
	constexpr int kStateCheckPeriodUs = 10000; // 10ms
	mo_ecat::CyclicRunner state_runner(kStateCheckPeriodUs);
	if (!state_runner.Start([&controller]() { controller.CheckSlaveStates(); })) {
		pdo_runner.Stop();
		controller.Stop();
		return 1;
	}

	// 4. 进入 OPERATIONAL 状态
	if (!controller.StartOperation()) {
		pdo_runner.Stop();
		state_runner.Stop();
		controller.Stop();
		return 1;
	}

	// 5. 注册退出信号
	RegisterShutdownSignals();

	LOG_INFO << "System running on " << config.ifname << ". Press Ctrl+C to stop.";

	// 6. 主线程阻塞等待退出信号
	WaitForShutdown();

	// 7. 正常清理：先停线程，再停控制器
	pdo_runner.Stop();
	state_runner.Stop();
	controller.Stop();

	return 0;
}
