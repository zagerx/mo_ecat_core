#pragma once

#include <string>
#include <vector>

#include "mo_ecat/types.h"

namespace mo_ecat
{

enum class LogSinkMode {
	kNone,
	kFile,
	kCallback,
	kFileAndCallback,
};

struct EcMasterConfig {
	std::string ifname = "eth0";
	int cycle_time_us = 1000;
	bool use_dc = true;

	// 预期从站数量；0 表示不校验数量。
	int expected_slave_count = 0;

	// 预期从站身份列表（按扫描位置顺序）。为空表示不校验身份。
	std::vector<SlaveIdentity> expected_identities;

	LogSinkMode log_sink = LogSinkMode::kCallback;
	std::string log_path;

	// 反馈发布频率，用于降低 GUI/CLI 刷新负担。
	int feedback_publish_hz = 20;
};

} // namespace mo_ecat
