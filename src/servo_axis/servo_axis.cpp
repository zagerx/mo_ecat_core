#include "servo_axis/servo_axis.h"

namespace mo_ecat {

ServoAxis::ServoAxis(const Config &config)
    : config_(config)
{
}

const ServoAxis::Config &ServoAxis::GetConfig() const
{
    return config_;
}

void ServoAxis::SetTargetPosition(double rad)
{
    std::lock_guard<std::mutex> lock(mutex_);
    target_position_ = rad;
}

void ServoAxis::SetTargetVelocity(double rad_per_s)
{
    std::lock_guard<std::mutex> lock(mutex_);
    target_velocity_ = rad_per_s;
}

void ServoAxis::SetTargetTorque(double nm)
{
    std::lock_guard<std::mutex> lock(mutex_);
    target_torque_ = nm;
}

void ServoAxis::SetEnable(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    enable_command_ = enable;
}

double ServoAxis::GetActualPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actual_position_;
}

double ServoAxis::GetActualVelocity() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actual_velocity_;
}

double ServoAxis::GetActualTorque() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actual_torque_;
}

bool ServoAxis::IsEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

bool ServoAxis::HasFault() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fault_;
}

void ServoAxis::UpdateCommandReadback(double position, double velocity, double torque, bool enabled, bool fault)
{
    std::lock_guard<std::mutex> lock(mutex_);
    actual_position_ = position;
    actual_velocity_ = velocity;
    actual_torque_ = torque;
    enabled_ = enabled;
    fault_ = fault;
}

double ServoAxis::GetTargetPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return target_position_;
}

double ServoAxis::GetTargetVelocity() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return target_velocity_;
}

double ServoAxis::GetTargetTorque() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return target_torque_;
}

bool ServoAxis::GetEnableCommand() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return enable_command_;
}

} // namespace mo_ecat
