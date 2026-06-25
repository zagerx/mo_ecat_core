#include "stdin_command_reader.h"

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "utils/logger.h"

namespace mo_ecat
{

// 使用 poll() 检查 stdin 是否有数据可读。
// timeout_ms = 0 时非阻塞；> 0 时阻塞等待至多 timeout_ms 毫秒。
// 返回 kOk / kTimeout / kEof / kError。
ReadResult StdinCommandReader::Read(std::string &command, int timeout_ms)
{
	struct pollfd pfd {};
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, timeout_ms);
	if (ret < 0) {
		if (errno == EINTR) {
			return ReadResult::kTimeout;
		}
		LOG_ERROR << "poll stdin failed: " << std::strerror(errno);
		return ReadResult::kError;
	}

	if (ret == 0) {
		return ReadResult::kTimeout;
	}

	if (!std::getline(std::cin, command)) {
		// EOF / Ctrl+D
		return ReadResult::kEof;
	}

	return ReadResult::kOk;
}

} // namespace mo_ecat
