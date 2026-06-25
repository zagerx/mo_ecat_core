#pragma once

#include <string>

namespace mo_ecat
{

// 命令读取结果
enum class ReadResult {
	kOk,       // 成功读取到一条命令
	kTimeout,  // 超时，没有命令
	kEof,      // 输入流结束（如 Ctrl+D），调用方应考虑退出
	kError,    // 读取错误
};

// 命令读取器抽象接口。
// EcatApplication 通过此接口读取用户命令，不依赖具体的输入通道。
// 当前实现为 stdin，后续可替换为 socket、named pipe、RPC 等。
class CommandReader
{
public:
	virtual ~CommandReader() = default;

	// 尝试在 timeout_ms 毫秒内读取一条命令。
	virtual ReadResult Read(std::string &command, int timeout_ms) = 0;
};

} // namespace mo_ecat
