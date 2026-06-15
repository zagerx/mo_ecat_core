#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace mo_ecat
{

class CyclicRunner
{
public:
	using Task = std::function<void()>;

	explicit CyclicRunner(int cycle_time_us);
	~CyclicRunner();

	bool Start(Task task);
	void Stop();
	bool IsRunning() const;

private:
	void RunLoop();

	int cycle_time_us_;
	std::atomic<bool> running_{false};
	std::thread thread_;
	Task task_;
};

} // namespace mo_ecat
