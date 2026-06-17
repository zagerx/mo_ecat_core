# 协议栈选择
- ssc工具生成的源码
- SOES

## 差异点

| 对比项 | SSC（Slave Stack Code） | SOES（Simple Open Source EtherCAT Slave Stack） |
|--------|------------------------|-----------------------------------------------|
| **官方来源** | EtherCAT Technology Group（ETG）官方提供 | OpenEtherCATSociety 开源组织维护，由 rt-labs 发起并贡献 |
| **获取方式** | 需 ETG 会员登录官网下载 | GitHub 公开仓库 [OpenEtherCATsociety/SOES](https://github.com/OpenEtherCATsociety/SOES) 免费获取 |
| **最新版本** | SSC V5.13（Tool 1.5.3） | 持续更新，以 GitHub 仓库主分支为准 |
| **授权协议** | ETG 会员许可协议（通常含使用范围限制） | 开源许可（BSD/MIT 风格，具体以仓库 LICENSE 文件为准） |
| **代码体积** | 功能完整，代码量较大 | 设计目标为小 footprint，代码精简 |
| **目标用户** | 商业产品开发、需 ETG 一致性认证的企业 | 学习研究、嵌入式轻量级应用、原型验证 |

### 功能支持对比

| 功能 | SSC | SOES |
|------|-----|------|
| **CoE（CANopen over EtherCAT）** | ✅ 完整支持 | ✅ 支持 |
| **对象字典（Object Dictionary）** | ✅ 完整支持 | ✅ 支持 |
| **SDO 读写** | ✅ 完整支持（含分段传输） | ✅ 支持（含分段传输） |
| **PDO 映射** | ✅ 固定 / 动态 PDO 映射 | ✅ 固定和/或动态 PDO 映射 |
| **FoE（File Access over EtherCAT）** | ✅ 完整支持 | ✅ 支持（含 bootstrap 模板） |
| **EoE（Ethernet over EtherCAT）** | ✅ 完整支持 | ✅ 支持（基础功能可用，EoE 示例应用待完善） |
| **SoE（SERCOS over EtherCAT）** | ✅ 完整支持 | ❌ 不支持 |
| **AoE（ADS over EtherCAT）** | ✅ 完整支持 | ❌ 不支持 |
| **SM 同步（Sync Manager）** | ✅ 完整支持 | ✅ 支持 |
| **DC 同步（Distributed Clocks）** | ✅ 完整支持（含 DC Sync0、DC Synchronization） | ✅ 支持（DC Sync0、DC Synchronization） |
| **大小端（Endianness）** | ✅ 支持 | ✅ 支持 Little / Big endian |
| **ESI 配置工具** | ✅ ETG 官方 SSC Tool 配套生成 | ❌ 无官方配套工具，需手动配置或第三方工具 |
| **HAL/硬件抽象层** | 与 Beckhoff ESC 芯片紧密绑定 | 基于地址偏移的通用 HAL，便于移植到任意 ESC 接口 |

### 架构与移植性

| 对比项 | SSC | SOES |
|--------|-----|------|
| **编程语言** | C | C |
| **ESC 绑定程度** | 与 Beckhoff ET1100/ET1200 等官方 ESC 深度适配 | 通用 HAL 设计，理论上可对接任意 ESC（含 FPGA IP Core） |
| **运行模式** | 中断驱动为主 | 支持轮询（Polling）、混合轮询/中断、纯中断三种模式 |
| **MCU 依赖** | 主要面向 Beckhoff 参考平台 | 易于移植到各类嵌入式 MCU |
| **示例应用** | 丰富，含多种配置文件和评估板工程 | 相对精简，部分示例（如 EoE 示例）待完善 |

### 文档与社区支持

| 对比项 | SSC | SOES |
|--------|-----|------|
| **官方文档** | ETG 提供完整数据手册、应用笔记、SSC Tool 使用指南 | README + 源码注释为主，独立文档待完善（TODO 项） |
| **技术支持** | ETG 官方技术支持通道（需会员资格） | GitHub Issues + rt-labs 社区支持 |
| **行业认证** | 通过 ETG CTT（Conformance Test Tool）验证的最权威参考实现 | 可用于产品开发，但商业认证需自行验证 |

### 选型建议

- **选择 SSC**：若产品需通过 ETG 官方一致性认证、需完整协议支持（SoE/AoE）、或使用 Beckhoff 官方 ESC 芯片进行商业化量产开发。
- **选择 SOES**：若项目为学习研究、原型验证、或对代码体积敏感的小型嵌入式应用，且核心需求仅需 CoE/SDO/PDO/FoE/EoE/DC 同步。

> **来源**: 
> - EtherCAT Technology Group (ETG) 官网下载区：<https://www.ethercat.org/en/downloads.html>（SSC V5.13 Release）
> - OpenEtherCATSociety/SOES GitHub 仓库：<https://github.com/OpenEtherCATsociety/SOES>（README.md 功能列表）
> - rt-labs 官网：<https://rt-labs.com>（EtherCAT 开源栈介绍）
