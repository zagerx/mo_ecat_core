#pragma once

#include <mutex>
#include <string>

namespace mo_ecat {

// 纯电机语义对象。
// 不关心 EtherCAT、PDO、SDO，只描述电机的命令和反馈。
class ServoAxis {
public:
    struct Config {
        std::string name;

        // 原始值 -> 工程单位的缩放系数
        double position_scale = 0.001; // inc -> rad
        double velocity_scale = 0.001; // inc/s -> rad/s
        double torque_scale = 0.001;   // 额定扭矩千分之一 -> Nm

        // 电机方向：1 或 -1
        int direction = 1;
    };

    explicit ServoAxis(const Config &config);

    const Config &GetConfig() const;

    // ---------- 命令输入（上层调用） ----------
    void SetTargetPosition(double rad);
    void SetTargetVelocity(double rad_per_s);
    void SetTargetTorque(double nm);
    void SetEnable(bool enable);

    // ---------- 反馈输出（上层读取） ----------
    double GetActualPosition() const;
    double GetActualVelocity() const;
    double GetActualTorque() const;
    bool IsEnabled() const;
    bool HasFault() const;

    // ---------- 由 SlaveNode 周期更新 ----------
    void UpdateCommandReadback(double position, double velocity, double torque, bool enabled, bool fault);

    // SlaveNode 读取目标值，编码成 PDO 原始值
    double GetTargetPosition() const;
    double GetTargetVelocity() const;
    double GetTargetTorque() const;
    bool GetEnableCommand() const;

private:
    Config config_;

    mutable std::mutex mutex_;

    // 命令
    double target_position_ = 0.0;
    double target_velocity_ = 0.0;
    double target_torque_ = 0.0;
    bool enable_command_ = false;

    // 反馈
    double actual_position_ = 0.0;
    double actual_velocity_ = 0.0;
    double actual_torque_ = 0.0;
    bool enabled_ = false;
    bool fault_ = false;
};

} // namespace mo_ecat
