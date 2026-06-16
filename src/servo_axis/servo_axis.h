#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ec_master/ec_master.h"

namespace mo_ecat {

// PDO 条目描述：名字、在 PDO 中的字节偏移、长度（1/2/4）
struct PdoEntry {
    std::string name;
    int offset = 0;
    int size = 0;
};

// 单个从站的配置
struct AxisConfig {
    std::string name;
    int slave_id = 1;
    std::vector<PdoEntry> output_entries;
    std::vector<PdoEntry> input_entries;
};

// 生成一组默认 PDO 映射的 AxisConfig（常用 CiA402 映射，仅做数据映射）。
AxisConfig MakeDefaultAxisConfig(int slave_id);

// 单个从站的 PDO 数据访问对象。
// 当前不做 CiA402 状态机，只负责按名字读写 PDO 条目。
class ServoAxis {
public:
    ServoAxis(EcMaster &master, const AxisConfig &config);

    // 周期调用：由 EcatController 统一调度
    void UpdatePdoOutput();
    void UpdatePdoInput();

    // 按名字写输出 PDO 条目
    template <typename T>
    bool WriteOutput(const std::string &name, const T &value);

    // 按名字读输入 PDO 条目
    template <typename T>
    bool ReadInput(const std::string &name, T &value) const;

    const AxisConfig &GetConfig() const;

private:
    static const PdoEntry *FindEntry(const std::vector<PdoEntry> &entries, const std::string &name);
    static size_t ComputeBufferSize(const std::vector<PdoEntry> &entries);

    EcMaster &master_;
    AxisConfig config_;

    std::vector<uint8_t> output_buffer_;
    std::vector<uint8_t> input_buffer_;

    mutable std::mutex mutex_;
};

// 模板成员函数定义（放在类外）
template <typename T>
bool ServoAxis::WriteOutput(const std::string &name, const T &value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const PdoEntry *entry = FindEntry(config_.output_entries, name);
    if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
        return false;
    }
    std::memcpy(output_buffer_.data() + entry->offset, &value, sizeof(T));
    return true;
}

template <typename T>
bool ServoAxis::ReadInput(const std::string &name, T &value) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const PdoEntry *entry = FindEntry(config_.input_entries, name);
    if (entry == nullptr || entry->size != static_cast<int>(sizeof(T))) {
        return false;
    }
    std::memcpy(&value, input_buffer_.data() + entry->offset, sizeof(T));
    return true;
}

} // namespace mo_ecat
