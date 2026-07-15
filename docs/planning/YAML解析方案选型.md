# YAML 配置解析方案选型

## 1. 需求

- 支持 X86 Ubuntu 等 PC 环境开发、调试。
- 支持嵌入式目标（如 Renesas RA8）——最终固件不引入运行期 YAML 解析开销。
- 项目主体为 C 代码，核心库尽量保持零运行期动态分配。
- 配置变更后，尽量做到“只改文件、不重写手写 C 代码”。

## 2. 主流做法对比

| 方案 | 原理 | X86 适用性 | 嵌入式适用性 | 优点 | 缺点 |
|---|---|---|---|---|---|
| **A. 运行期解析库** | 目标程序内置 `libyaml`，启动时读 YAML 文件并填充 C 结构体 | 很好 | 差 | 现场换配置无需重编；调试灵活 | 目标端需链接解析库；增加 Flash/RAM；运行期分配/失败处理复杂；嵌入式负担大 |
| **B. 代码生成工具** | 在**宿主机**运行一个可执行程序，把 YAML 转成 `.c/.h`，随项目一起编译 | 很好 | 很好 | 目标端零解析开销；配置在编译期固化；可利用编译器做类型和数组边界检查 | 换配置后需重新运行工具并编译；需要维护生成器 |
| **C. 混合方案** | 以 B 为主，X86 上可额外启用 A 作为可选运行时加载 | 最好 | 很好 | 兼顾嵌入式精简与 PC 灵活性 | 代码量最大；需要维护两套路径 |

结论：**优先采用方案 B（代码生成工具）**。它最契合“嵌入式可用、核心库零运行期动态分配”的目标；PC 上同样适用，只是每次改 YAML 后需要重新生成并编译。

---

## 3. 方案 B：代码生成工具（详细设计）

### 3.1 核心思想

把 YAML 从“运行期数据”变成“编译期数据源”。

```
YAML 文件
    │
    ▼
┌─────────────────────┐
│  mo_ecat_config_gen │   宿主机可执行程序
│  输入：YAML          │
│  输出：.c / .h       │
└─────────────────────┘
    │
    ▼
build/generated/*.c / *.h
    │
    ▼
交叉编译 / 本地编译
    │
    ▼
目标固件 / ecat_cli
```

目标程序里没有任何 YAML 解析器、没有文件 I/O、没有动态分配。所有配置以 `static const` 形式存在，直接参与编译和链接。

### 3.2 为什么选方案 B

1. **嵌入式零负担**
   - 不需要在目标端链接 `libyaml`。
   - 不需要为目标端分配解析缓冲区。
   - 配置大小在编译期已知，可放入 Flash，不占用运行时堆。

2. **与核心库设计一致**
   - 核心库强调“运行期零动态分配”。运行期解析 YAML 必然引入动态分配或大规模栈缓冲，与现有设计冲突。
   - 代码生成后，所有结构体都是固定大小数组或指向 `static const` 的指针，完全遵循现有 C 模型。

3. **错误前置**
   - YAML 格式错误、字段越界、从站重复等问题在**构建阶段**就暴露，而不是在目标设备启动后才出错。
   - 生成器可以给出精确的行号和错误原因。

4. **类型安全**
   - 生成的 `.c` 直接初始化标准 C 结构体，编译器会检查类型、枚举、数组长度。
   - 运行期解析 void* / 字符串转枚举的方案无法做到这一点。

5. **多平台一致**
   - 同一份 YAML 生成的 C 文件，可以在 X86、ARM、Renesas RA8 上无差别编译。
   - 不需要为不同平台维护不同的解析逻辑。

### 3.3 系统架构

#### 3.3.1 文件分层

```
src/config/master.yaml               # 核心库主站配置
examples/cli/config/robot.yaml       # CLI 机器人配置

/tools/config_gen/                  # 生成器源码
    main.c                          # 命令行入口
    yaml_loader.c / .h              # 基于 libyaml 的通用加载
    master_gen.c / .h               # 生成 master 相关 C/H
    robot_gen.c / .h                # 生成 robot 相关 C/H
    validator.c / .h                # YAML 校验
    codegen.c / .h                  # C 代码打印工具函数

/build/generated/                   # 生成产物
    mo_ecat_master_cfg.c
    mo_ecat_master_cfg.h
    robot_cfg.c
    robot_cfg.h
```

#### 3.3.2 生成器内部数据流

```
YAML 文件
    │
    ▼
libyaml → yaml_loader → 内存树（生成器内部临时结构）
    │
    ▼
validator ──错误─────▶ stderr + 非零退出
    │ 通过
    ▼
master_gen / robot_gen
    │
    ▼
codegen 输出 .c / .h
```

生成器内部允许使用动态分配（如链表、树），因为这只是宿主机工具，不受核心库零分配约束。

### 3.4 生成器命令行设计

统一工具，一个可执行文件处理所有 YAML：

```bash
mo_ecat_config_gen \
  --master-yaml src/config/master.yaml \
  --robot-yaml examples/cli/config/robot.yaml \
  --master-out-c build/generated/mo_ecat_master_cfg.c \
  --master-out-h build/generated/mo_ecat_master_cfg.h \
  --robot-out-c  build/generated/robot_cfg.c \
  --robot-out-h  build/generated/robot_cfg.h
```

也可拆成两个独立工具，便于单独测试：

```bash
mo_ecat_master_config_gen  src/config/master.yaml  build/generated/mo_ecat_master_cfg
mo_ecat_robot_config_gen   examples/cli/config/robot.yaml  build/generated/robot_cfg
```

**第一阶段建议先做一个统一工具**，后续若维护成本变高再拆分。

### 3.5 YAML Schema（与 C 模型对应）

#### 3.5.1 `src/config/master.yaml`

```yaml
version: "1.0"

master:
  interface: "eth0"          # 必填
  cycle_time_us: 1000        # 可选，默认 1000
  dc_enabled: false          # 可选，默认 false
  backend_type: "soem"       # 可选，默认 "soem"

bus:
  slaves:
    - alias: 0
      position: 1
      vendor_id: 0x00000766
      product_code: 0x00002001
      revision_number: 0x00000001
      name: "left_shoulder"
      dc_active: false
      pdo_entries:
        - index: 0x6040
          subindex: 0
          bit_length: 16
          direction: output
        - index: 0x6064
          subindex: 0
          bit_length: 32
          direction: input

backend:
  type: soem
  soem:
    iomap_size: 2048
```

映射关系：

| YAML 段 | C 结构体 |
|---|---|
| `master.*` | `struct mo_ecat_master_config` |
| `bus.slaves[]` | `struct mo_ecat_user_config` + `struct mo_ecat_slave_config[]` |
| `backend.soem.*` | `struct soem_backend_options` |

#### 3.5.2 `examples/cli/config/robot.yaml`

```yaml
version: "1.0"

robot:
  name: "humanoid"
  joints:
    - joint_name: "left_shoulder_pitch"
      group: "left_arm"
      identity:
        alias: 0
        position: 1
        vendor_id: 0x00000766
        product_code: 0x00002001
```

映射关系：

| YAML 段 | C 结构体 |
|---|---|
| `robot.name` | `struct robot_config.name` |
| `robot.joints[]` | `struct robot_joint_config[]` |
| `joints[].group` | `enum robot_group_id` |
| `joints[].identity` | `struct robot_slave_identity` |

### 3.6 生成的 C 代码结构

生成器不生成复杂逻辑，只生成**纯数据**。核心库和应用层按既有 API 使用这些数据。

#### 3.6.1 `mo_ecat_master_cfg.h`

```c
#ifndef MO_ECAT_MASTER_CFG_H
#define MO_ECAT_MASTER_CFG_H

#include "mo_ecat/mo_ecat_master.h"
#include "mo_ecat/mo_ecat_config.h"
#include "backend/soem/soem_options.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct mo_ecat_master_config g_master_config;
extern const struct mo_ecat_user_config    g_user_config;
extern const struct soem_backend_options   g_soem_options;

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_CFG_H */
```

#### 3.6.2 `mo_ecat_master_cfg.c`

```c
#include "mo_ecat_master_cfg.h"

static const struct mo_ecat_pdo_entry_config s_slave0_pdo_entries[] = {
    { 0x6040, 0, 16, MO_ECAT_PDO_DIRECTION_OUTPUT },
    { 0x6064, 0, 32, MO_ECAT_PDO_DIRECTION_INPUT  },
};

static const struct mo_ecat_slave_config s_slaves[] = {
    {
        .alias           = 0,
        .position        = 1,
        .vendor_id       = 0x00000766,
        .product_code    = 0x00002001,
        .revision_number = 0x00000001,
        .name            = "left_shoulder",
        .pdo_entries     = s_slave0_pdo_entries,
        .pdo_entry_count = sizeof(s_slave0_pdo_entries) / sizeof(s_slave0_pdo_entries[0]),
        .dc_active       = 0,
    },
};

const struct mo_ecat_master_config g_master_config = {
    .interface_name = "eth0",
    .cycle_time_us  = 1000,
    .dc_enabled     = 0,
    .backend_type   = "soem",
};

const struct mo_ecat_user_config g_user_config = {
    .slaves      = s_slaves,
    .slave_count = sizeof(s_slaves) / sizeof(s_slaves[0]),
};

const struct soem_backend_options g_soem_options = {
    .iomap_size = 2048,
};
```

#### 3.6.3 `robot_cfg.h`

```c
#ifndef ROBOT_CFG_H
#define ROBOT_CFG_H

#include "robot_config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct robot_config g_robot_config;

#ifdef __cplusplus
}
#endif

#endif /* ROBOT_CFG_H */
```

#### 3.6.4 `robot_cfg.c`

```c
#include "robot_cfg.h"

static const struct robot_joint_config s_joints[] = {
    {
        .joint_name = "left_shoulder_pitch",
        .group      = ROBOT_GROUP_LEFT_ARM,
        .identity   = {
            .alias        = 0,
            .position     = 1,
            .vendor_id    = 0x00000766,
            .product_code = 0x00002001,
        },
    },
};

const struct robot_config g_robot_config = {
    .name        = "humanoid",
    .joints      = s_joints,
    .joint_count = sizeof(s_joints) / sizeof(s_joints[0]),
};
```

### 3.7 生成器工作流程

```c
int main(int argc, char *argv[])
{
    /* 1. 解析命令行 */
    parse_args(argc, argv, &paths);

    /* 2. 加载 master.yaml */
    yaml_document_t *master_doc = yaml_load(paths.master_yaml);
    if (!master_doc) { print_error(...); return 1; }

    /* 3. 加载 robot.yaml */
    yaml_document_t *robot_doc = yaml_load(paths.robot_yaml);
    if (!robot_doc) { print_error(...); return 1; }

    /* 4. 校验 */
    if (validate_master(master_doc) != 0) return 1;
    if (validate_robot(robot_doc) != 0) return 1;
    if (cross_validate(master_doc, robot_doc) != 0) return 1;

    /* 5. 生成 C 文件 */
    generate_master_cfg(master_doc, paths.master_out_c, paths.master_out_h);
    generate_robot_cfg(robot_doc, paths.robot_out_c, paths.robot_out_h);

    return 0;
}
```

### 3.8 校验规则（构建期错误前置）

生成器应在输出任何 C 代码之前完成全部校验，失败时给出文件、行号和原因。

#### 3.8.1 master.yaml 校验

| 校验项 | 错误示例 | 处理方式 |
|---|---|---|
| `master.interface` 必填且非空 | 缺失 | 报错退出 |
| `master.cycle_time_us` 为正整数 | `0` 或字符串 | 报错退出 |
| `master.dc_enabled` 为布尔 | `yes` | 报错退出 |
| `bus.slaves[].position` 必填、非负、不重复 | 两个从站 `position=1` | 报错退出 |
| `bus.slaves[].vendor_id/product_code` 必填 | 缺失 | 报错退出 |
| `bus.slaves[].pdo_entries[]` 不超过 `MO_ECAT_MAX_PDO_ENTRIES` | 33 条 PDO | 报错退出 |
| `bus.slaves[].pdo_entries[].direction` 为 `output/input` | `out` | 报错退出 |
| `slave_count` 不超过 `MO_ECAT_MAX_SLAVES` | 17 个从站 | 报错退出 |
| `backend.type` 必须被支持 | `type: igh`（未实现） | 报错退出 |

#### 3.8.2 robot.yaml 校验

| 校验项 | 错误示例 | 处理方式 |
|---|---|---|
| `robot.joints[].joint_name` 必填、不重复 | 两个 `left_shoulder_pitch` | 报错退出 |
| `robot.joints[].group` 必须为合法枚举字符串 | `group: left_hang` | 报错退出 |
| `robot.joints[].identity.*` 必填 | 缺失 `vendor_id` | 报错退出 |
| `joint_count` 不超过 `MO_ROBOT_MAX_JOINTS` | 33 个关节 | 报错退出 |

#### 3.8.3 交叉校验（推荐）

- 机器人配置中的每个 `identity` 应在 `bus.slaves[]` 中能找到匹配的从站。
- 这能尽早发现“机器人拓扑配置了，但总线配置没配”的错误。
- 初期可先不做，等状态机完整后再加。

### 3.9 CMake 集成（X86 本地构建）

```cmake
# 1. 编译生成器（宿主机程序）
add_executable(mo_ecat_config_gen
    tools/config_gen/main.c
    tools/config_gen/yaml_loader.c
    tools/config_gen/validator.c
    tools/config_gen/master_gen.c
    tools/config_gen/robot_gen.c
    tools/config_gen/codegen.c
)
target_link_libraries(mo_ecat_config_gen yaml)

# 2. 生成文件路径
set(MASTER_CFG_C ${CMAKE_CURRENT_BINARY_DIR}/generated/mo_ecat_master_cfg.c)
set(MASTER_CFG_H ${CMAKE_CURRENT_BINARY_DIR}/generated/mo_ecat_master_cfg.h)
set(ROBOT_CFG_C  ${CMAKE_CURRENT_BINARY_DIR}/generated/robot_cfg.c)
set(ROBOT_CFG_H  ${CMAKE_CURRENT_BINARY_DIR}/generated/robot_cfg.h)

# 3. 自定义命令：YAML -> C/H
add_custom_command(
    OUTPUT ${MASTER_CFG_C} ${MASTER_CFG_H} ${ROBOT_CFG_C} ${ROBOT_CFG_H}
    COMMAND mo_ecat_config_gen
        --master-yaml ${CMAKE_SOURCE_DIR}/src/config/master.yaml
        --robot-yaml ${CMAKE_SOURCE_DIR}/examples/cli/config/robot.yaml
        --master-out-c ${MASTER_CFG_C}
        --master-out-h ${MASTER_CFG_H}
        --robot-out-c  ${ROBOT_CFG_C}
        --robot-out-h  ${ROBOT_CFG_H}
    DEPENDS
        mo_ecat_config_gen
        ${CMAKE_SOURCE_DIR}/src/config/master.yaml
        ${CMAKE_SOURCE_DIR}/examples/cli/config/robot.yaml
    COMMENT "Generating C configuration from YAML"
)

# 4. 把生成文件加入库/可执行文件
add_library(mo_ecat_generated_config STATIC
    ${MASTER_CFG_C}
    ${ROBOT_CFG_C}
)
target_include_directories(mo_ecat_generated_config PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
```

关键 CMake 行为：

- 修改 YAML 后，CMake 会自动重新运行生成器。
- 生成器失败（校验不通过）会导致构建失败，错误信息直接显示在终端。
- 生成文件放在 `build/generated/`，不污染源码目录。

### 3.10 交叉编译 / 嵌入式策略

难点：生成器必须在**宿主机**运行，而目标程序在**目标机**编译。常用三种策略：

#### 策略 1：CMake 自动编译宿主机生成器（推荐用于开发）

使用 `ExternalProject_Add` 或 `try_compile` 为宿主机单独编译一个生成器，不参与交叉编译：

```cmake
include(ExternalProject)
ExternalProject_Add(mo_ecat_config_gen_host
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tools/config_gen
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
    INSTALL_COMMAND ""
)
set(MO_ECAT_CONFIG_GEN ${CMAKE_BINARY_DIR}/mo_ecat_config_gen_host-prefix/src/mo_ecat_config_gen_host-build/mo_ecat_config_gen)
```

然后 `add_custom_command` 使用 `${MO_ECAT_CONFIG_GEN}` 而不是构建目标名。

#### 策略 2：环境变量指向预编译生成器

```cmake
if(DEFINED ENV{MO_ECAT_CONFIG_GEN})
    set(MO_ECAT_CONFIG_GEN $ENV{MO_ECAT_CONFIG_GEN})
else()
    set(MO_ECAT_CONFIG_GEN mo_ecat_config_gen)   # 默认从 PATH 找
endif()
```

适合 CI/CD：在 X86 构建机上预编译好生成器，嵌入交叉编译时直接调用。

#### 策略 3：预生成 C 文件（最稳妥，适合发布）

在发布前手动或 CI 自动运行一次生成器，把生成的 `.c/.h` 提交或打包到嵌入式源码树中。嵌入式构建完全不依赖 YAML、libyaml 和生成器。

```bash
# 在 X86 上预生成
mo_ecat_config_gen ... --master-out-c src/generated/mo_ecat_master_cfg.c ...

# 嵌入式构建直接编译 src/generated/*.c
```

**推荐组合**：

- 日常 X86 开发：策略 1 或 2，CMake 自动处理。
- 嵌入式发布：策略 3，确保目标源码自包含、无额外构建依赖。

### 3.11 与现有硬编码配置的 fallback

- 保留 CLI 的 `robot_default_config()`；主站使用核心库内建 `mo_ecat_master_config`。
- 当 `MO_ECAT_CONFIG_YAML` 关闭、或 YAML 文件不存在、或用户明确选择不使用生成器时，仍可走现有硬编码路径。
- 这样新方案可以逐步引入，不破坏当前 CLI 和测试。

### 3.12 版本与兼容性

- YAML 顶层必须包含 `version` 字段，例如 `version: "1.0"`。
- 生成器读取 `version`，不支持的未来版本直接报错。
- 向后兼容：小版本升级时，生成器保留对旧字段的解析；新增字段提供默认值。
- 生成的 `.c/.h` 顶部添加版本注释，方便追踪：
  ```c
  /* Auto-generated by mo_ecat_config_gen v1.0 from src/config/master.yaml */
  /* DO NOT EDIT MANUALLY */
  ```

### 3.13 局限与应对

| 局限 | 应对 |
|---|---|
| 每次改 YAML 都要重新编译 | 这是嵌入式固件的常见做法；X86 上如需要热更新，可后续叠加可选的运行时加载器 |
| 需要维护生成器代码 | 生成器逻辑稳定后变动很小；可用 Python 脚本快速验证生成器输出 |
| 增加 CMake 复杂度 | 提供预生成模式，让不熟悉 CMake 的用户也能直接用 |
| libyaml 依赖 | 只在宿主机工具中依赖，目标端完全无依赖 |

---

## 4. 其他方案简要说明

### 4.1 方案 A：运行期解析库

适合需要“不重启程序就能换配置”的 X86 调试场景。但会违反核心库零运行期动态分配原则，增加目标端依赖，不推荐作为默认方案。

### 4.2 方案 C：混合方案

在方案 B 基础上，X86 构建额外开启 `MO_ECAT_CONFIG_RUNTIME_LOADER`，提供一个运行时加载函数：

```c
#ifdef MO_ECAT_CONFIG_RUNTIME_LOADER
int mo_ecat_config_load_runtime(const char *path, struct mo_ecat_config *cfg);
#endif
```

建议**第一阶段不做**，等代码生成路径稳定后再评估是否需要。

---

## 5. 推荐结论

| 项目 | 建议 |
|---|---|
| 解析方案 | **方案 B：代码生成工具** |
| 生成器实现 | **C 可执行程序**，使用 `libyaml` |
| 输入 | `src/config/master.yaml` + `examples/cli/config/robot.yaml` |
| 输出 | `build/generated/mo_ecat_master_cfg.c/h`、`build/generated/robot_cfg.c/h` |
| 调用时机 | CMake 构建时自动调用；嵌入式发布可预生成 |
| 目标端依赖 | **无** YAML 解析器、无动态分配 |
| 扩展 | 未来 X86 可再叠加可选运行时加载器 |

---

## 6. 下一步可做的事情

1. 创建目录 `tools/config_gen/`。
2. 定义生成器命令行参数和输出文件名规范。
3. 把 `mo_ecat_user_config`、`mo_ecat_slave_config` 等核心配置模型公开到 `include/mo_ecat/mo_ecat_config.h`。
4. 实现最小可用的 `mo_ecat_config_gen`：先支持 `master.yaml` 里的 `interface` 和 `bus.slaves[]`，生成对应的 C 文件。
5. 在 CMake 中加入 `add_custom_command`，让 `ecat_cli` 构建时自动生成配置。
6. 改造 CLI，使其只链接自己的机器人配置；主站配置由核心库内部使用。
