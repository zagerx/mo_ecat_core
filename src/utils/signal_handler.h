#pragma once

#include <atomic>
#include <functional>

namespace mo_ecat
{

class SignalHandler
{
public:
	static void Register(std::function<void()> callback);
	static void RequestShutdown();
	static bool IsShutdownRequested();
	static void WaitForShutdown();

private:
	static std::atomic<bool> shutdown_requested_;
	static std::function<void()> callback_;
};

} // namespace mo_ecat
