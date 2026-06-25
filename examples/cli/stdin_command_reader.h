#pragma once

#include "command_reader.h"

namespace mo_ecat
{

// 从 stdin 读取命令的实现，使用 poll() 实现超时。
class StdinCommandReader : public CommandReader
{
public:
	~StdinCommandReader() override = default;

	ReadResult Read(std::string &command, int timeout_ms) override;
};

} // namespace mo_ecat
