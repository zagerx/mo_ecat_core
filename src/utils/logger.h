#pragma once

#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace mo_ecat {

enum class LogLevel {
    Debug = 0,
    Info,
    Warn,
    Error,
    Fatal,
};

// 异步日志器。
// 调用方仅把日志记录加入内存队列并立即返回，实际的格式化、控制台/文件输出
// 由后台工作线程完成，从而避免文件 IO 阻塞业务线程（尤其是 EtherCAT 周期线程）。
class Logger {
public:
    static Logger &GetInstance();

    void SetConsoleLevel(LogLevel level);
    void SetFileLevel(LogLevel level);
    void SetConsoleEnabled(bool enabled);

    // 同时设置控制台和文件日志级别。
    void SetLogLevel(LogLevel level);

    // 设置日志文件路径，空字符串表示不输出到文件。
    // 会自动创建父目录。
    void SetLogFile(const std::string &path);

    // 设置回调 sink，空 function 表示禁用回调。
    // 回调在 Logger 后台线程中执行，调用方不应阻塞。
    void SetCallbackSink(LogLevel level,
                         std::function<void(LogLevel, const char *, int,
                                            const std::string &)> callback);

    // message 按值传入，内部会 move 进队列，避免额外拷贝。
    void Log(LogLevel level, const char *file, int line, std::string message);

private:
    Logger();
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    struct LogRecord {
        LogLevel level;
        const char *file;
        int line;
        std::string message;
    };

    void WorkerLoop();
    void WriteToSinks(const LogRecord &record);
    std::string FormatRecord(const LogRecord &record) const;

    static const char *LevelToString(LogLevel level);
    static std::string GetTimestamp();

    // 队列相关：调用方入队，工作线程出队
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<LogRecord> queue_;
    bool shutdown_ = false;

    // 输出目标相关
    std::mutex sink_mutex_;
    std::ofstream file_stream_;
    bool has_console_ = true;
    LogLevel console_level_ = LogLevel::Info;
    LogLevel file_level_ = LogLevel::Debug;
    bool has_file_ = false;

    LogLevel callback_level_ = LogLevel::Debug;
    std::function<void(LogLevel, const char *, int, const std::string &)> callback_sink_;
    bool has_callback_ = false;

    std::thread worker_thread_;
};

// 流式日志辅助类，在析构时把缓冲内容提交给 Logger。
class LogStream {
public:
    LogStream(LogLevel level, const char *file, int line);
    ~LogStream();

    template <typename T>
    LogStream &operator<<(const T &value) {
        buffer_ << value;
        return *this;
    }

private:
    LogLevel level_;
    const char *file_;
    int line_;
    std::ostringstream buffer_;
};

// 用于编译时裁剪日志的 no-op 流。
class NullLogStream {
public:
    template <typename T>
    NullLogStream &operator<<(const T &) {
        return *this;
    }
};

} // namespace mo_ecat

// 编译时日志级别：0=Debug, 1=Info, 2=Warn, 3=Error, 4=Fatal, 5=无日志。
// 可通过 CMake 的 target_compile_definitions 覆盖，例如 -DMO_ECAT_LOG_LEVEL=2。
#ifndef MO_ECAT_LOG_LEVEL
#define MO_ECAT_LOG_LEVEL 1
#endif

#if MO_ECAT_LOG_LEVEL <= 0
#define LOG_DEBUG ::mo_ecat::LogStream(::mo_ecat::LogLevel::Debug, __FILE__, __LINE__)
#else
#define LOG_DEBUG ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 1
#define LOG_INFO ::mo_ecat::LogStream(::mo_ecat::LogLevel::Info, __FILE__, __LINE__)
#else
#define LOG_INFO ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 2
#define LOG_WARN ::mo_ecat::LogStream(::mo_ecat::LogLevel::Warn, __FILE__, __LINE__)
#else
#define LOG_WARN ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 3
#define LOG_ERROR ::mo_ecat::LogStream(::mo_ecat::LogLevel::Error, __FILE__, __LINE__)
#else
#define LOG_ERROR ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 4
#define LOG_FATAL ::mo_ecat::LogStream(::mo_ecat::LogLevel::Fatal, __FILE__, __LINE__)
#else
#define LOG_FATAL ::mo_ecat::NullLogStream()
#endif
