#include "mo_ecat/master.h"

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

	MoEcatMaster *master_ = nullptr;

private:
	EcatController controller_;
	ActivityScheduler scheduler_;
	ProcessDataEngine engine_;
};

MoEcatMaster::Impl::Impl()
	: scheduler_(controller_),
	  engine_(controller_, controller_.GetEcMaster(), controller_.GetSlaveNodeManager())
{
}

MoEcatMaster::Impl::~Impl() = default;

bool MoEcatMaster::Impl::InitializeAdapter(const EcMasterConfig &config)
{
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

} // namespace mo_ecat
