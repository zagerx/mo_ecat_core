#include "utils/logger.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace mo_ecat {

namespace fs = std::filesystem;

Logger &Logger::GetInstance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
{
    // 默认输出到 logger/ecat.log，目录不存在则自动创建。
    SetLogFile("logger/ecat.log");
    worker_thread_ = std::thread(&Logger::WorkerLoop, this);
}

Logger::~Logger()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
    }
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // 工作线程退出后，sink 不会再被访问，可直接关闭文件。
    std::lock_guard<std::mutex> lock(sink_mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::SetConsoleLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(sink_mutex_);
    console_level_ = level;
}

void Logger::SetFileLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(sink_mutex_);
    file_level_ = level;
}

void Logger::SetLogFile(const std::string &path)
{
    if (path.empty()) {
        std::lock_guard<std::mutex> lock(sink_mutex_);
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        has_file_ = false;
        return;
    }

    try {
        fs::path p(path);
        fs::create_directories(p.parent_path());
    } catch (const std::exception &e) {
        std::cerr << "[Logger] Failed to create log directory for " << path << ": " << e.what() << "\n";
        return;
    }

    std::lock_guard<std::mutex> lock(sink_mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_stream_.open(path, std::ios::out | std::ios::app);
    if (!file_stream_.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << path << "\n";
        has_file_ = false;
        return;
    }
    has_file_ = true;
}

const char *Logger::LevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

std::string Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm{};
    localtime_r(&time_t_now, &local_tm);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3)
        << std::setfill('0') << ms.count();
    return oss.str();
}

std::string Logger::FormatRecord(const LogRecord &record) const
{
    // 取源文件短名，避免完整路径过长。
    const char *basename = record.file;
    for (const char *p = record.file; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            basename = p + 1;
        }
    }

    std::ostringstream oss;
    oss << "[" << GetTimestamp() << "] [" << LevelToString(record.level) << "] [" << basename << ":"
        << record.line << "] " << record.message << "\n";
    return oss.str();
}

void Logger::WriteToSinks(const LogRecord &record)
{
    std::string formatted = FormatRecord(record);

    std::lock_guard<std::mutex> lock(sink_mutex_);
    if (static_cast<int>(record.level) >= static_cast<int>(console_level_)) {
        if (record.level >= LogLevel::Warn) {
            std::cerr << formatted;
        } else {
            std::cout << formatted;
        }
    }
    if (has_file_ && static_cast<int>(record.level) >= static_cast<int>(file_level_)) {
        file_stream_ << formatted;
    }
}

void Logger::Log(LogLevel level, const char *file, int line, std::string message)
{
    bool log_console = false;
    bool log_file = false;
    {
        std::lock_guard<std::mutex> lock(sink_mutex_);
        log_console = static_cast<int>(level) >= static_cast<int>(console_level_);
        log_file = has_file_ && static_cast<int>(level) >= static_cast<int>(file_level_);
    }
    if (!log_console && !log_file) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (shutdown_) {
            return;
        }
        queue_.emplace_back(LogRecord{level, file, line, std::move(message)});
    }
    queue_cv_.notify_one();
}

void Logger::WorkerLoop()
{
    while (true) {
        std::vector<LogRecord> batch;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return shutdown_ || !queue_.empty(); });
            if (shutdown_ && queue_.empty()) {
                break;
            }
            batch.swap(queue_);
        }

        for (const auto &record : batch) {
            WriteToSinks(record);
        }

        {
            std::lock_guard<std::mutex> lock(sink_mutex_);
            if (file_stream_.is_open()) {
                file_stream_.flush();
            }
        }
    }
}

LogStream::LogStream(LogLevel level, const char *file, int line)
    : level_(level), file_(file), line_(line)
{
}

LogStream::~LogStream()
{
    Logger::GetInstance().Log(level_, file_, line_, buffer_.str());
}

} // namespace mo_ecat
