#include "utils/signal_handler.h"

#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>

namespace mo_ecat
{

std::atomic<bool> SignalHandler::shutdown_requested_{false};
std::function<void()> SignalHandler::callback_;

static std::mutex g_mutex;
static std::condition_variable g_cv;

static void OnSignal(int /*signum*/)
{
	std::cout << "\nShutdown signal received\n";
	SignalHandler::RequestShutdown();
}

void SignalHandler::Register(std::function<void()> callback)
{
	callback_ = std::move(callback);
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
}

void SignalHandler::RequestShutdown()
{
	shutdown_requested_ = true;
	if (callback_) {
		callback_();
	}
	g_cv.notify_all();
}

bool SignalHandler::IsShutdownRequested()
{
	return shutdown_requested_;
}

void SignalHandler::WaitForShutdown()
{
	std::unique_lock<std::mutex> lock(g_mutex);
	g_cv.wait(lock, []() { return shutdown_requested_.load(); });
}

} // namespace mo_ecat
