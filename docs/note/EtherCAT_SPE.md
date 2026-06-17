# ETG组织对Single Pair Ethernet的态度

## 结论

ETG对Single Pair Ethernet（SPE）的官方态度可概括为：**严谨的规范演进与开放的生态兼容**。

- **官方标准层面**：ETG.1000核心规范目前仅将 100BASE‑TX、100BASE‑FX 等列为官方物理层选项，尚未发布将 10BASE‑T1L 或 10BASE‑T1S 纳入官方物理层标准的增补文件。
- **生态兼容层面**：ETG公开声明"完整的以太网布线范围均可用于EtherCAT"，其协议架构与物理层解耦，从技术上允许在兼容的IEEE 802.3物理层上运行EtherCAT协议。

---

## 1. ETG.1000 规范：官方物理层基础

ETG.1000《EtherCAT Specification》是ETG最核心的官方规范，定义了EtherCAT协议的全部核心内容。该规范明确将以下物理层列为官方选项：

- **100BASE‑TX**（IEEE 802.3u）：标准双绞线，100 Mbit/s
- **100BASE‑FX**（IEEE 802.3u）：光纤扩展
- **EtherCAT P**：ETG官方协议补充，通过标准四线以太网电缆同时传输通信数据与电源

> **来源**：EtherCAT Technology Group (ETG) 官网技术介绍页 - https://www.ethercat.org/en/technology.html
>
> **来源**：Renesas RA8T2 User's Manual (R01UH1067EJ0130) 第36.2节："Regarding the detailed specification of EtherCAT and ESC, refer to the documentation (ETG.1000 EtherCAT Specification) provided by EtherCAT Technology Group (ETG)."

在ETG.1000现行版本中，**10BASE‑T1L**（IEEE 802.3cg）与**10BASE‑T1S**（IEEE 802.3cg）未被列入官方物理层选项。因此，从官方规范角度，SPE目前不属于ETG认证体系内的标准物理层。

## 2. ETG.1020 规范：协议增强，非物理层扩展

ETG.1020《EtherCAT Protocol Enhancements》是对ETG.1000的功能补充，主要涉及协议软件层面的更新（如 mailbox 通道优化、状态机扩展等）。该规范**不涉及物理层扩展**，因此不能作为ETG官方支持SPE物理层的依据。

> **来源**：EtherCAT Technology Group (ETG) 官网下载区对ETG.1020的文件描述 - https://www.ethercat.org/en/downloads.html

## 3. ETG 的开放兼容立场

ETG在官方技术介绍中明确声明：

> "The complete range of Ethernet wiring is also available for EtherCAT."
>
> **来源**：EtherCAT Technology Group (ETG) 官网 - https://www.ethercat.org/en/technology.html

这一声明的技术基础在于EtherCAT的协议架构：

- EtherCAT协议嵌入标准以太网帧，EtherType为 **0x88A4**
- 数据链路层由EtherCAT Slave Controller（ESC）硬件处理，与物理层解耦
- 只要物理层满足IEEE 802.3标准且能通过标准MII/RMII接口与ESC对接，EtherCAT协议即可在其上运行

> **来源**：ETG官网技术页对EtherCAT协议结构的说明；Renesas RA8T2 User's Manual (R01UH1067EJ0130) 第36.1节 Overview

## 4. 重要区分：ELX6233 的协议桥接属性

Beckhoff ELX6233 常被误认为"EtherCAT over SPE"的官方实现，但官方文档明确显示其架构与"原生 EtherCAT over SPE"存在本质区别。

### 4.1 官方定义

> "The ELX6233 EtherCAT Terminal allows direct connection of Ethernet-APL-capable field devices from the hazardous areas of zones 0/20 and 1/21. The sensors are supplied in accordance with the SPAA (TS10186) port profile and integrated via PROFINET."
>
> **来源**：Beckhoff 官方数据手册 `elx6233_en.pdf`

### 4.2 关键区分：桥接方案 vs 原生 EtherCAT over SPE

| 对比项 | **ELX6233 方案** | **原生 EtherCAT over SPE（概念）** |
|--------|-----------------|----------------------------------|
| **主站侧接口** | EtherCAT（作为 ELX 端子从站接入 TwinCAT） | EtherCAT 主站直接通过 SPE 物理层通信 |
| **设备侧协议** | PROFINET（PROFINET controller protocol） | EtherCAT（0x88A4） |
| **物理层** | Ethernet-APL（10BASE-T1L） | Ethernet-APL（10BASE-T1L）或 10BASE-T1S |
| **ESC 角色** | 该模块自身作为 EtherCAT 从站，内部集成 PROFINET 控制器 | ESC 直接通过 MII/RMII 连接 SPE PHY |
| **应用场景** | 过程工业传感器/执行器（通过 PROFINET 集成） | 伺服驱动、IO 等实时 EtherCAT 从站 |
| **协议栈层次** | EtherCAT 帧承载 PROFINET 应用层 | EtherCAT 数据链路层直接运行在 SPE 物理层上 |

> **来源**：Beckhoff 官方数据手册 `elx6233_en.pdf` 第 2.3 节 Intended use、第 4 章 PROFINET controller protocol；ETG 官网对 EtherCAT 协议架构的说明

### 4.3 为什么不能作为 EtherCAT-SPE 伺服控制的参考

基于官方文档，ELX6233 不能证明"带 SPE 接口的伺服驱动器可直接接入 EtherCAT 网络"：

- **协议不匹配**：ELX6233 设备侧运行的是 PROFINET 协议，而非 EtherCAT CoE/CiA402。伺服关节模组通常需要 EtherCAT CoE 协议进行实时循环通信。
- **拓扑限制**：ELX6233 是单点桥接模块，每个模块连接两个 APL 现场设备。它不具备 EtherCAT 从站的"on-the-fly"帧处理和端口转发能力。
- **官方定位**：Beckhoff 官方文档将该模块明确限定为过程工业现场设备（传感器、执行器）的集成，**从未提及**伺服驱动器、电机控制、CiA402 或运动控制应用。

### 4.4 结论

ELX6233 是一个**"EtherCAT 转 PROFINET over APL"的协议桥接模块**，不是"原生 EtherCAT over SPE"的实现。它证明了 Beckhoff 在过程工业中采用 Ethernet-APL 物理层，但采用的是 PROFINET 协议栈而非 EtherCAT 协议栈直接运行在 SPE 上。

因此，ELX6233 **不能作为** EtherCAT-SPE 伺服控制或关节模组应用的参考案例。
## 5.TI社区
| 组件            | 型号                        | 官方定位                                        |
| ------------- | ------------------------- | ------------------------------------------- |
| 主控评估板         | **TMDS64EVM**             | 支持 EtherCAT 从站协议的 AM64x 官方评估板               |
| SPE 子卡 (100M) | **DP83TC812-IND-SPE-EVM** | 用于评估单对以太网 PHY，通过 Samtec 连接器与 AM64x EVM 直插兼容 |
| SPE 子卡 (1G)   | **DP83TG720-IND-SPE-EVM** | 同上，基于 DP83TG720 1000BASE-T1 PHY             |

### SNLA420A
TI 白皮书 SNLA420A（《How and Why to Use Single Twisted-Pair Ethernet for Industrial Robotics》）中明确搭建了菊花链测试系统，用于验证 SPE 在工业实时协议中的确定性：
- 测试拓扑模拟了 EtherCAT、SORTE、Profinet 等典型的菊花链工业网络。
- 使用 TMDS64GPEVM + DP83TC812/DP83TG720 测量了 PHY 收发延迟、MAC 延迟、周期时间与抖动。
- 文档原文指出："needed to emulate an industrial daisy-chained system which is typically used in industrial protocols like EtherCAT..."

这说明 TI 在内部测试中验证了 SPE PHY 的延迟特性可以满足 EtherCAT 的实时性要求，但这不是一份面向用户的 "EtherCAT over SPE" 应用指南。

## 讨论贴

根据TI官方E2E论坛（德州仪器在线技术支持社区）的讨论，关于“使用AM64通信协议栈通过单对以太网（SPE）实现EtherCAT”的方案，讨论已结束并被标记为“已解决” (Resolved)。最终的讨论结果可以总结为以下关键结论和重要发现，这为开发者提供了权威的参考：

### 🔎 核心结论

| 芯片型号 | 速度等级 | 讨论结论 | 关键使用条件/特性 |
|---------|---------|---------|-----------------|
| **DP83TD510E** | 10Mbps (10BASE-T1L) | 官方确认支持 EtherCAT | 满足实时应用需求，专为长距离低速场景优化 |
| **DP83TC812** | 100Mbps (100BASE-T1) | 可行但有妥协 | 仅建议用于封闭系统，且需接受更慢的链路断开时间 |

TI工程师在回复中明确提到：

> "DP83TD510 has support and fits the needs of EtherCAT. The DP83TC812 may also fit the needs of Ethercat if you are using a closed system and are ok with a slower link down time."
>
> **来源**：TI E2E 社区官方讨论帖

### 🔧 技术细节与补充

- **MII接口兼容**：无论是 DP83TC812 (100M) 还是 DP83TD510E (10M)，都与AM64的EtherCAT从站协议栈所需的MII（媒体独立接口）兼容，这是方案可行的物理层基础。
- **DP83TC812的考量**：其最大局限在于链路断开（Link Down）反应时间无法满足EtherCAT冗余切换所需的 <15µs 苛刻要求，这也是“封闭系统”限制的根本原因。
- **关于RGMII的说明**：论坛讨论澄清了此前SNLA420A白皮书“仅测试了RGMII”的疑点。结论是，用户需要的MII接口是两者均支持的，因此白皮书的测试结论仍具参考价值。

### ⚠️ 已知限制与官方状态

| 类别 | 详细说明 |
|------|---------|
| **官方认证** | 这属于内部技术验证，目前并非官方正式认证的“EtherCAT over SPE”解决方案 |
| **链路检测限制** | DP83TC812 的具体 Link Down 时间需向TI官方进一步确认 |
| **速率一致性** | 若混用10M和100M设备，因两者无法通过自协商匹配速率，通信将完全失效 |
| **自定义协议** | 标准EtherCAT要求Auto-negotiation和MDI/MDI-X等功能，而SPE PHY对这些功能的支持较弱，可能需要自定义协议 |

### 💎 总结

在TI的官方技术讨论中，**DP83TD510E (10BASE-T1L)** 是明确获得“满足EtherCAT需求”评价的SPE PHY。而 **DP83TC812 (100BASE-T1)** 虽可能实现，但受限于其性能，仅建议在封闭且对实时性要求不高的系统中使用。如果对实时性有严苛要求，稳妥起见应优先选择DP83TD510E。

## 6. 总结

| 层面 | ETG 官方态度 |
|------|-------------|
| **规范层面** | ETG.1000 未将 SPE 纳入官方物理层标准；ETG.1020 不涉及物理层扩展 |
| **技术层面** | EtherCAT 协议栈与物理层解耦，兼容任何标准 IEEE 802.3 物理层 |
| **生态层面** | ETG 秉持开放原则，允许成员探索 SPE 集成方案 |
