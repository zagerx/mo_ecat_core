# 人形机器人 EtherCAT 主站运行流程设计

## 1. 文档定位

本文档定义人形机器人 EtherCAT 主站从环境验证、总线扫描、PREOP 维护、ReadyToRun、OP 周期通信、CiA402 伺服使能、轴组控制到整机运行的**分阶段实施流程**。

本文档目标不是一次性描述最终状态，而是给出一条可测试、可验收、尽量不走回头路的演进路线。每个阶段都必须能独立验证，通过后再进入下一阶段。

## 2. 总体原则

1. **先通信，后伺服**：总线、WKC、DC、PDO 稳定前，不做伺服使能。
2. **先单轴，后多轴**：单轴 CiA402 流程可靠后，再扩展到轴组和全身。
3. **先空载/低风险，后整机运动**：台架和限幅验证完成前，不允许整机大动作。
4. **先安全，后性能**：命令超时、Quick Stop、Fault 快照和 E-stop 链路必须早于上层控制器接入。
5. **每阶段都有门禁**：未通过本阶段测试，不进入下一阶段。
6. **OP 不等于可运动**：OP 只代表周期通信可运行，伺服使能和机器人运动需要后续阶段。

## 3. 推荐阶段路线

```text
Phase 0  环境与网卡验证
Phase 1  从站扫描与身份/拓扑校验
Phase 2  PREOP 维护与 SDO 诊断
Phase 3  PDO / IOmap / SAFEOP / DC，进入 ReadyToRun
Phase 4  OP 周期通信与 WKC/DC 监控
Phase 5  CiA402 单轴伺服状态机
Phase 6  多轴与轴组控制
Phase 7  上层实时接口与整机运行
Phase 8  故障注入、恢复与长期稳定性
```

主站软件状态与阶段关系：

| 阶段 | 主站状态 | EtherCAT 从站状态 | 是否允许伺服运动 |
|------|----------|-------------------|------------------|
| Phase 0 | `Uninitialized` / `AdapterReady` | 不要求 | 否 |
| Phase 1 | `Scanned` | INIT / PREOP 过渡 | 否 |
| Phase 2 | `Maintenance` | PREOP | 否 |
| Phase 3 | `ReadyToRun` | SAFEOP | 否 |
| Phase 4 | `Operational` | OP | 否，除非后续伺服阶段允许 |
| Phase 5 | `Operational` | OP | 允许单轴低风险测试 |
| Phase 6 | `Operational` | OP | 允许轴组受控测试 |
| Phase 7 | `Operational` | OP | 允许整机受控运行 |
| Phase 8 | `Fault` / `EmergencyStop` / 恢复流程 | 取决于故障 | 否 |

## 4. Phase 0：环境与网卡验证

### 4.1 目标

确认运行环境具备 EtherCAT 主站基础条件。

### 4.2 进入条件

- 主站程序可编译。
- SOEM 子模块完整。
- 目标网卡可被系统识别。
- 具备原始 socket 权限，通常需要 `sudo`。

### 4.3 主站动作

- 编译程序。
- 指定网卡启动。
- 初始化 SOEM。
- 主站进入 `AdapterReady`。

### 4.4 测试命令

```bash
cmake -S . -B build
cmake --build build
sudo ./build/ecat_scan <interface>
```

例如：

```bash
sudo ./build/ecat_scan enp0s31f6
```

### 4.5 通过标准

- 编译成功。
- 日志出现 `SOEM initialized on <interface>`。
- 主站状态为 `AdapterReady`。
- 输入 `help` 能看到命令列表。

### 4.6 失败处理

| 失败 | 处理 |
|------|------|
| 网卡不存在 | 检查 `ip link` 输出 |
| 权限不足 | 使用 `sudo` 或配置 capability |
| SOEM 初始化失败 | 检查网卡是否被占用、驱动是否正常 |

### 4.7 阶段门禁

未稳定进入 `AdapterReady` 前，不允许进入扫描和配置阶段。

## 5. Phase 1：从站扫描与身份/拓扑校验

### 5.1 目标

确认 EtherCAT 总线存在预期从站，并建立从站清单。

### 5.2 进入条件

- Phase 0 通过。
- EtherCAT 从站上电。
- 总线连接完成。

### 5.3 主站动作

- 执行 `scan`。
- 读取 Vendor ID、Product Code、Revision、Serial。
- 读取配置地址、别名地址、邮箱能力、DC 能力。
- 生成 `SlaveNode` 的基础身份信息。
- 校验从站数量、物理顺序、身份匹配策略。

### 5.4 测试命令

```text
scan
```

### 5.5 预期状态

- 主站：`Scanned`
- 从站：通常仍处于 INIT 或扫描后的初始状态
- 伺服：不使能

### 5.6 通过标准

- 从站数量与预期一致，或符合可选从站策略。
- 每个从站身份信息完整。
- 关键从站不得缺失。
- 安全关键从站不得缺失。
- 拓扑顺序符合配置。

### 5.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| 无从站 | 回到 `Uninitialized` 或进入 `Fault`，禁止继续 |
| 关键从站缺失 | 进入 `Fault` |
| 可选从站缺失 | 记录 Warning，按配置决定是否继续 |
| 身份不匹配 | 进入 `Fault` |
| 拓扑顺序错误 | 进入 `Fault` |

### 5.8 阶段门禁

未通过身份和拓扑校验前，不允许进入 PREOP 维护阶段。

## 6. Phase 2：PREOP 维护与 SDO 诊断

### 6.1 目标

建立邮箱通信，使主站可以通过 SDO 执行诊断、参数读取和配置准备。

### 6.2 进入条件

- Phase 1 通过。
- 从站身份和拓扑确认。

### 6.3 主站动作

- 执行 `config`。
- 请求必要从站进入 PREOP。
- 建立邮箱通信。
- 创建或补全 `SlaveNode`。
- 执行 SDO 诊断。
- 读取对象字典中的身份、版本、错误码、PDO 映射。

### 6.4 测试命令

```text
config
diagnose
inspect
pdo
```

必要时：

```text
param <index> <subindex> <value>
```

### 6.5 预期状态

- 主站：`Maintenance`
- 从站：PREOP
- 邮箱：可用
- 伺服：不使能

### 6.6 通过标准

- 所有 required 从站进入 PREOP。
- SDO 读 `0x1000`、`0x1018` 成功。
- 可读取或显示 PDO 映射。
- 可读取 AL status code。
- 从站掉出 PREOP 时能触发 `Fault`。

### 6.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| PREOP 失败 | 进入 `Fault` 或 Stop 后重扫 |
| 邮箱不可用 | 进入 `Fault` |
| SDO 读取失败 | Activity 失败，按策略保持状态或进入 `Fault` |
| 参数写入失败 | 保留失败对象、索引、子索引和值 |

### 6.8 阶段门禁

未能稳定执行 SDO 诊断和 PDO 映射读取前，不允许配置 PDO/SAFEOP/DC。

## 7. Phase 3：PDO / IOmap / SAFEOP / DC，进入 ReadyToRun

### 7.1 目标

完成进入 OP 前的所有通信准备，使主站进入 `ReadyToRun`。

### 7.2 进入条件

- Phase 2 通过。
- required 从站处于 PREOP。
- Profile 与配置已确认。

### 7.3 主站动作

- 执行 `prepare`。
- 按 Profile 配置或校验 PDO 映射。
- 构建 IOmap。
- 配置 Sync Manager / FMMU。
- 写安全初始输出。
- 配置 DC 或确认 DC 被显式禁用。
- 请求 SAFEOP。
- 验证 required 从站进入 SAFEOP。
- 主站进入 `ReadyToRun`。

### 7.4 测试命令

```text
prepare
```

可回退测试：

```text
back
prepare
```

### 7.5 预期状态

- 主站：`ReadyToRun`
- 从站：SAFEOP
- PDO：映射已确认
- IOmap：已构建
- DC：已配置或显式禁用
- 输出：安全初始值
- 伺服：不使能

### 7.6 通过标准

- 日志显示 PDO 配置成功。
- 日志显示 IOmap 输出/输入字节数。
- SAFEOP 达成。
- DC 配置达成或明确跳过。
- `back` 可回到 `Maintenance`。
- 失败会进入 `Fault` 并输出从站快照。

### 7.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| PDO 映射不匹配 | 进入 `Fault`，禁止进入 OP |
| IOmap 构建失败 | 进入 `Fault` |
| SAFEOP 失败 | 进入 `Fault` |
| DC 配置失败 | 按配置决定 Fault 或降级；人形机器人默认建议 Fault |
| 初始输出不安全 | 禁止进入 ReadyToRun |

### 7.8 阶段门禁

未进入 `ReadyToRun` 前，不允许执行 `start` 进入 OP。

## 8. Phase 4：OP 周期通信与 WKC/DC 监控

### 8.1 目标

确认 EtherCAT 周期通信稳定运行。此阶段只验证通信，不做伺服运动。

### 8.2 进入条件

- Phase 3 通过。
- 主站处于 `ReadyToRun`。

### 8.3 主站动作

- 执行 `start`。
- 请求 required 从站进入 OP。
- 周期执行 PDO 收发。
- 检查 WKC。
- 检查从站状态。
- 检查 DC 同步窗口。
- 输出周期统计。

### 8.4 测试命令

```text
start
```

停止：

```text
stop
```

### 8.5 预期状态

- 主站：`Operational`
- 从站：OP
- PDO：周期收发
- WKC：稳定等于 expected WKC
- DC：偏差在阈值内
- 伺服：仍不要求 Operation Enabled

### 8.6 通过标准

- OP 请求成功。
- 连续运行 1 分钟无 WKC mismatch。
- 从站不掉出 OP。
- 周期任务无明显超时。
- `stop` 可回到 `Uninitialized`。

### 8.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| OP 失败 | 进入 `Fault` |
| WKC 连续异常 | 进入 `Fault` |
| 从站掉线 | required 从站进入 `Fault`，safety critical 从站触发安全策略 |
| DC 偏差超限 | 告警或进入 `Fault` |

### 8.8 阶段门禁

未证明 OP 周期通信稳定前，不允许做 CiA402 伺服使能。

## 9. Phase 5：CiA402 单轴伺服状态机

### 9.1 目标

在 OP 周期通信稳定基础上，完成单轴 CiA402 伺服状态机验证。

### 9.2 进入条件

- Phase 4 通过。
- 单个伺服轴机械风险可控，建议空载或台架。
- 急停、STO、限位输入可观测。
- 命令限幅已启用。

### 9.3 主站动作

- 配置 Mode of Operation，例如 CSP。
- 读取 `0x6041` 状态字。
- 写 `0x6040` 控制字。
- 执行 CiA402 标准上电序列：

```text
Switch On Disabled
  -> Ready To Switch On
  -> Switched On
  -> Operation Enabled
```

- 验证 Fault Reset。
- 验证 Quick Stop。
- 读取实际位置、速度、力矩、电流、错误码。

### 9.4 测试方法

建议新增单轴测试命令或测试程序：

```text
axis status <axis>
axis mode <axis> csp
axis enable <axis>
axis hold <axis>
axis quickstop <axis>
axis faultreset <axis>
```

当前 CLI 未实现这些命令前，可通过专用测试代码或 Activity 验证。

### 9.5 预期状态

- 主站：`Operational`
- 从站：OP
- 伺服：可进入 `Operation Enabled`
- 轴组：不参与整组动作
- 目标值：低速、限幅、安全范围内

### 9.6 通过标准

- CiA402 状态解析正确。
- 控制字序列正确。
- 伺服可从 Fault Reset 回到可使能状态。
- Quick Stop 生效。
- 目标位置小幅变化时反馈方向正确。
- 软限位和目标突变检查生效。

### 9.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| CiA402 Fault | 停止该轴，记录错误码 |
| Quick Stop 失败 | 升级为 `Fault` 或 `EmergencyStop` |
| 跟随误差超限 | 进入 `EmergencyStop` 或轴组安全停机 |
| 方向错误 | 禁止继续多轴测试，修正配置 |

### 9.8 阶段门禁

未完成单轴使能、Quick Stop、Fault Reset、方向、限位验证前，不允许多轴联动。

## 10. Phase 6：多轴与轴组控制

### 10.1 目标

将单轴能力扩展为人形机器人轴组能力，支持腿、臂、腰、头、手等分组控制。

### 10.2 进入条件

- Phase 5 至少在代表性轴上通过。
- 轴组配置完整。
- 抱闸、限位、安全输入可观测。
- 机械结构处于低风险测试姿态。

### 10.3 主站动作

- 建立轴组模型。
- 按轴组执行上电序列。
- 检查抱闸释放条件。
- 按轴组使能伺服。
- 支持轴组 Quick Stop。
- 支持轴组 Fault Reset。
- 支持轴组诊断快照。

### 10.4 推荐轴组状态

```text
Disabled
  -> PowerReady
  -> BrakeReleased
  -> ServoEnabled
  -> Active
  -> Stopping
  -> Fault
```

### 10.5 测试方法

建议新增命令：

```text
group status <group>
group enable <group>
group hold <group>
group quickstop <group>
group disable <group>
group faultreset <group>
```

### 10.6 通过标准

- 单个轴组可独立使能和停止。
- 某个非关键轴组失败不误报全身成功。
- 关键轴组故障能触发正确安全策略。
- 抱闸释放前条件检查有效。
- 多轴反馈方向和单位一致。

### 10.7 失败处理

| 失败 | 主站响应 |
|------|----------|
| 轴组使能失败 | 轴组进入 Fault |
| 抱闸状态异常 | 禁止使能或立即停机 |
| 关键轴组故障 | 全身 Quick Stop 或 Fault |
| 安全关键输入异常 | EmergencyStop |

### 10.8 阶段门禁

未完成轴组级使能、停机、抱闸和故障恢复验证前，不允许接入整机上层控制。

## 11. Phase 7：上层实时接口与整机运行

### 11.1 目标

接入运动控制器，实现全身周期目标下发和反馈回传。

### 11.2 进入条件

- Phase 6 通过。
- 上层控制器具备低风险姿态保持能力。
- 命令限幅和命令超时保护已实现。
- 故障快照和数据记录已实现。

### 11.3 主站动作

- 接收上层周期命令。
- 校验 sequence / timestamp。
- 执行目标限幅、突变检测、软限位检查。
- 将命令写入 PDO 输出。
- 发布反馈帧。
- 监控命令超时。
- 记录关键 PDO、命令、反馈和状态。

### 11.4 测试方法

建议分级：

```text
1. 上层只发送 hold 命令
2. 单轴小幅正弦或阶跃
3. 单轴组低速动作
4. 多轴组低速动作
5. 全身静态姿态保持
6. 全身低速动作
```

### 11.5 通过标准

- 命令 sequence 单调。
- 反馈 command_sequence 与实际采用命令一致。
- 命令超时能触发保持、减速或停机。
- 异常目标值不会写入 PDO。
- 上层退出时主站能安全降级。
- 数据记录可复现故障前后窗口。

### 11.6 失败处理

| 失败 | 主站响应 |
|------|----------|
| 命令超时 | Controlled Decel 或 Quick Stop |
| sequence 乱序 | 拒绝该帧并计数 |
| 目标越限 | 拦截命令，必要时 Fault |
| 上层进程退出 | 安全降级 |

### 11.7 阶段门禁

未验证命令超时、异常命令拦截和数据记录前，不允许长时间整机运行。

## 12. Phase 8：故障注入、恢复与长期稳定性

### 12.1 目标

验证系统面对真实现场故障时是否可诊断、可停机、可恢复。

### 12.2 进入条件

- Phase 7 通过。
- 已具备故障快照。
- 已具备安全停机策略。

### 12.3 必测故障

| 故障 | 期望响应 |
|------|----------|
| 从站掉线 | `Fault` 或 `EmergencyStop`，记录从站快照 |
| WKC 连续异常 | `Fault` |
| DC 偏差超限 | 告警或 `Fault` |
| SDO 超时 | Activity 失败，按策略处理 |
| 伺服 Fault | 轴或轴组 Fault |
| 跟随误差超限 | `EmergencyStop` 或轴组安全停机 |
| 外部 E-stop | `EmergencyStop`，禁止自动恢复 |
| 上层命令超时 | Controlled Decel / Quick Stop / Fault |
| 配置与实际从站不匹配 | 阻止启动 |

### 12.4 长稳测试

建议分级：

| 测试 | 目标 |
|------|------|
| 10 分钟 | 开发阶段快速验证 |
| 1 小时 | 单轴/轴组稳定性 |
| 8 小时 | 日常回归 |
| 24 小时 | 版本候选 |
| 72 小时 | 量产或重要发布前 |

### 12.5 通过标准

- 无未解释的 WKC mismatch。
- 无从站异常掉状态。
- 周期 jitter 在目标范围。
- 日志无持续错误。
- 故障注入结果符合预期。
- 每次 Fault / EmergencyStop 都有完整快照。

### 12.6 阶段门禁

未完成故障注入和长稳测试前，不应认为主站具备人形机器人整机运行能力。

## 13. 推荐开发顺序

| 顺序 | 开发内容 | 对应阶段 |
|------|----------|----------|
| 1 | `AdapterReady`、`scan`、身份/拓扑校验 | Phase 0~1 |
| 2 | `config`、PREOP、SDO 诊断、PDO 映射读取 | Phase 2 |
| 3 | `prepare`、PDO 配置、IOmap、SAFEOP、DC | Phase 3 |
| 4 | `start`、OP 周期、WKC/DC 监控 | Phase 4 |
| 5 | 单轴 CiA402 Profile | Phase 5 |
| 6 | 轴组、抱闸、Quick Stop、Fault Reset | Phase 6 |
| 7 | 实时接口、命令保护、反馈发布 | Phase 7 |
| 8 | 故障注入、恢复策略、长稳测试 | Phase 8 |

## 14. 不建议跳过的门禁

1. 不通过 `scan` 身份/拓扑校验，不进入 `config`。
2. 不通过 SDO 诊断，不执行 `prepare`。
3. 不通过 `prepare`，不执行 `start`。
4. 不通过 OP 周期稳定性测试，不做 CiA402 使能。
5. 不通过单轴 CiA402，不做多轴。
6. 不通过轴组 Quick Stop 和抱闸测试，不接上层控制器。
7. 没有命令超时保护，不做整机运行。
8. 没有故障快照和恢复策略，不做长稳测试。

## 15. 当前代码与阶段关系

当前代码已覆盖或正在覆盖：

| 能力 | 阶段 | 状态 |
|------|------|------|
| 网卡初始化 | Phase 0 | 已有 |
| 从站扫描 | Phase 1 | 已有 |
| PREOP 维护 | Phase 2 | 已有 |
| SDO 诊断 | Phase 2 | 已有基础能力 |
| PDO 映射查看 | Phase 2 | 已有基础能力 |
| `prepare` | Phase 3 | 已拆分入口，具体从站配置仍需完善 |
| `start` | Phase 4 | 已拆分入口，当前从站 OP 失败属于已知从站/配置问题 |
| CiA402 单轴 | Phase 5 | 待实现 |
| 轴组控制 | Phase 6 | 待实现 |
| 实时上层接口 | Phase 7 | 待实现 |
| 故障注入和长稳 | Phase 8 | 待实现 |

## 16. 结语

人形机器人 EtherCAT 主站的正确流程不是“能 scan、能进 OP”这么简单，而是一条逐阶段收敛的工程链路。每一阶段都应有明确状态、测试命令、通过标准和失败策略。只有这样，后续从通信主站扩展到单轴伺服、多轴轴组和整机实时控制时，才不会反复推翻前面的设计。
