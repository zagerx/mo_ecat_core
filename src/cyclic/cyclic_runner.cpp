#include "cyclic/cyclic_runner.h"

#include <iostream>

namespace mo_ecat
{

CyclicRunner::CyclicRunner(int cycle_time_us)
	: cycle_time_us_(cycle_time_us)
{
}

CyclicRunner::~CyclicRunner()
{
	Stop();
}

bool CyclicRunner::Start(Task task)
{
	if (running_) {
		std::cerr << "CyclicRunner already running\n";
		return false;
	}

	task_ = std::move(task);
	running_ = true;
	thread_ = std::thread(&CyclicRunner::RunLoop, this);
	return true;
}

void CyclicRunner::Stop()
{
	if (!running_) {
		return;
	}

	running_ = false;
	if (thread_.joinable()) {
		thread_.join();
	}
}

bool CyclicRunner::IsRunning() const
{
	return running_;
}

void CyclicRunner::RunLoop()
{
	using namespace std::chrono;

	auto next_time = steady_clock::now();
	auto interval = microseconds(cycle_time_us_);

	while (running_) {
		if (task_) {
			task_();
		}

		next_time += interval;
		std::this_thread::sleep_until(next_time);
	}
}

} // namespace mo_ecat
