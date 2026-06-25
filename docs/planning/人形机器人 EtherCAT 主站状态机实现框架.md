# 人形机器人 EtherCAT 主站状态机实现框架

## 1. 文档定位

本文档细化《人形机器人 EtherCAT 主站状态机设计》，给出主站状态机在代码中的实际框架、核心接口、调用链和与现有模块的关系。

本文档关注“状态机如何被调用”，不展开 SOEM、PDO、SDO、DC 的底层细节。

## 2. 实现结论

建议自己实现一个轻量级、表驱动的状态机，不引入开源状态机组件。

理由：

- 主站状态数量少，迁移关系清晰。
- 状态机属于安全边界，必须便于审查、打日志和做故障快照。
- 开源状态机库会增加模板和抽象复杂度，不利于现场问题定位。
- 项目已有 `EcatController`、`ActivityScheduler`、`ProcessDataEngine`、`EcatApplication` 边界，适合直接扩展。

推荐结构：

| 模块 | 职责 |
|------|------|
| `MasterStateMachine` | 保存当前状态、检查迁移表、统一执行动作 |
| `MasterContext` | 执行动作的上下文接口，由 `EcatController` 实现或持有 |
| `EcatController` | 对外门面，暴露 `InitializeAdapter` / `Scan` / `PrepareRun` / `StartOperation` 等接口 |
| `EcatApplication` | 命令分发，根据当前状态调用 `EcatController` |
| `ProcessDataEngine` | OP 后周期执行 PDO 和状态监控，异常时请求 Fault |
| `ActivityScheduler` | Maintenance 下执行 SDO、诊断、参数等维护活动 |

## 3. 总体调用链

主调用链如下：

```text
main()
  -> 创建 EcatController
  -> 创建 ActivityScheduler(controller)
  -> 创建 ProcessDataEngine(controller, master, slave_manager)
  -> 创建 EcatApplication(command_reader, controller, scheduler, engine)
  -> app.Initialize(config)
       -> controller.InitializeAdapter(config)
            -> state_machine.Dispatch(kInitializeAdapter)
  -> while running:
       -> app.Run()
            -> 读取命令
            -> 按 controller.GetState() 分发
            -> 调 controller.Scan / EnterMaintenance / PrepareRun / StartOperation / Stop
            -> Maintenance: engine.CheckSlaveStates()
            -> Operational: engine.RunOnce() + engine.CheckSlaveStates()
```

异常调用链：

```text
ProcessDataEngine 检测到 WKC / 掉站 / DC / 从站状态异常
  -> controller.RequestFault(reason)
       -> state_machine.Dispatch(kRequestFault, reason)
            -> StopRealtimeCycle()
            -> CaptureFaultSnapshot()
            -> state = Fault

外部急停 / STO / 硬限位 / 安全控制器事件
  -> controller.RequestEmergencyStop(reason)
       -> state_machine.Dispatch(kRequestEmergencyStop, reason)
            -> TriggerEmergencyStop()
            -> CaptureFaultSnapshot()
            -> state = EmergencyStop
```

## 4. 状态和动作定义

```cpp
enum class MasterState {
    kUninitialized,
    kAdapterReady,
    kScanned,
    kMaintenance,
    kReadyToRun,
    kOperational,
    kFault,
    kEmergencyStop,
};

enum class MasterAction {
    kInitializeAdapter,
    kScanSlaves,
    kEnterMaintenance,
    kPrepareRun,
    kStartOperation,
    kBackToMaintenance,
    kStop,
    kRequestFault,
    kRequestEmergencyStop,
};
```

说明：

- `MasterState` 只表达稳定工况。
- `MasterAction` 表达一次状态迁移请求。
- PDO 配置、DC 配置、SAFEOP 等是 `kPrepareRun` 的内部步骤，不单独成为主站状态。

## 5. 迁移表

```cpp
struct TransitionRule {
    MasterState from;
    MasterAction action;
    MasterState to;
};

static constexpr TransitionRule kTransitionRules[] = {
    {MasterState::kUninitialized, MasterAction::kInitializeAdapter, MasterState::kAdapterReady},
    {MasterState::kAdapterReady,  MasterAction::kScanSlaves,        MasterState::kScanned},
    {MasterState::kScanned,       MasterAction::kEnterMaintenance,  MasterState::kMaintenance},
    {MasterState::kMaintenance,   MasterAction::kPrepareRun,        MasterState::kReadyToRun},
    {MasterState::kReadyToRun,    MasterAction::kStartOperation,    MasterState::kOperational},

    {MasterState::kOperational,   MasterAction::kBackToMaintenance, MasterState::kMaintenance},
    {MasterState::kReadyToRun,    MasterAction::kBackToMaintenance, MasterState::kMaintenance},

    {MasterState::kAdapterReady,  MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kScanned,       MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kMaintenance,   MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kReadyToRun,    MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kOperational,   MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kFault,         MasterAction::kStop,              MasterState::kUninitialized},
    {MasterState::kEmergencyStop, MasterAction::kStop,              MasterState::kUninitialized},
};
```

特殊规则：

- `kRequestFault` 可从任意非 `EmergencyStop` 状态进入 `Fault`。
- `kRequestEmergencyStop` 可从任意状态进入 `EmergencyStop`。
- `EmergencyStop` 默认只允许 `Stop` 回到 `Uninitialized`。
- `Fault -> Maintenance` 是否开放，应作为受限恢复路径单独实现，不默认开放。

## 6. 状态机核心类

```cpp
class MasterContext {
public:
    virtual ~MasterContext() = default;

    virtual bool InitializeAdapter() = 0;
    virtual bool ScanSlaves() = 0;
    virtual bool EnterMaintenance() = 0;
    virtual bool PrepareRun() = 0;
    virtual bool StartOperation() = 0;
    virtual bool BackToMaintenance() = 0;
    virtual bool Stop() = 0;

    virtual void StopRealtimeCycle() = 0;
    virtual void TriggerEmergencyStop(const std::string& reason) = 0;
    virtual void CaptureFaultSnapshot(const std::string& reason) = 0;
    virtual void LogTransition(MasterState from,
                               MasterAction action,
                               MasterState to,
                               const std::string& reason) = 0;
    virtual void LogRejected(MasterState state,
                             MasterAction action,
                             const std::string& reason) = 0;
};

class MasterStateMachine {
public:
    explicit MasterStateMachine(MasterContext& context)
        : context_(context) {}

    MasterState GetState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    bool Dispatch(MasterAction action, std::string reason = {}) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (action == MasterAction::kRequestEmergencyStop) {
            EnterEmergencyStop(std::move(reason));
            return true;
        }

        if (action == MasterAction::kRequestFault) {
            EnterFault(std::move(reason));
            return true;
        }

        const auto next = FindNextState(state_, action);
        if (!next.has_value()) {
            context_.LogRejected(state_, action, "invalid transition");
            return false;
        }

        const MasterState old_state = state_;

        if (!ExecuteAction(action)) {
            EnterFault("action failed");
            return false;
        }

        state_ = *next;
        context_.LogTransition(old_state, action, state_, reason);
        return true;
    }

private:
    std::optional<MasterState> FindNextState(MasterState from,
                                             MasterAction action) const {
        for (const auto& rule : kTransitionRules) {
            if (rule.from == from && rule.action == action) {
                return rule.to;
            }
        }
        return std::nullopt;
    }

    bool ExecuteAction(MasterAction action) {
        switch (action) {
        case MasterAction::kInitializeAdapter:
            return context_.InitializeAdapter();
        case MasterAction::kScanSlaves:
            return context_.ScanSlaves();
        case MasterAction::kEnterMaintenance:
            return context_.EnterMaintenance();
        case MasterAction::kPrepareRun:
            return context_.PrepareRun();
        case MasterAction::kStartOperation:
            return context_.StartOperation();
        case MasterAction::kBackToMaintenance:
            return context_.BackToMaintenance();
        case MasterAction::kStop:
            return context_.Stop();
        default:
            return false;
        }
    }

    void EnterFault(std::string reason) {
        const MasterState old_state = state_;
        context_.StopRealtimeCycle();
        context_.CaptureFaultSnapshot(reason);
        state_ = MasterState::kFault;
        context_.LogTransition(old_state, MasterAction::kRequestFault, state_, reason);
    }

    void EnterEmergencyStop(std::string reason) {
        const MasterState old_state = state_;
        context_.TriggerEmergencyStop(reason);
        context_.CaptureFaultSnapshot(reason);
        state_ = MasterState::kEmergencyStop;
        context_.LogTransition(old_state,
                               MasterAction::kRequestEmergencyStop,
                               state_,
                               reason);
    }

private:
    MasterContext& context_;
    mutable std::mutex mutex_;
    MasterState state_{MasterState::kUninitialized};
};
```

## 7. `EcatController` 如何接入

`EcatController` 建议作为 `MasterContext` 的实现者，同时对上层隐藏状态机细节。

```cpp
class EcatController : public MasterContext {
public:
    EcatController()
        : state_machine_(*this) {}

    bool InitializeAdapter(const EcMasterConfig& config) {
        config_ = config;
        return state_machine_.Dispatch(MasterAction::kInitializeAdapter);
    }

    bool Scan() {
        return state_machine_.Dispatch(MasterAction::kScanSlaves);
    }

    bool EnterMaintenanceState() {
        return state_machine_.Dispatch(MasterAction::kEnterMaintenance);
    }

    bool PrepareRunState() {
        return state_machine_.Dispatch(MasterAction::kPrepareRun);
    }

    bool StartOperationState() {
        return state_machine_.Dispatch(MasterAction::kStartOperation);
    }

    bool BackToMaintenanceState() {
        return state_machine_.Dispatch(MasterAction::kBackToMaintenance);
    }

    void StopController() {
        state_machine_.Dispatch(MasterAction::kStop);
    }

    void RequestFault(const std::string& reason) {
        state_machine_.Dispatch(MasterAction::kRequestFault, reason);
    }

    void RequestEmergencyStop(const std::string& reason) {
        state_machine_.Dispatch(MasterAction::kRequestEmergencyStop, reason);
    }

    MasterState GetState() const {
        return state_machine_.GetState();
    }

private:
    bool InitializeAdapter() override;
    bool ScanSlaves() override;
    bool EnterMaintenance() override;
    bool PrepareRun() override;
    bool StartOperation() override;
    bool BackToMaintenance() override;
    bool Stop() override;

    void StopRealtimeCycle() override;
    void TriggerEmergencyStop(const std::string& reason) override;
    void CaptureFaultSnapshot(const std::string& reason) override;
    void LogTransition(MasterState from,
                       MasterAction action,
                       MasterState to,
                       const std::string& reason) override;
    void LogRejected(MasterState state,
                     MasterAction action,
                     const std::string& reason) override;

private:
    MasterStateMachine state_machine_;
    EcMaster master_;
    SlaveNodeManager slave_manager_;
    EcMasterConfig config_;
};
```

注意：

- 对外方法负责提交动作。
- 私有 override 方法负责真正执行动作。
- 任何模块都不应直接写 `state_`。
- `ProcessDataEngine` 和 `ActivityScheduler` 只能通过 `RequestFault` 等接口请求状态变化。

## 8. `EcatController` 动作实现

### 8.1 初始化网卡

```cpp
bool EcatController::InitializeAdapter() {
    return master_.Initialize(config_);
}
```

成功后状态机自动从 `Uninitialized` 迁移到 `AdapterReady`。

### 8.2 扫描从站

```cpp
bool EcatController::ScanSlaves() {
    const auto slaves = master_.ScanSlaves();
    if (slaves.empty()) {
        return false;
    }

    if (!slave_manager_.BuildFromScanResult(slaves, config_)) {
        return false;
    }

    return slave_manager_.VerifyTopology() &&
           slave_manager_.VerifyProfiles();
}
```

成功后状态机迁移到 `Scanned`。

### 8.3 进入维护

```cpp
bool EcatController::EnterMaintenance() {
    if (!master_.RequestPreOpState()) {
        return false;
    }

    if (!master_.CheckAllRequiredSlavesInState(EC_STATE_PRE_OP)) {
        return false;
    }

    return slave_manager_.EnterMaintenance(master_);
}
```

成功后状态机迁移到 `Maintenance`。

### 8.4 准备运行

```cpp
bool EcatController::PrepareRun() {
    if (!slave_manager_.PrepareAllForRun(master_)) {
        return false;
    }

    if (!master_.ConfigureProcessData()) {
        return false;
    }

    if (config_.use_dc && !master_.ConfigureDc()) {
        return false;
    }

    if (!master_.WriteSafeInitialOutputs()) {
        return false;
    }

    if (!master_.RequestSafeOpState()) {
        return false;
    }

    if (!master_.CheckAllRequiredSlavesInState(EC_STATE_SAFE_OP)) {
        return false;
    }

    return slave_manager_.AllRequiredReadyToRun();
}
```

成功后状态机迁移到 `ReadyToRun`。

### 8.5 开始 OP

```cpp
bool EcatController::StartOperation() {
    if (!realtime_runner_.Start()) {
        return false;
    }

    if (!master_.RequestOperationalState()) {
        realtime_runner_.Stop();
        return false;
    }

    if (!master_.CheckAllRequiredSlavesInState(EC_STATE_OPERATIONAL)) {
        realtime_runner_.Stop();
        return false;
    }

    if (!master_.VerifyWorkingCounter()) {
        realtime_runner_.Stop();
        return false;
    }

    if (config_.use_dc && !master_.VerifyDcSyncWindow()) {
        realtime_runner_.Stop();
        return false;
    }

    return true;
}
```

成功后状态机迁移到 `Operational`。

### 8.6 停止

```cpp
bool EcatController::Stop() {
    realtime_runner_.Stop();

    if (master_.IsOpen()) {
        master_.RequestSafeOpState();
        master_.RequestPreOpState();
        master_.RequestInitState();
        master_.Close();
    }

    slave_manager_.Clear();
    return true;
}
```

成功后状态机迁移到 `Uninitialized`。

## 9. `EcatApplication` 如何调用

`EcatApplication` 不直接改状态，只根据当前状态决定允许哪些命令。

```cpp
bool EcatApplication::Run() {
    std::string command;
    const bool has_command = command_reader_->TryRead(command);

    switch (controller_.GetState()) {
    case MasterState::kAdapterReady:
        if (has_command && command == "scan") {
            controller_.Scan();
        }
        break;

    case MasterState::kScanned:
        if (has_command && command == "config") {
            controller_.EnterMaintenanceState();
        }
        break;

    case MasterState::kMaintenance:
        engine_.CheckSlaveStates();

        if (has_command && command == "prepare") {
            controller_.PrepareRunState();
        } else if (has_command && command == "diagnose") {
            scheduler_.Execute(MakeDiagnoseActivity());
        }
        break;

    case MasterState::kReadyToRun:
        if (has_command && command == "start") {
            controller_.StartOperationState();
        } else if (has_command && command == "back") {
            controller_.BackToMaintenanceState();
        }
        break;

    case MasterState::kOperational:
        engine_.RunOnce();
        engine_.CheckSlaveStates();

        if (has_command && command == "stop") {
            controller_.StopController();
        }
        break;

    case MasterState::kFault:
        if (has_command && command == "stop") {
            controller_.StopController();
        }
        break;

    case MasterState::kEmergencyStop:
        if (has_command && command == "stop") {
            controller_.StopController();
        }
        break;

    default:
        break;
    }

    return true;
}
```

命令建议：

| 命令 | 状态 | 调用 |
|------|------|------|
| `scan` | `AdapterReady` | `controller.Scan()` |
| `config` | `Scanned` | `controller.EnterMaintenanceState()` |
| `prepare` | `Maintenance` | `controller.PrepareRunState()` |
| `start` | `ReadyToRun` | `controller.StartOperationState()` |
| `back` | `ReadyToRun` / `Operational` | `controller.BackToMaintenanceState()` |
| `stop` | 多数状态 | `controller.StopController()` |
| `diagnose` | `Maintenance` / `Fault` | `scheduler.Execute(activity)` |

## 10. `ProcessDataEngine` 如何请求故障

周期模块不能直接改状态，只能请求主站进入故障。

```cpp
void ProcessDataEngine::CheckSlaveStates() {
    slave_manager_.RefreshRuntimeHealth(master_);

    if (slave_manager_.HasSafetyCriticalFault()) {
        controller_.RequestEmergencyStop("safety critical slave fault");
        return;
    }

    if (!slave_manager_.AllRequiredHealthy()) {
        controller_.RequestFault("required slave unhealthy");
        return;
    }

    if (controller_.GetState() == MasterState::kOperational) {
        if (!master_.VerifyWorkingCounter()) {
            controller_.RequestFault("working counter mismatch");
            return;
        }

        if (config_.use_dc && !master_.VerifyDcSyncWindow()) {
            controller_.RequestFault("DC sync out of window");
            return;
        }
    }
}
```

## 11. `ActivityScheduler` 如何调用

维护活动也不能直接改状态，只能通过策略上报。

```cpp
bool ActivityScheduler::Execute(std::unique_ptr<EcatActivity> activity) {
    if (!activity->CanStart(controller_.GetState())) {
        return false;
    }

    const bool ok = activity->Execute();
    if (ok) {
        return true;
    }

    switch (activity->FailurePolicy()) {
    case ActivityFailurePolicy::kKeepControllerState:
        return false;
    case ActivityFailurePolicy::kEnterFault:
        controller_.RequestFault(activity->Name());
        return false;
    case ActivityFailurePolicy::kEmergencyStop:
        controller_.RequestEmergencyStop(activity->Name());
        return false;
    case ActivityFailurePolicy::kShutdown:
        controller_.StopController();
        return false;
    }

    return false;
}
```

## 12. 与当前代码的演进关系

当前项目已按该框架完成核心收敛：

- `main.cpp` 创建 `EcatController`、`ActivityScheduler`、`ProcessDataEngine`、`EcatApplication`。
- `EcatApplication::Run()` 根据 `ControllerState` 分发命令。
- `EcatController::Dispatch()` 是唯一状态机动作入口。
- `EcatController::kAllowedActions` 是动作迁移表。
- `ProcessDataEngine` 可以通过 `RequestFault()` 上报普通故障。
- `ActivityScheduler` 可以根据 Activity 失败策略请求 `Fault` 或 `Stop`。

已完成的演进：

| 原状态 | 当前状态 |
|------|----------|
| `ControllerState` 6 状态 | 8 状态，包含 `ReadyToRun`、`Fault`、`EmergencyStop` |
| `TransitionTo(target)` | 已移除，统一使用 `Dispatch(action, reason)` |
| `kAllowedTransitions` | 已移除，统一使用 `kAllowedActions` |
| `Maintenance -> Operational` 复合转换 | 已拆成 `Maintenance -> ReadyToRun -> Operational` |
| `kError` | 已替换为 `kFault` |
| `RequestErrorState` | 已替换为 `RequestFault` 和 `RequestEmergencyStop` |
| `StartOperation()` | 只负责 `ReadyToRun -> Operational` |
| `PrepareRun()` | 负责 PDO、IOmap、DC、SAFEOP、初始输出 |

## 13. 结语

实际实现时，状态机应成为主站所有生命周期动作的唯一入口。`main` 只负责循环调度，`EcatApplication` 只负责命令分发，`EcatController` 只通过状态机执行动作，`ProcessDataEngine` 和 `ActivityScheduler` 只能请求故障或停机。这样状态变化路径单一、可审查、可测试，也便于后续扩展人形机器人多轴安全策略。
