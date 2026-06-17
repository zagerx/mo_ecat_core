#include "ec_controller/ec_controller.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "utils/logger.h"

namespace {

std::string MbxProtoToString(uint16_t proto)
{
    std::string s;
    if (proto & 0x01) s += "AoE ";
    if (proto & 0x02) s += "EoE ";
    if (proto & 0x04) s += "CoE ";
    if (proto & 0x08) s += "FoE ";
    if (proto & 0x10) s += "SoE ";
    if (proto & 0x20) s += "VoE ";
    if (s.empty()) {
        return "none";
    }
    s.pop_back();
    return s;
}

std::string CoEDetailsToString(uint8_t details)
{
    std::string s;
    if (details & 0x01) s += "SDO ";
    if (details & 0x02) s += "SDOINFO ";
    if (details & 0x04) s += "PDOASSIGN ";
    if (details & 0x08) s += "PDOCONFIG ";
    if (details & 0x10) s += "UPLOAD ";
    if (details & 0x20) s += "SDOCA ";
    if (s.empty()) {
        return "none";
    }
    s.pop_back();
    return s;
}

std::string FormatSlaveInfo(const mo_ecat::SlaveInfo &info)
{
    std::ostringstream oss;
    oss << "Slave[" << info.slave_id << "] info:\n";

    auto line = [&](const char *key, const std::string &value) {
        oss << "  " << std::left << std::setw(18) << key << " = " << value << "\n";
    };

    auto hex_u32 = [](uint32_t v) {
        std::ostringstream s;
        s << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
        return s.str();
    };
    auto hex_u16 = [](uint16_t v) {
        std::ostringstream s;
        s << "0x" << std::hex << std::setfill('0') << std::setw(4) << v;
        return s.str();
    };
    auto hex_u8 = [](uint8_t v) {
        std::ostringstream s;
        s << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(v);
        return s.str();
    };

    line("name", "\"" + info.name + "\"");
    line("vendor_id", hex_u32(info.vendor_id));
    line("product_id", hex_u32(info.product_id));
    line("revision_id", hex_u32(info.revision_id));
    line("serial_id", hex_u32(info.serial_id));
    line("config_address", hex_u16(info.config_address));
    line("alias_address", hex_u16(info.alias_address));
    line("supports_dc", info.supports_dc ? "yes" : "no");

    line("mbx_l", std::to_string(info.mbx_l));
    line("mbx_wo", hex_u16(info.mbx_wo));
    line("mbx_ro", hex_u16(info.mbx_ro));
    line("mbx_proto", hex_u16(info.mbx_proto) + " (" + MbxProtoToString(info.mbx_proto) + ")");
    line("mbx_cnt", std::to_string(info.mbx_cnt));

    line("state", hex_u16(info.state));
    line("al_status_code", hex_u16(info.al_status_code));
    line("coe_details", hex_u8(info.coe_details) + " (" + CoEDetailsToString(info.coe_details) + ")");
    line("output_bytes", std::to_string(info.output_bytes));
    line("input_bytes", std::to_string(info.input_bytes));

    return oss.str();
}

} // namespace

namespace mo_ecat
{

EcatController::EcatController()
{
}

EcatController::~EcatController()
{
	Stop();
}

bool EcatController::Initialize(const EcMasterConfig &config)
{
	if (initialized_) {
		LOG_WARN << "EcatController already initialized";
		return false;
	}

	if (!master_.Initialize(config)) {
		return false;
	}

	auto slave_infos = master_.ScanSlaves();
	if (slave_infos.empty()) {
		return false;
	}

	for (const auto &info : slave_infos) {
		LOG_INFO << FormatSlaveInfo(info);
	}

	node_manager_.Initialize(master_, slave_infos);
	LOG_INFO << "Created " << node_manager_.GetNodeCount() << " slave node(s)";

	initialized_ = true;
	return true;
}

bool EcatController::StartOperation()
{
	if (!initialized_) {
		LOG_WARN << "EcatController not initialized, call Initialize() first";
		return false;
	}

	if (operational_) {
		LOG_WARN << "EcatController already operational";
		return false;
	}

	if (!master_.RequestOperationalState()) {
		LOG_ERROR << "Failed to enter OPERATIONAL";
		return false;
	}

	operational_ = true;
	LOG_INFO << "EcatController operational";
	return true;
}

void EcatController::Stop()
{
	if (!initialized_) {
		return;
	}

	master_.RequestSafeOpState();
	master_.RequestInitState();

	node_manager_.Clear();
	operational_ = false;
	initialized_ = false;
}

void EcatController::RunOneCycle()
{
	node_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	node_manager_.UpdateAllInputs();
}

void EcatController::CheckSlaveStates()
{
	master_.CheckSlaveStates();
}

SlaveNodeManager &EcatController::GetSlaveNodeManager()
{
	return node_manager_;
}

bool EcatController::IsInitialized() const
{
	return initialized_;
}

bool EcatController::IsOperational() const
{
	return operational_;
}

} // namespace mo_ecat
