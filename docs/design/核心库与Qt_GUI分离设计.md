# 核心库与 Qt GUI 分离设计

## 1. 文档定位

本文档定义 `MoDriverPC_EtherCAT` 如何从当前“CLI 程序工程”演进为：

- **`mo_ecat_core`**：纯 C++17 + SOEM 的 EtherCAT 主站核心库，不依赖 Qt，不输出可执行文件，可运行于 x86、RK3568 等无图形环境。
- **CLI 验证工具**：前期保留在核心仓库内作为 `examples/cli`，用于快速硬件回归；公共 API 稳定后再迁出为独立仓库或独立项目。
- **Qt GUI 项目**：独立仓库，通过核心库公共 C++ API 控制主站，不侵入核心库。
- **RK3568 控制项目**：独立项目，通过核心库公共 C++ API 构建无 GUI 控制程序。

本文档是后续拆分工程的执行依据。原则是：**核心库先库化，但 CLI 先留在仓库内验证；公共 API 分阶段成熟后，再拆出 CLI、启动 Qt GUI 和 RK3568 项目**，避免核心边界、线程模型和 GUI 问题混在一起。

当前核心库仍处于快速演进期，后续 CiA402、轴组、安全策略、实时接口都会持续变化。因此本文档不要求一次性冻结最终 API，而是要求公共 API 按阶段收敛。

## 2. 设计目标

1. **核心库零 Qt 依赖**：核心源码、公共头文件、CMake 中不得出现 Qt。
2. **核心库最终不输出业务可执行文件**：过渡期允许构建 `examples/cli`，用于硬件验证；稳定后 CLI 可迁出为独立项目。
3. **公共 API 稳定且收敛**：Qt GUI、RK3568 项目只依赖 `include/mo_ecat/`。
4. **内部实现不泄漏**：`EcatController`、`EcMaster`、`SlaveNodeManager` 等保持内部模块，不暴露给 GUI。
5. **线程边界明确**：核心周期任务、维护动作、GUI 主线程不能互相阻塞。
6. **可交叉编译**：核心库可通过 CMake toolchain file 编译到 RK3568。
7. **分阶段迁移**：每阶段都能编译和运行，不做一次性大拆。
8. **API 分阶段成熟**：核心内部允许持续重构，公共 API 先最小化，后续按能力阶段稳定。

## 3. 总体架构

```text
┌─────────────────────────────────────────────────────────────┐
│                    MoDriverPC_EtherCAT                       │
│                     核心库仓库                               │
│                                                             │
│  include/mo_ecat/       公共 API                             │
│  src/ec_controller/     主站状态机与生命周期                  │
│  src/ec_master/         SOEM 封装                             │
│  src/slave_node/        从站管理                              │
│  src/activity/          SDO/诊断/维护活动                     │
│  src/cyclic/            PDO 周期与状态监控                    │
│  examples/cli/          过渡期硬件验证工具                     │
└─────────────────────────────────────────────────────────────┘
             ▲                    ▲                    ▲
             │ link mo_ecat_core  │ link mo_ecat_core  │ link mo_ecat_core
             │                    │                    │
┌────────────┴──────────┐  ┌──────┴─────────────────┐  ┌────────────┴──────────────────┐
│ MoDriverPC_EtherCAT_GUI│  │ MoDriverPC_EtherCAT_CLI│  │ RobotController_RK3568         │
│ Qt Widgets / Qt Quick  │  │ 后期独立命令行验证工具   │  │ 无 GUI 控制程序 / ROS 节点       │
│ CoreWorker + MainWindow│  │ scan/config/prepare/start│ │ main / daemon                   │
└───────────────────────┘  └────────────────────────┘  └───────────────────────────────┘
```

## 4. 推荐仓库结构

### 4.1 核心库仓库

```text
MoDriverPC_EtherCAT/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── planning/
│   └── design/
├── include/mo_ecat/
│   ├── master.h
│   ├── config.h
│   └── types.h
├── src/
│   ├── core/
│   │   └── mo_ecat_master.cpp
│   ├── ec_controller/
│   ├── ec_master/
│   ├── slave_node/
│   ├── activity/
│   ├── cyclic/
│   ├── servo_axis/
│   └── utils/
├── examples/cli/                  # 过渡期保留，公共 API 稳定后可迁出
│   ├── main.cpp
│   ├── ecat_application.cpp/.h
│   ├── command_reader.h
│   └── stdin_command_reader.cpp/.h
├── tests/
└── third_party/SOEM/
```

说明：

- 过渡期保留 `examples/cli`，用于核心库每次改动后的硬件回归。
- 公共 API 稳定前，不建议立即拆出 CLI 仓库，避免多仓库同步成本过高。
- 公共 API 进入候选稳定后，再将 `examples/cli` 迁出到 CLI 独立仓库。
- `src/core/mo_ecat_master.cpp` 作为公共 API 的 PImpl 实现入口。
- 测试目录 `tests/` 用于核心单元测试和集成测试。

### 4.2 Qt GUI 仓库

```text
MoDriverPC_EtherCAT_GUI/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── main_window.cpp/.h/.ui
│   ├── core_worker.cpp/.h
│   ├── log_model.cpp/.h
│   ├── slave_table_model.cpp/.h
│   └── widgets/
├── resources/
└── third_party/mo_ecat_core/
```

### 4.3 CLI 验证工具仓库

```text
MoDriverPC_EtherCAT_CLI/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── ecat_application.cpp/.h
│   ├── command_reader.h
│   └── stdin_command_reader.cpp/.h
└── third_party/mo_ecat_core/     # git submodule -> MoDriverPC_EtherCAT
```

职责：

- 依赖核心库公共 API，不直接依赖 SOEM 内部头文件。
- 保留 `scan / config / prepare / start / back / stop / diagnose / pdo / param / inspect` 等命令。
- 在公共 API 稳定后，作为核心库修改后的独立硬件回归工具。
- 不依赖 Qt，可运行于任何能编译核心库的平台。

### 4.4 RK3568 项目

```text
RobotController_RK3568/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── cmake/
│   └── toolchain_rk3568.cmake
└── third_party/mo_ecat_core/
```

## 5. API 成熟度策略

核心库当前仍在快速演进期，公共 API 不应一次性承诺最终稳定。推荐按能力阶段定义 API 成熟度：

| 等级 | 含义 | 适用阶段 | 变更策略 |
|------|------|----------|----------|
| Experimental | 快速迭代 API，允许破坏性修改 | Phase 0~4：网卡、扫描、维护、ReadyToRun、OP 通信 | 以 CLI 验证为准，可直接调整 |
| Stable Candidate | 候选稳定 API，尽量保持兼容 | Phase 5~6：CiA402、轴组、安全停机 | 修改前评估 CLI/GUI/RK 影响 |
| Stable | 稳定 API，被 GUI/RK 正式依赖 | GUI/RK 正式接入后 | 破坏性修改需要版本升级或兼容层 |

当前建议：

- 生命周期 API 先进入 Experimental。
- SDO/PDO 诊断 API 可随 CLI 需求逐步进入 Stable Candidate。
- CiA402、轴组、实时控制接口暂不承诺稳定。
- GUI 早期只依赖基础生命周期、从站信息、快照和日志 API。

## 6. 核心库公共 API

公共 API 只放在 `include/mo_ecat/`，不得包含 SOEM、Qt 或内部模块头文件。

第一版公共 API 只覆盖 Phase 0~4：

- 网卡初始化。
- 从站扫描。
- PREOP 维护。
- ReadyToRun。
- OP 通信。
- 基础从站信息。
- 故障快照。

CiA402、轴组控制、实时上层接口后续单独扩展，不在第一版公共 API 中一次性冻结。

### 6.1 `types.h`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mo_ecat
{

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

struct SlaveIdentity {
    uint32_t vendor_id = 0;
    uint32_t product_id = 0;
    uint32_t revision_id = 0;
    uint32_t serial_id = 0;
    std::string name;
};

struct SlaveInfo {
    int slave_id = 0;
    uint16_t config_address = 0;
    uint16_t alias_address = 0;
    uint32_t vendor_id = 0;
    uint32_t product_id = 0;
    uint32_t revision_id = 0;
    uint32_t serial_id = 0;
    std::string name;
    bool supports_dc = false;
    uint16_t ethercat_state = 0;
    uint16_t al_status_code = 0;
    uint32_t output_bytes = 0;
    uint32_t input_bytes = 0;
};

struct SlaveFeedback {
    int slave_id = 0;
    std::string name;
    uint16_t status_word = 0;
    uint16_t error_code = 0;
    int32_t actual_position = 0;
    int32_t actual_velocity = 0;
    int16_t actual_torque = 0;
};

struct MasterSnapshot {
    MasterState state = MasterState::kUninitialized;
    std::vector<SlaveInfo> slaves;
    std::string last_fault;
};

} // namespace mo_ecat
```

### 6.2 `config.h`

```cpp
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

    int expected_slave_count = 0;
    std::vector<SlaveIdentity> expected_identities;

    LogSinkMode log_sink = LogSinkMode::kCallback;
    std::string log_path;

    int feedback_publish_hz = 20;
};

} // namespace mo_ecat
```

### 6.3 `master.h`

```cpp
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mo_ecat/config.h"
#include "mo_ecat/types.h"

namespace mo_ecat
{

class MoEcatMaster {
public:
    MoEcatMaster();
    ~MoEcatMaster();

    MoEcatMaster(const MoEcatMaster&) = delete;
    MoEcatMaster& operator=(const MoEcatMaster&) = delete;

    bool InitializeAdapter(const EcMasterConfig& config);
    bool Scan();
    bool EnterMaintenance();
    bool PrepareRun();
    bool StartOperation();
    bool BackToMaintenance();
    void Stop();

    void RequestFault(const std::string& reason);
    void RequestEmergencyStop(const std::string& reason);

    MasterState GetState() const;
    std::size_t GetSlaveCount() const;
    std::vector<SlaveInfo> GetSlaveInfos() const;
    MasterSnapshot GetSnapshot() const;

    bool ReadSdo(int slave_id, uint16_t index, uint8_t subindex,
                 std::vector<uint8_t>& data, int timeout_ms = 1000);
    bool WriteSdo(int slave_id, uint16_t index, uint8_t subindex,
                  const std::vector<uint8_t>& data, int timeout_ms = 1000);
    std::string DumpPdoMapping(int slave_id);

    std::function<void(MasterState old_state, MasterState new_state)>
        on_state_changed;

    std::function<void(const std::string& reason)>
        on_fault;

    std::function<void(const std::string& level,
                       const std::string& source,
                       const std::string& message)>
        on_log_message;

    std::function<void(const std::vector<SlaveFeedback>& feedback)>
        on_feedback;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mo_ecat
```

### 6.4 API 约束

- `StartOperation()` 只允许在 `ReadyToRun` 后调用。
- GUI 按钮顺序必须是 `Initialize -> Scan -> Config -> Prepare -> Start`。
- `on_feedback` 不保证每个 EtherCAT 周期触发，默认按 `feedback_publish_hz` 降频发布。
- 回调不得阻塞核心线程。
- 不得从回调中再次调用 `MoEcatMaster` 的阻塞 API，避免死锁或重入。
- 公共头文件不得暴露 `EcatController`、`EcMaster`、`SlaveNode`、SOEM 类型。

## 7. 核心库内部边界

| 模块 | 可见性 | 说明 |
|------|--------|------|
| `MoEcatMaster::Impl` | 内部 | 公共 API 与内部模块之间的适配层 |
| `EcatController` | 内部 | 主站状态机与生命周期 |
| `EcMaster` | 内部 | SOEM 封装 |
| `SlaveNodeManager` / `SlaveNode` | 内部 | 从站管理 |
| `ProcessDataEngine` | 内部 | PDO 周期与状态监控 |
| `ActivityScheduler` | 内部 | SDO、诊断、参数、PDO 映射等维护活动 |
| `DeviceProfile` / `ServoProfile` | 内部 | 设备语义与厂商适配 |
| `Logger` | 内部 | 可通过回调向外发送日志 |

## 8. CLI 验证工具

CLI 是核心库的第一验证对象。公共 API 稳定前，CLI 保留在核心仓库 `examples/cli/`；公共 API 进入 Stable Candidate 后，再迁出为独立仓库。

过渡期路径：

```text
MoDriverPC_EtherCAT/examples/cli/
```

稳定后仓库：

```text
MoDriverPC_EtherCAT_CLI/
```

目标程序：

```text
ecat_cli
```

职责：

- 验证核心库能独立编译和链接。
- 验证真实硬件基础流程。
- 保留 `scan / config / prepare / start / back / stop / diagnose / pdo / param / inspect`。
- 作为 Qt GUI 开发前的回归工具。
- 不依赖 Qt，可随核心库一起交叉编译到 RK3568。

CLI 调用关系：

```text
MoDriverPC_EtherCAT_CLI/src/main.cpp
  -> MoEcatMaster
  -> 命令解析
  -> 调用公共 API
```

阶段门禁：

- 在 Qt GUI 仓库开始前，CLI 必须能完成 Phase 0~4 的基础流程。

## 9. Qt GUI 架构

### 9.1 模块划分

| 模块 | 职责 |
|------|------|
| `CoreWorker` | 在独立 `QThread` 中持有 `MoEcatMaster` |
| `MainWindow` | 状态显示、按钮、菜单、布局 |
| `SlaveTableModel` | 从站列表和状态 |
| `LogModel` | 核心日志显示 |
| `FeedbackModel` | 降频后的反馈显示 |
| `StateIndicator` | 主站状态可视化 |

### 9.2 线程模型

```text
┌─────────────────────────────────────────┐
│             Qt GUI 主线程                │
│ MainWindow / Model / Widget              │
└─────────────────┬───────────────────────┘
                  │ signal / slot queued
┌─────────────────┴───────────────────────┐
│             CoreWorker 线程              │
│ 持有 MoEcatMaster                         │
│ 调用 Initialize / Scan / Prepare / Start  │
│ 接收核心回调                              │
└─────────────────────────────────────────┘
```

规则：

- GUI 主线程不直接调用核心 API。
- `Scan()`、`PrepareRun()`、`StartOperation()` 等阻塞 API 必须在 `CoreWorker` 线程执行。
- 核心回调进入 GUI 时必须使用 Qt queued connection。
- GUI 不直接调用 SOEM。
- GUI 不直接访问 `EcatController` 或 `SlaveNode`。

### 9.3 最小界面元素

- 网卡选择输入框。
- 状态指示灯：`Uninitialized / AdapterReady / Scanned / Maintenance / ReadyToRun / Operational / Fault / EmergencyStop`。
- 操作按钮：`Initialize / Scan / Config / Prepare / Start / Back / Stop`。
- 从站表：slave id、name、vendor、product、state、AL code。
- 日志视图。
- 反馈面板：位置、速度、力矩、状态字、错误码。

### 9.4 按钮状态门禁

| 主站状态 | 可用按钮 |
|----------|----------|
| `Uninitialized` | Initialize |
| `AdapterReady` | Scan / Stop |
| `Scanned` | Config / Stop |
| `Maintenance` | Prepare / Diagnose / SDO / PDO / Stop |
| `ReadyToRun` | Start / Back / Stop |
| `Operational` | Stop |
| `Fault` | Snapshot / Stop |
| `EmergencyStop` | Snapshot / Stop |

## 10. CMake 构建设计

### 10.1 核心库 CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(mo_ecat_core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MO_ECAT_BUILD_TESTS "Build tests" OFF)
option(MO_ECAT_BUILD_CLI "Build transitional CLI example" ON)

add_subdirectory(third_party/SOEM)

set(MO_ECAT_CORE_SOURCES
    src/core/mo_ecat_master.cpp
    src/activity/activity_scheduler.cpp
    src/activity/pdo_mapping_activity.cpp
    src/activity/sdo_diagnostics_activity.cpp
    src/activity/sdo_parameter_activity.cpp
    src/activity/state_inspection_activity.cpp
    src/ec_master/ec_master.cpp
    src/ec_controller/ec_controller.cpp
    src/cyclic/cyclic_runner.cpp
    src/cyclic/process_data_engine.cpp
    src/servo_axis/servo_axis.cpp
    src/slave_node/device_profile.cpp
    src/slave_node/slave_config_factory.cpp
    src/slave_node/slave_node.cpp
    src/slave_node/slave_node_manager.cpp
    src/slave_node/slave_type_detector.cpp
    src/utils/logger.cpp
)

add_library(mo_ecat_core STATIC ${MO_ECAT_CORE_SOURCES})

target_include_directories(mo_ecat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mo_ecat_core
    PUBLIC soem
    PRIVATE pthread
)

if(MO_ECAT_BUILD_CLI)
    add_executable(ecat_cli
        examples/cli/main.cpp
        examples/cli/ecat_application.cpp
        examples/cli/stdin_command_reader.cpp
    )
    target_link_libraries(ecat_cli PRIVATE mo_ecat_core)
endif()

if(MO_ECAT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

说明：

- 公共头文件不暴露 SOEM 类型；`soem` 链接关系可按实际 CMake 验证选择 `PUBLIC` 或 `PRIVATE`。静态库场景下若下游链接缺少 SOEM 符号，使用 `PUBLIC soem`。
- 过渡期核心仓库可通过 `MO_ECAT_BUILD_CLI` 构建 `examples/cli`。
- 公共 API 稳定后，CLI 可迁出为独立仓库，并将 `MO_ECAT_BUILD_CLI` 默认关闭或移除。
- 后续可增加 install/export package，但第一阶段不强制。

### 10.2 Qt GUI CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(mo_ecat_gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_subdirectory(third_party/mo_ecat_core)

add_executable(mo_ecat_gui
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/main_window.ui
    src/core_worker.cpp
    src/core_worker.h
    src/log_model.cpp
    src/log_model.h
    src/slave_table_model.cpp
    src/slave_table_model.h
    src/feedback_model.cpp
    src/feedback_model.h
)

target_link_libraries(mo_ecat_gui
    PRIVATE Qt6::Core Qt6::Widgets mo_ecat_core
)
```

### 10.3 RK3568 CMake

```cmake
add_subdirectory(third_party/mo_ecat_core)

add_executable(robot_controller src/main.cpp)
target_link_libraries(robot_controller PRIVATE mo_ecat_core)
```

编译示例：

```bash
cmake -S . -B build-rk3568 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_rk3568.cmake \
  -DMO_ECAT_BUILD_CLI=OFF \
  -DMO_ECAT_BUILD_TESTS=OFF
cmake --build build-rk3568
```

## 11. 回调与数据发布

### 11.1 状态变化

状态迁移成功后发布：

```cpp
on_state_changed(old_state, new_state);
```

要求：

- 不在回调里执行阻塞动作。
- GUI 通过 queued connection 切回主线程。

### 11.2 日志

核心日志应支持可配置 sink：

- 不输出。
- 文件输出。
- 回调输出。
- 文件 + 回调。

核心库不得默认写固定相对路径，必须由 `EcMasterConfig::log_path` 指定。

### 11.3 反馈

PDO 周期可能是 1ms，GUI 不应每周期刷新。

策略：

- 核心内部每周期更新最新反馈快照。
- 按 `feedback_publish_hz` 降频触发 `on_feedback`。
- GUI 只显示降频后的反馈。
- 实时控制接口不要依赖 GUI 回调。

## 12. 隔离规则

核心库必须满足：

- [ ] 不 `#include` Qt 头文件。
- [ ] 不 `find_package(Qt)`。
- [ ] 不调用 GUI、X11、OpenGL 相关 API。
- [ ] 公共头文件只位于 `include/mo_ecat/`。
- [ ] 公共头文件不暴露 SOEM 类型。
- [ ] 内部头文件只作为 `PRIVATE` include。
- [ ] 过渡期 CLI 只通过 `MoEcatMaster` 公共 API 调用核心能力。
- [ ] 公共 API 稳定后，CLI 不参与核心仓库构建。

Qt GUI 必须满足：

- [ ] 不直接调用 SOEM。
- [ ] 不直接包含 `src/` 下内部头文件。
- [ ] 不直接修改主站状态。
- [ ] 所有核心 API 通过 `CoreWorker` 线程调用。
- [ ] 所有 UI 更新回到 GUI 主线程。

## 13. 分阶段迁移步骤

### Phase 1：核心库目标拆分，CLI 暂留仓内

目标：

- 新增 `add_library(mo_ecat_core STATIC ...)`。
- 将原 `src/main.cpp` 和 `src/app/` 移到 `examples/cli/`。
- CLI 暂时保留在核心仓库，链接 `mo_ecat_core`，保持硬件验证闭环。
- 不改变现有业务行为。

动作：

1. 将核心源文件放入 `MO_ECAT_CORE_SOURCES`。
2. 将 `src/main.cpp` 和 `src/app/` 迁移到 `examples/cli/`。
3. 核心仓库 `CMakeLists.txt` 构建 `mo_ecat_core` 和过渡期 `ecat_cli`。
4. 编译验证核心库和 CLI。

通过标准：

```bash
cmake -S . -B build
cmake --build build
# 生成 libmo_ecat_core.a 和过渡期 ecat_cli
```

同时，CLI 硬件验证：

```bash
sudo ./build/ecat_cli <interface>
```

### Phase 2：公共头文件与 `MoEcatMaster`

目标：

- 新增 `include/mo_ecat/`。
- 新增 `MoEcatMaster` PImpl。
- `examples/cli` 改为调用 `MoEcatMaster`，不再直接组装 `EcatController`、`ActivityScheduler`、`ProcessDataEngine`。

动作：

1. 添加 `types.h`、`config.h`、`master.h`。
2. 实现 `src/core/mo_ecat_master.cpp`。
3. 在 Impl 中持有 `EcatController`、`ActivityScheduler`、`ProcessDataEngine`。
4. 更新 `examples/cli`，让其调用公共 API。

通过标准：

- `examples/cli` 不再包含核心仓库 `src/` 下的内部头文件。
- GUI/RK 项目只需要 `include/mo_ecat/`。
- CLI 硬件流程仍通过。

### Phase 3：CLI 迁出为独立仓库

目标：

- 当基础生命周期 API 进入 Stable Candidate 后，将 CLI 迁出为独立仓库。
- CLI 成为核心库的第一个外部消费者。

动作：

1. 创建 `MoDriverPC_EtherCAT_CLI` 仓库。
2. 添加核心库为 submodule。
3. 迁移 `examples/cli` 到 CLI 仓库。
4. CLI 只包含 `include/mo_ecat/` 公共头文件。
5. 核心仓库中关闭或移除 `MO_ECAT_BUILD_CLI`。

通过标准：

- CLI 独立仓库可编译。
- CLI 独立仓库可完成 `scan -> config -> prepare -> start`。
- CLI 不包含核心仓库 `src/` 下的内部头文件。

### Phase 4：日志、快照、诊断 API

目标：

- GUI 需要的调试信息通过公共 API 获取。
- 不让 GUI 访问内部类。

动作：

1. 增加 `GetSnapshot()`。
2. 增加 `GetSlaveInfos()`。
3. 增加 SDO 读写 API。
4. 增加 PDO 映射 dump API。
5. 增加日志回调 sink。

通过标准：

- CLI 的 `diagnose / pdo / param / inspect` 通过公共 API 实现。
- CLI 不直接包含核心仓库内部头文件。

### Phase 5：反馈发布与线程策略

目标：

- 支持 GUI 显示反馈，但不影响实时周期。

动作：

1. 核心内部维护反馈快照。
2. 按 `feedback_publish_hz` 调用 `on_feedback`。
3. 明确回调不阻塞。
4. 必要时引入线程安全队列或双缓冲。

通过标准：

- OP 下 GUI/CLI 读取反馈不会放大周期 jitter。
- 回调异常不影响主站状态机。

### Phase 6：Qt GUI 独立仓库

目标：

- 新建 GUI 仓库并以 submodule 引入核心。

动作：

1. 创建 `MoDriverPC_EtherCAT_GUI`。
2. 添加核心库 submodule。
3. 实现 `CoreWorker`。
4. 实现最小 UI。
5. 支持基础流程按钮。

通过标准：

- GUI 不包含核心 `src/` 头文件。
- GUI 不直接调用 SOEM。
- GUI 能完成 `Initialize -> Scan -> Config -> Prepare`。
- OP 失败时能显示 Fault 与从站快照。

### Phase 7：RK3568 验证

目标：

- 在 RK3568 项目中复用核心库。

动作：

1. 添加核心库 submodule。
2. 编写 toolchain file。
3. 编写无 GUI 入口。
4. 交叉编译。
5. 上板运行基础流程。

通过标准：

- 核心库能交叉编译。
- RK3568 上可完成 Phase 0~2。
- 后续逐步验证 Phase 3~4。

## 14. 风险与对策

| 风险 | 说明 | 对策 |
|------|------|------|
| GUI 线程阻塞 | `Scan()` / `PrepareRun()` 可能耗时 | 只在 `CoreWorker` 线程调用 |
| 回调阻塞周期 | 日志或反馈回调太重 | 降频、队列、快照，不在实时路径阻塞 |
| 公共 API 不够 | GUI 为了功能包含内部头文件 | 先补公共 API，不允许绕过 |
| SOEM 依赖泄漏 | GUI 需要 SOEM include | 公共头文件不出现 SOEM 类型 |
| 日志路径不可写 | RK3568 无固定日志目录 | 日志路径配置化 |
| 过早拆出 CLI | 公共 API 未稳定时多仓库同步成本高 | CLI 先作为 `examples/cli` 过渡，稳定后迁出 |
| CLI 长期留在核心仓库 | 核心仓库边界不够纯粹 | 进入 Stable Candidate 后迁出 CLI |
| 核心拆分过大 | 一次性改动难验证 | 按 Phase 拆分，每阶段编译和硬件验证 |

## 15. 不建议做法

- 不建议一边核心库化一边做 Qt GUI。
- 不建议 GUI 直接包含 `src/ec_controller/ec_controller.h`。
- 不建议 GUI 主线程直接调用 `Scan()` / `PrepareRun()`。
- 不建议每 1ms 通过 Qt 刷新一次反馈表格。
- 不建议核心库默认写固定相对路径日志。
- 不建议在公共 API 未稳定前把 CLI 过早拆成独立仓库。
- 不建议公共 API 稳定后仍让 CLI 长期包含核心内部头文件。
- 不建议删除 CLI 验证工具本身（应作为独立仓库存在）。

## 16. 结语

核心库与 Qt GUI 分离是可行且必要的。正确顺序是：

```text
先拆核心库
  -> 通过仓内 examples/cli 验证
  -> 收敛公共 API
  -> CLI 迁出为独立仓库
  -> 再接 Qt GUI
  -> 最后做 RK3568 项目复用
```

这样可以保证主站核心能力、GUI 交互和嵌入式部署三条线互不污染，也能让每一步都有明确的编译和硬件验证门禁。
