#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include "app/ecat_application.h"
#include "ec_master/ec_master.h"
#include "utils/logger.h"

namespace
{

// 全局指针用于信号处理函数访问应用实例
mo_ecat::EcatApplication *g_app = nullptr;

void OnSignal(int /*signal*/)
{
	if (g_app != nullptr) {
		g_app->RequestShutdown();
	}
}

void RegisterShutdownSignals()
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
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

	auto app = std::make_unique<mo_ecat::EcatApplication>();
	if (!app->Initialize(config)) {
		LOG_ERROR << "Failed to initialize application";
		return 1;
	}

	g_app = app.get();
	RegisterShutdownSignals();

	app->Run();

	LOG_INFO << "Shutting down...";
	app->Shutdown();

	g_app = nullptr;
	return 0;
}
