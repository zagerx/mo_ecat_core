#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace mo_ecat
{

enum class CliLogLevel {
	kDebug = 0,
	kInfo,
	kWarn,
	kError,
};

inline CliLogLevel &CliLogLevelThreshold()
{
	static CliLogLevel level = CliLogLevel::kInfo;
	return level;
}

inline std::mutex &CliLogMutex()
{
	static std::mutex mutex;
	return mutex;
}

inline const char *CliLogLevelName(CliLogLevel level)
{
	switch (level) {
	case CliLogLevel::kDebug:
		return "DEBUG";
	case CliLogLevel::kInfo:
		return "INFO";
	case CliLogLevel::kWarn:
		return "WARN";
	case CliLogLevel::kError:
		return "ERROR";
	}
	return "UNKNOWN";
}

inline std::string CliLogTimestamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto time = std::chrono::system_clock::to_time_t(now);

	std::tm tm {};
	localtime_r(&time, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%F %T");
	return oss.str();
}

inline void SetCliLogLevel(CliLogLevel level)
{
	CliLogLevelThreshold() = level;
}

class CliLogStream {
public:
	explicit CliLogStream(CliLogLevel level) : level_(level) {}

	~CliLogStream()
	{
		if (static_cast<int>(level_) <
		    static_cast<int>(CliLogLevelThreshold())) {
			return;
		}

		std::lock_guard<std::mutex> lock(CliLogMutex());
		std::ostream &os =
			(level_ == CliLogLevel::kError) ? std::cerr : std::cout;
		os << "[" << CliLogTimestamp() << "] [" << CliLogLevelName(level_)
		   << "] " << buffer_.str() << std::endl;
	}

	template <typename T>
	CliLogStream &operator<<(const T &value)
	{
		buffer_ << value;
		return *this;
	}

private:
	CliLogLevel level_;
	std::ostringstream buffer_;
};

} // namespace mo_ecat

#define LOG_DEBUG ::mo_ecat::CliLogStream(::mo_ecat::CliLogLevel::kDebug)
#define LOG_INFO ::mo_ecat::CliLogStream(::mo_ecat::CliLogLevel::kInfo)
#define LOG_WARN ::mo_ecat::CliLogStream(::mo_ecat::CliLogLevel::kWarn)
#define LOG_ERROR ::mo_ecat::CliLogStream(::mo_ecat::CliLogLevel::kError)
