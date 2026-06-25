#include "mo_ecat/master.h"

#include <iomanip>
#include <sstream>

#include "activity/activity_scheduler.h"
#include "cyclic/process_data_engine.h"
#include "ec_controller/ec_controller.h"
#include "ec_master/ec_master.h"
#include "slave_node/slave_node_manager.h"
#include "utils/logger.h"

namespace mo_ecat
{

namespace
{

MasterState ToMasterState(ControllerState state)
{
	switch (state) {
	case ControllerState::kUninitialized:
		return MasterState::kUninitialized;
	case ControllerState::kAdapterReady:
		return MasterState::kAdapterReady;
	case ControllerState::kScanned:
		return MasterState::kScanned;
	case ControllerState::kMaintenance:
		return MasterState::kMaintenance;
	case ControllerState::kReadyToRun:
		return MasterState::kReadyToRun;
	case ControllerState::kOperational:
		return MasterState::kOperational;
	case ControllerState::kFault:
		return MasterState::kFault;
	case ControllerState::kEmergencyStop:
		return MasterState::kEmergencyStop;
	}
	return MasterState::kUninitialized;
}

const char *LevelToString(LogLevel level)
{
	switch (level) {
	case LogLevel::Debug:
		return "debug";
	case LogLevel::Info:
		return "info";
	case LogLevel::Warn:
		return "warn";
	case LogLevel::Error:
		return "error";
	case LogLevel::Fatal:
		return "fatal";
	}
	return "unknown";
}

} // namespace

class MoEcatMaster::Impl {
public:
	Impl();
	~Impl();

	bool InitializeAdapter(const EcMasterConfig &config);
	bool Scan();
	bool EnterMaintenance();
	bool PrepareRun();
	bool StartOperation();
	bool BackToMaintenance();
	void Stop();

	void RequestFault(const std::string &reason);
	void RequestEmergencyStop(const std::string &reason);

	void Service();

	MasterState GetState() const;
	std::size_t GetSlaveCount() const;
	std::vector<SlaveInfo> GetSlaveInfos();
	MasterSnapshot GetSnapshot();

	bool ReadSdo(int slave_id, uint16_t index, uint8_t subindex,
		     std::vector<uint8_t> &data, int timeout_ms);
	bool WriteSdo(int slave_id, uint16_t index, uint8_t subindex,
		      const std::vector<uint8_t> &data, int timeout_ms);
	std::string DumpPdoMapping(int slave_id);

	MoEcatMaster *master_ = nullptr;

private:
	void ConfigureLogSink(const EcMasterConfig &config);
	void DisableLogCallback();

	bool ReadSmAssignment(uint16_t slave_id, uint16_t assign_index,
			      std::vector<uint16_t> &pdo_indices);
	bool ReadPdoMapping(uint16_t slave_id, uint16_t pdo_index,
			    std::vector<uint32_t> &entries);

	EcatController controller_;
	ActivityScheduler scheduler_;
	ProcessDataEngine engine_;
};

MoEcatMaster::Impl::Impl()
	: scheduler_(controller_),
	  engine_(controller_, controller_.GetEcMaster(), controller_.GetSlaveNodeManager())
{
}

MoEcatMaster::Impl::~Impl()
{
	DisableLogCallback();
}

void MoEcatMaster::Impl::ConfigureLogSink(const EcMasterConfig &config)
{
	auto &logger = Logger::GetInstance();

	switch (config.log_sink) {
	case LogSinkMode::kNone:
		logger.SetConsoleEnabled(false);
		logger.SetLogFile("");
		DisableLogCallback();
		break;

	case LogSinkMode::kFile:
		logger.SetConsoleEnabled(false);
		logger.SetLogFile(config.log_path);
		DisableLogCallback();
		break;

	case LogSinkMode::kCallback:
		logger.SetConsoleEnabled(false);
		logger.SetLogFile("");
		logger.SetCallbackSink(
			LogLevel::Debug,
			[this](LogLevel level, const char *file, int /*line*/,
			       const std::string &message) {
				if (master_ != nullptr && master_->on_log_message) {
					master_->on_log_message(LevelToString(level),
							file ? file : "", message);
				}
			});
		break;

	case LogSinkMode::kFileAndCallback:
		logger.SetConsoleEnabled(false);
		logger.SetLogFile(config.log_path);
		logger.SetCallbackSink(
			LogLevel::Debug,
			[this](LogLevel level, const char *file, int /*line*/,
			       const std::string &message) {
				if (master_ != nullptr && master_->on_log_message) {
					master_->on_log_message(LevelToString(level),
							file ? file : "", message);
				}
			});
		break;
	}
}

void MoEcatMaster::Impl::DisableLogCallback()
{
	Logger::GetInstance().SetCallbackSink(LogLevel::Debug, nullptr);
}

bool MoEcatMaster::Impl::InitializeAdapter(const EcMasterConfig &config)
{
	ConfigureLogSink(config);
	return controller_.InitializeAdapter(config);
}

bool MoEcatMaster::Impl::Scan()
{
	return controller_.Scan();
}

bool MoEcatMaster::Impl::EnterMaintenance()
{
	return controller_.EnterMaintenance();
}

bool MoEcatMaster::Impl::PrepareRun()
{
	return controller_.PrepareRun();
}

bool MoEcatMaster::Impl::StartOperation()
{
	return controller_.StartOperation();
}

bool MoEcatMaster::Impl::BackToMaintenance()
{
	return controller_.BackToMaintenance();
}

void MoEcatMaster::Impl::Stop()
{
	controller_.Stop();
}

void MoEcatMaster::Impl::RequestFault(const std::string &reason)
{
	controller_.RequestFault(reason);
}

void MoEcatMaster::Impl::RequestEmergencyStop(const std::string &reason)
{
	controller_.RequestEmergencyStop(reason);
}

void MoEcatMaster::Impl::Service()
{
	const auto old_state = GetState();

	switch (controller_.GetState()) {
	case ControllerState::kMaintenance:
		engine_.CheckSlaveStates();
		break;
	case ControllerState::kOperational:
		engine_.RunOnce();
		engine_.CheckSlaveStates();
		break;
	default:
		break;
	}

	const auto new_state = GetState();
	if (old_state != new_state && master_ != nullptr && master_->on_state_changed) {
		master_->on_state_changed(old_state, new_state);
	}
}

MasterState MoEcatMaster::Impl::GetState() const
{
	return ToMasterState(controller_.GetState());
}

std::size_t MoEcatMaster::Impl::GetSlaveCount() const
{
	return controller_.GetSlaveCount();
}

std::vector<SlaveInfo> MoEcatMaster::Impl::GetSlaveInfos()
{
	std::vector<SlaveInfo> infos;
	const int count = controller_.GetEcMaster().GetSlaveCount();
	infos.reserve(static_cast<size_t>(count));
	for (int i = 1; i <= count; ++i) {
		infos.push_back(controller_.GetEcMaster().GetSlaveInfo(i));
	}
	return infos;
}

MasterSnapshot MoEcatMaster::Impl::GetSnapshot()
{
	MasterSnapshot snapshot;
	snapshot.state = GetState();
	snapshot.slaves = GetSlaveInfos();
	return snapshot;
}

bool MoEcatMaster::Impl::ReadSdo(int slave_id, uint16_t index, uint8_t subindex,
				 std::vector<uint8_t> &data, int timeout_ms)
{
	if (slave_id <= 0 || data.empty()) {
		return false;
	}
	return controller_.GetEcMaster().SdoRead(
		static_cast<uint16_t>(slave_id), index, subindex, data.data(),
		static_cast<int>(data.size()), timeout_ms * 1000);
}

bool MoEcatMaster::Impl::WriteSdo(int slave_id, uint16_t index, uint8_t subindex,
				  const std::vector<uint8_t> &data, int timeout_ms)
{
	if (slave_id <= 0 || data.empty()) {
		return false;
	}
	return controller_.GetEcMaster().SdoWrite(
		static_cast<uint16_t>(slave_id), index, subindex, data.data(),
		static_cast<int>(data.size()), timeout_ms * 1000);
}

bool MoEcatMaster::Impl::ReadSmAssignment(uint16_t slave_id, uint16_t assign_index,
					  std::vector<uint16_t> &pdo_indices)
{
	pdo_indices.clear();

	uint8_t count = 0;
	if (!controller_.GetEcMaster().SdoRead(slave_id, assign_index, 0x00, &count,
					       sizeof(count), 10000)) {
		return false;
	}

	for (uint8_t i = 1; i <= count; ++i) {
		uint16_t pdo_index = 0;
		if (!controller_.GetEcMaster().SdoRead(slave_id, assign_index, i, &pdo_index,
					       sizeof(pdo_index), 10000)) {
			return false;
		}
		pdo_indices.push_back(pdo_index);
	}
	return true;
}

bool MoEcatMaster::Impl::ReadPdoMapping(uint16_t slave_id, uint16_t pdo_index,
					std::vector<uint32_t> &entries)
{
	entries.clear();

	uint8_t count = 0;
	if (!controller_.GetEcMaster().SdoRead(slave_id, pdo_index, 0x00, &count,
					       sizeof(count), 10000)) {
		return false;
	}

	for (uint8_t i = 1; i <= count; ++i) {
		uint32_t entry = 0;
		if (!controller_.GetEcMaster().SdoRead(slave_id, pdo_index, i, &entry,
					       sizeof(entry), 10000)) {
			return false;
		}
		entries.push_back(entry);
	}
	return true;
}

std::string MoEcatMaster::Impl::DumpPdoMapping(int slave_id)
{
	if (slave_id <= 0) {
		return "";
	}

	const uint16_t sid = static_cast<uint16_t>(slave_id);
	std::ostringstream oss;

	auto dump_direction = [&](const char *direction, uint16_t assign_index) {
		std::vector<uint16_t> pdo_indices;
		if (!ReadSmAssignment(sid, assign_index, pdo_indices)) {
			oss << direction << " (0x" << std::hex << assign_index << std::dec
			    << "): read failed\n";
			return;
		}

		oss << direction << " (0x" << std::hex << assign_index << std::dec << "):\n";
		for (uint16_t pdo_index : pdo_indices) {
			std::vector<uint32_t> entries;
			if (!ReadPdoMapping(sid, pdo_index, entries)) {
				oss << "  PDO 0x" << std::hex << pdo_index << std::dec
				    << ": read failed\n";
				continue;
			}

			oss << "  PDO 0x" << std::hex << pdo_index << std::dec << ":\n";
			for (size_t i = 0; i < entries.size(); ++i) {
				const uint32_t entry = entries[i];
				const uint16_t index = static_cast<uint16_t>((entry >> 16) & 0xFFFF);
				const uint8_t subindex = static_cast<uint8_t>((entry >> 8) & 0xFF);
				const uint8_t bit_length = static_cast<uint8_t>(entry & 0xFF);
				oss << "    [" << i << "] 0x" << std::hex << index << ":"
				    << static_cast<int>(subindex) << std::dec << " ("
				    << static_cast<int>(bit_length) << " bits)\n";
			}
		}
	};

	dump_direction("RxPDO", 0x1C12);
	dump_direction("TxPDO", 0x1C13);

	return oss.str();
}

MoEcatMaster::MoEcatMaster() : impl_(std::make_unique<Impl>())
{
	impl_->master_ = this;
}

MoEcatMaster::~MoEcatMaster() = default;

bool MoEcatMaster::InitializeAdapter(const EcMasterConfig &config)
{
	return impl_->InitializeAdapter(config);
}

bool MoEcatMaster::Scan()
{
	return impl_->Scan();
}

bool MoEcatMaster::EnterMaintenance()
{
	return impl_->EnterMaintenance();
}

bool MoEcatMaster::PrepareRun()
{
	return impl_->PrepareRun();
}

bool MoEcatMaster::StartOperation()
{
	return impl_->StartOperation();
}

bool MoEcatMaster::BackToMaintenance()
{
	return impl_->BackToMaintenance();
}

void MoEcatMaster::Stop()
{
	impl_->Stop();
}

void MoEcatMaster::RequestFault(const std::string &reason)
{
	impl_->RequestFault(reason);
}

void MoEcatMaster::RequestEmergencyStop(const std::string &reason)
{
	impl_->RequestEmergencyStop(reason);
}

void MoEcatMaster::Service()
{
	impl_->Service();
}

MasterState MoEcatMaster::GetState() const
{
	return impl_->GetState();
}

std::size_t MoEcatMaster::GetSlaveCount() const
{
	return impl_->GetSlaveCount();
}

std::vector<SlaveInfo> MoEcatMaster::GetSlaveInfos()
{
	return impl_->GetSlaveInfos();
}

MasterSnapshot MoEcatMaster::GetSnapshot()
{
	return impl_->GetSnapshot();
}

bool MoEcatMaster::ReadSdo(int slave_id, uint16_t index, uint8_t subindex,
			   std::vector<uint8_t> &data, int timeout_ms)
{
	return impl_->ReadSdo(slave_id, index, subindex, data, timeout_ms);
}

bool MoEcatMaster::WriteSdo(int slave_id, uint16_t index, uint8_t subindex,
			    const std::vector<uint8_t> &data, int timeout_ms)
{
	return impl_->WriteSdo(slave_id, index, subindex, data, timeout_ms);
}

std::string MoEcatMaster::DumpPdoMapping(int slave_id)
{
	return impl_->DumpPdoMapping(slave_id);
}

} // namespace mo_ecat
