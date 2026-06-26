#include "mo_ecat/master.h"

#include <iomanip>
#include <optional>
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

MasterRuntimeState ToMasterRuntimeState(ControllerState state)
{
	switch (state) {
	case ControllerState::kUninitialized:
		return { MasterMode::kInit, TransitionStage::kStable, PrepareStage::kNone };
	case ControllerState::kAdapterReady:
		return { MasterMode::kPrepare, TransitionStage::kStable,
			 PrepareStage::kAdapterReady };
	case ControllerState::kScanned:
		return { MasterMode::kPrepare, TransitionStage::kStable,
			 PrepareStage::kTopologyDiscovered };
	case ControllerState::kMaintenance:
		return { MasterMode::kPrepare, TransitionStage::kStable,
			 PrepareStage::kPreOpMaintenance };
	case ControllerState::kReadyToRun:
		return { MasterMode::kPrepare, TransitionStage::kStable,
			 PrepareStage::kSafeOpReady };
	case ControllerState::kOperational:
		return { MasterMode::kRun, TransitionStage::kStable, PrepareStage::kNone };
	case ControllerState::kFault:
		return { MasterMode::kFault, TransitionStage::kStable, PrepareStage::kNone };
	case ControllerState::kEmergencyStop:
		return { MasterMode::kEmergencyStop, TransitionStage::kStable,
			 PrepareStage::kNone };
	}
	return { MasterMode::kInit, TransitionStage::kStable, PrepareStage::kNone };
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
	MasterRuntimeState GetRuntimeState() const;
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
	void BeginRuntimeTransition(MasterRuntimeState state);
	void EndRuntimeTransition();
	void NotifyRuntimeStateChanged() const;
	MasterRuntimeState CurrentRuntimeState() const;

	bool ReadSmAssignment(uint16_t slave_id, uint16_t assign_index,
			      std::vector<uint16_t> &pdo_indices);
	bool ReadPdoMapping(uint16_t slave_id, uint16_t pdo_index,
			    std::vector<uint32_t> &entries);

	EcatController controller_;
	ActivityScheduler scheduler_;
	ProcessDataEngine engine_;
	std::optional<MasterRuntimeState> runtime_state_override_;
};

MoEcatMaster::Impl::Impl()
	: scheduler_(controller_, [this]() { return CurrentRuntimeState(); }),
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
	BeginRuntimeTransition({ MasterMode::kPrepare, TransitionStage::kEntering,
				 PrepareStage::kAdapterReady });
	const bool ok = controller_.InitializeAdapter(config);
	EndRuntimeTransition();
	return ok;
}

bool MoEcatMaster::Impl::Scan()
{
	BeginRuntimeTransition({ MasterMode::kPrepare, TransitionStage::kEntering,
				 PrepareStage::kTopologyDiscovered });
	const bool ok = controller_.Scan();
	EndRuntimeTransition();
	return ok;
}

bool MoEcatMaster::Impl::EnterMaintenance()
{
	BeginRuntimeTransition({ MasterMode::kPrepare, TransitionStage::kEntering,
				 PrepareStage::kPreOpMaintenance });
	const bool ok = controller_.EnterMaintenance();
	EndRuntimeTransition();
	return ok;
}

bool MoEcatMaster::Impl::PrepareRun()
{
	BeginRuntimeTransition({ MasterMode::kPrepare, TransitionStage::kEntering,
				 PrepareStage::kSafeOpReady });
	const bool ok = controller_.PrepareRun();
	EndRuntimeTransition();
	return ok;
}

bool MoEcatMaster::Impl::StartOperation()
{
	BeginRuntimeTransition({ MasterMode::kRun, TransitionStage::kEntering,
				 PrepareStage::kNone });
	const bool ok = controller_.StartOperation();
	EndRuntimeTransition();
	return ok;
}

bool MoEcatMaster::Impl::BackToMaintenance()
{
	const auto current_state = CurrentRuntimeState();
	if (current_state.mode == MasterMode::kRun) {
		BeginRuntimeTransition({ MasterMode::kRun, TransitionStage::kExiting,
					 PrepareStage::kNone });
	} else {
		BeginRuntimeTransition({ MasterMode::kPrepare, TransitionStage::kEntering,
					 PrepareStage::kPreOpMaintenance });
	}
	const bool ok = controller_.BackToMaintenance();
	EndRuntimeTransition();
	return ok;
}

void MoEcatMaster::Impl::Stop()
{
	auto state = CurrentRuntimeState();
	state.transition = TransitionStage::kExiting;
	BeginRuntimeTransition(state);
	controller_.Stop();
	EndRuntimeTransition();
}

void MoEcatMaster::Impl::RequestFault(const std::string &reason)
{
	BeginRuntimeTransition({ MasterMode::kFault, TransitionStage::kEntering,
				 PrepareStage::kNone });
	controller_.RequestFault(reason);
	EndRuntimeTransition();
}

void MoEcatMaster::Impl::RequestEmergencyStop(const std::string &reason)
{
	BeginRuntimeTransition({ MasterMode::kEmergencyStop, TransitionStage::kEntering,
				 PrepareStage::kNone });
	controller_.RequestEmergencyStop(reason);
	EndRuntimeTransition();
}

void MoEcatMaster::Impl::Service()
{
	const auto old_state = GetState();
	const auto runtime_state = CurrentRuntimeState();

	if (ShouldCheckPreOpSlaves(runtime_state)) {
		engine_.CheckSlaveStates();
	} else if (CanRunProcessData(runtime_state)) {
		engine_.RunOnce();
		engine_.CheckSlaveStates();
	}

	const auto new_state = GetState();
	if (old_state != new_state && master_ != nullptr && master_->on_state_changed) {
		master_->on_state_changed(old_state, new_state);
	}
	if (old_state != new_state) {
		NotifyRuntimeStateChanged();
	}
}

MasterState MoEcatMaster::Impl::GetState() const
{
	return ToMasterState(controller_.GetState());
}

MasterRuntimeState MoEcatMaster::Impl::GetRuntimeState() const
{
	return CurrentRuntimeState();
}

void MoEcatMaster::Impl::BeginRuntimeTransition(MasterRuntimeState state)
{
	if (!IsValidRuntimeState(state)) {
		LOG_ERROR << "Invalid runtime state requested: " << ToDisplayString(state)
			  << ", fallback to fault";
		state = { MasterMode::kFault, TransitionStage::kEntering, PrepareStage::kNone };
	}
	runtime_state_override_ = state;
	NotifyRuntimeStateChanged();
}

void MoEcatMaster::Impl::EndRuntimeTransition()
{
	runtime_state_override_.reset();
	NotifyRuntimeStateChanged();
}

void MoEcatMaster::Impl::NotifyRuntimeStateChanged() const
{
	if (master_ != nullptr && master_->on_runtime_state_changed) {
		master_->on_runtime_state_changed(CurrentRuntimeState());
	}
}

MasterRuntimeState MoEcatMaster::Impl::CurrentRuntimeState() const
{
	if (runtime_state_override_.has_value()) {
		if (IsValidRuntimeState(*runtime_state_override_)) {
			return *runtime_state_override_;
		}
		return { MasterMode::kFault, TransitionStage::kStable, PrepareStage::kNone };
	}
	const auto state = ToMasterRuntimeState(controller_.GetState());
	if (!IsValidRuntimeState(state)) {
		return { MasterMode::kFault, TransitionStage::kStable, PrepareStage::kNone };
	}
	return state;
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
	snapshot.runtime_state = GetRuntimeState();
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

MasterRuntimeState MoEcatMaster::GetRuntimeState() const
{
	return impl_->GetRuntimeState();
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
