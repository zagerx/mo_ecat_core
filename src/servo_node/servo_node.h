#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"
#include "servo_axis/servo_axis.h"

namespace mo_ecat {

// PDO 条目描述
struct PdoEntry {
    std::string name;
    int offset = 0;
    int size = 0;
};

// PDO 映射：输出 + 输入
struct PdoMapping {
    std::vector<PdoEntry> outputs;
    std::vector<PdoEntry> inputs;
};

// 从站配置
struct SlaveConfig {
    int slave_id = 1;
    std::string name;
    PdoMapping pdo_mapping;
    ServoAxis::Config axis_config;
};

// 生成一组默认 PDO 映射（不含 CiA402 控制字/状态字，仅位置/速度/力矩数据）
SlaveConfig MakeDefaultSlaveConfig(int slave_id);

// 单个从站节点：负责 EtherCAT 通信状态、PDO 映射、SDO、以及对应的 ServoAxis
class ServoNode {
public:
    ServoNode(EcMaster &master, const SlaveConfig &config);

    // ---------- EtherCAT 通信状态 ----------
    bool RequestState(uint16_t state);
    uint16_t GetCurrentState() const;

    // ---------- SDO 通信（当前为框架，功能后续实现） ----------
    bool SdoRead(uint16_t index, uint8_t subindex, void *data, size_t len, int timeout_us = 10000);
    bool SdoWrite(uint16_t index, uint8_t subindex, const void *data, size_t len, int timeout_us = 10000);

    // ---------- PDO 映射配置 ----------
    bool ConfigurePdoMapping();
    void SetPdoMapping(const PdoMapping &mapping);

    // ---------- 周期调用：PDO 数据 <=> ServoAxis 语义 ----------
    void UpdatePdoOutput();
    void UpdatePdoInput();

    ServoAxis &GetAxis();
    const SlaveConfig &GetConfig() const;

private:
    static const PdoEntry *FindEntry(const std::vector<PdoEntry> &entries, const std::string &name);
    static size_t ComputeBufferSize(const std::vector<PdoEntry> &entries);

    template <typename T>
    bool WriteOutputRaw(const std::string &name, const T &value) {
        const PdoEntry *entry = FindEntry(config_.pdo_mapping.outputs, name);
        if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
            return false;
        }
        std::memcpy(output_buffer_.data() + entry->offset, &value, sizeof(T));
        return true;
    }

    template <typename T>
    bool ReadInputRaw(const std::string &name, T &value) const {
        const PdoEntry *entry = FindEntry(config_.pdo_mapping.inputs, name);
        if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
            return false;
        }
        std::memcpy(&value, input_buffer_.data() + entry->offset, sizeof(T));
        return true;
    }

    EcMaster &master_;
    SlaveConfig config_;

    std::vector<uint8_t> output_buffer_;
    std::vector<uint8_t> input_buffer_;

    ServoAxis axis_;
    uint16_t current_state_ = 0; // EC_STATE_INIT
};

} // namespace mo_ecat
