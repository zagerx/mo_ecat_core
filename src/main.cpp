#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>

#include "activity/sdo_diagnostics_activity.h"
#include "ec_controller/ec_controller.h"
#include "ec_master/ec_master.h"
#include "slave_node/slave_node.h"
#include "slave_node/slave_node_manager.h"
#include "utils/logger.h"

namespace
{

std::atomic<bool> g_shutdown_requested{false};
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;

void OnSignal(int)
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

	mo_ecat::EcatController controller;
	if (!controller.Initialize(config)) {
		return 1;
	}

	// 当前阶段：初始化到 PREOP 即完成，SDO 可用
	// PDO 通信（StartOperation）放到后续阶段再启用
	LOG_INFO << "System initialized, slaves in PREOP on " << config.ifname;

	// 对每个从站执行 SDO 诊断，验证通信
	mo_ecat::SlaveNodeManager &node_manager = controller.GetSlaveNodeManager();
	for (size_t i = 0; i < node_manager.GetNodeCount(); ++i) {
		mo_ecat::SlaveNode *node = node_manager.GetNode(i);
		if (node != nullptr) {
			controller.ExecuteActivity(
				std::make_unique<mo_ecat::SdoDiagnosticsActivity>(*node));
		}
	}

	RegisterShutdownSignals();
	LOG_INFO << "Press Ctrl+C to stop.";
	WaitForShutdown();
	controller.Stop();
	return 0;
}
