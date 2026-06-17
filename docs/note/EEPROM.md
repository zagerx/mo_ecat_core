# EtherCAT 从站 EEPROM (SII) 数据结构

> **来源说明**：本文档全部内容来源于 **ETG.1000.6 S (R) V1.0.4** — *EtherCAT Specification – Part 6: Application Layer protocol specification* (2017-09-15)，以下简称 "ETG.1000.6"。
>
> 辅助参考：Beckhoff Application Note **ET9300**、Renesas Application Note **R20AN0807EJ0100**、**SOEM v2.0.0** 源码（用于验证实现一致性）。

---

## 一、术语与概述

**SII (Slave Information Interface)**：EtherCAT 从站信息接口，通常以 EEPROM 形式实现，通过 I²C 与 ESC 点对点连接。

> *"The EtherCAT Slave Controller loads its configuration data (e.g. PDI configuration) during start up from the Slave Information Interface which usually is implemented as EEPROM."*
>
> — ETG.1000.6, Clause 5.4

**地址说明**：本文档中 EEPROM 地址若无特别说明，均指 **Word Address**（16 位为单位），即 Word 0 对应 Byte 0~1，Word 1 对应 Byte 2~3，以此类推。

> *"Address means a word address (e.g. 0 is first word, 1 is second word)."*
>
> — ETG.1000.6, Table 16 表头注释

**字节序**：EEPROM 数据格式为 **Little Endian**。

> *"Data format of EEPROM is little endian (e.g. ConfigData byte order: 0x0140, 0x0141, 0x0150, 0x0151, etc.)"*
>
> — ETG.1000.6, Clause 5.4 / Table 64 注释

---

## 二、SII 总体结构

SII 数据在 EEPROM 中分为两大区域：

| 区域 | Word 地址范围 | Byte 地址范围 | 说明 |
|---|---|---|---|
| **固定头部 (Configuration Area)** | 0x0000 ~ 0x003F | 0x00 ~ 0x7F | 64 Word（128 Byte），包含 ESC 启动必需的配置参数 |
| **Category Sections** | 0x0040 起 | 0x80 起 | 按 Category 组织的可选扩展信息 |

> — ETG.1000.6, Table 16 & Table 17

---

## 三、固定头部（Configuration Area）

> 数据来源：ETG.1000.6, **Table 16 – Slave Information Interface Area**

### 3.1 PDI 与系统配置

| Word Addr | Byte Offset | 字段名 | 数据类型 | 说明 |
|---|---|---|---|---|
| 0x0000 | 0x00~0x01 | **PDI Control** | Unsigned16 | ESC PDI Control 寄存器 (0x140~0x141) 的初始化值 |
| 0x0001 | 0x02~0x03 | **PDI Configuration** | Unsigned16 | ESC PDI Configuration 寄存器 (0x150~0x151) 的初始化值 |
| 0x0002 | 0x04~0x05 | **SyncImpulseLen** | Unsigned16 | Sync 脉冲长度，单位为 **10 ns** 的倍数 |
| 0x0003 | 0x06~0x07 | **PDI Configuration2** | Unsigned16 | R8 系列 ESC 的 PDI Config 高字 (0x152~0x153) |
| 0x0004 | 0x08~0x09 | **Configured Station Alias** | Unsigned16 | 配置站点别名，映射到 ESC 寄存器 0x0012 |
| 0x0005 | 0x0A~0x0D | **Reserved** | BYTE[4] | 保留 |
| 0x0007 | 0x0E~0x0F | **Checksum** | Unsigned16 | **低字节**为 Word 0~6 的 CRC 余数，多项式 **x⁸+x²+x+1**，初值 **0xFF** |

### 3.2 从站标识

| Word Addr | Byte Offset | 字段名 | 数据类型 | 说明 |
|---|---|---|---|---|
| 0x0008 | 0x10~0x13 | **Vendor ID** | Unsigned32 | 厂商标识符，对应 CoE Object 0x1018 Subindex 1 |
| 0x000A | 0x14~0x17 | **Product Code** | Unsigned32 | 产品代码，对应 CoE Object 0x1018 Subindex 2 |
| 0x000C | 0x18~0x1B | **Revision Number** | Unsigned32 | 版本号，对应 CoE Object 0x1018 Subindex 3 |
| 0x000E | 0x1C~0x1F | **Serial Number** | Unsigned32 | 序列号，对应 CoE Object 0x1018 Subindex 4 |
| 0x0010 | 0x20~0x27 | **Reserved** | BYTE[8] | 保留 |

### 3.3 Bootstrap Mailbox 配置

| Word Addr | Byte Offset | 字段名 | 数据类型 | 说明 |
|---|---|---|---|---|
| 0x0014 | 0x28~0x29 | **Bootstrap Rx Mailbox Offset** | Unsigned16 | Bootstrap 状态接收邮箱偏移（主站 → 从站） |
| 0x0015 | 0x2A~0x2B | **Bootstrap Rx Mailbox Size** | Unsigned16 | Bootstrap 状态接收邮箱大小。可与标准邮箱大小不同 |
| 0x0016 | 0x2C~0x2D | **Bootstrap Tx Mailbox Offset** | Unsigned16 | Bootstrap 状态发送邮箱偏移（从站 → 主站） |
| 0x0017 | 0x2E~0x2F | **Bootstrap Tx Mailbox Size** | Unsigned16 | Bootstrap 状态发送邮箱大小 |

### 3.4 标准 Mailbox 配置

| Word Addr | Byte Offset | 字段名 | 数据类型 | 说明 |
|---|---|---|---|---|
| 0x0018 | 0x30~0x31 | **Standard Rx Mailbox Offset** | Unsigned16 | 标准状态接收邮箱偏移（主站 → 从站） |
| 0x0019 | 0x32~0x33 | **Standard Rx Mailbox Size** | Unsigned16 | 标准状态接收邮箱大小 |
| 0x001A | 0x34~0x35 | **Standard Tx Mailbox Offset** | Unsigned16 | 标准状态发送邮箱偏移（从站 → 主站） |
| 0x001B | 0x36~0x37 | **Standard Tx Mailbox Size** | Unsigned16 | 标准状态发送邮箱大小 |
| 0x001C | 0x38~0x39 | **Mailbox Protocol** | Unsigned16 | 支持的邮箱协议，定义见 **Table 18** |
| 0x001D | 0x3A~0x7B | **Reserved** | BYTE[66] | 保留 |

### 3.5 EEPROM 元信息

| Word Addr | Byte Offset | 字段名 | 数据类型 | 说明 |
|---|---|---|---|---|
| 0x003E | 0x7C~0x7D | **Size** | Unsigned16 | EEPROM 大小 = **(Value + 1) KiBit**。`Value = 0` 表示 1 KiBit (128 Byte) |
| 0x003F | 0x7E~0x7F | **Version** | Unsigned16 | SII 格式版本，当前为 **1** |

> — 以上全部引自 ETG.1000.6, **Table 16 – Slave Information Interface Area**

### 3.6 Mailbox Protocol 支持标志（Table 18）

> 数据来源：ETG.1000.6, **Table 18 – Mailbox Protocols Supported Types**

| 协议 | 值 | 说明 |
|---|---|---|
| AoE | 0x0001 | ADS over EtherCAT |
| EoE | 0x0002 | Ethernet over EtherCAT |
| CoE | 0x0004 | CANopen over EtherCAT |
| FoE | 0x0008 | File Access over EtherCAT |
| SoE | 0x0010 | Servo Drive Profile over EtherCAT |
| VoE | 0x0020 | Vendor specific protocol over EtherCAT |

---

## 四、Category Sections（类别区域）

> 数据来源：ETG.1000.6, **Table 17 – Slave Information Interface Categories** & **Table 19 – Categories Types**

从 **Word Address 0x0040**（Byte 0x80）开始，SII 数据按 **Category** 组织。

### 4.1 Category 头部格式

每个 Category 以固定 3-Word 头部开头：

| 偏移（相对 Category 起始） | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| Word 0 | **[15:1] Category Type** | Unsigned15 | 类别码，定义见 **Table 19** |
| Word 0 | **[0] Vendor Specific** | Unsigned1 | 厂商特定标志 |
| Word 1 | **Following Category Word Size** | Unsigned16 | 本 Category 数据区长度（Word 为单位） |
| Word 2 ~ Word (1+x) | **Category Data** | Category dependent | 实际类别数据 |

> — ETG.1000.6, **Table 17 – Slave Information Interface Categories**

### 4.2 Category 类型定义（Table 19）

> 数据来源：ETG.1000.6, **Table 19 – Categories Types**

| CatNo | 名称 | 说明 |
|---|---|---|
| 00 | **NOP** | 无信息 |
| 01 ~ 09 | **Device specific** | 设备特定类别。**不得被主站或配置工具覆盖**。可用于校准值等 |
| 10 | **STRINGS** | 字符串仓库，供其他 Category 引用。结构见 **Table 20** |
| 20 | **DataTypes** | 保留给未来使用 |
| 30 | **GENERAL** | 一般信息（CoE/FoE/EoE 详情、E-bus 电流等）。结构见 **Table 21** |
| 40 | **FMMU** | FMMU 配置。结构见 **Table 23** |
| 41 | **SyncM** | SyncManager 配置。结构见 **Table 24** |
| 50 | **TXPDO** | TxPDO（输入过程数据）描述。结构见 **Table 25** |
| 51 | **RXPDO** | RxPDO（输出过程数据）描述。结构见 **Table 25** |
| 60 | **DC** | Distributed Clock 同步参数。结构见 **Table 27 / Table 28** |
| 0x0800 ~ 0x0FFF | **Vendor specific** | 厂商特定类别 |
| 0xFFFF | **End** | 类别列表结束标记 |

---

## 五、各类别详细结构

### 5.1 STRINGS Category（CatNo = 10）

> 数据来源：ETG.1000.6, **Table 20 – Structure Category String**

| Byte Addr | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x0000 | **nStrings** | Unsigned8 | 字符串数量 |
| 0x0001 | **str1_len** | Unsigned8 | 字符串 1 长度 |
| 0x0002 | **str_1** | BYTE[str1_len] | 字符串 1 数据（VISIBLESTRING） |
| ... | ... | ... | 后续字符串按同样格式排列 |
| 末尾 | **PAD_Byte** | BYTE | 若 Category 长度为奇数，填充 1 字节 |

> **注 1**：字符串索引从 **1** 开始，索引 **0** 表示空字符串。
> **注 2**：字符串类型默认为 VISIBLESTRING。
>
> — ETG.1000.6, **Table 20 – Structure Category String** 注释

### 5.2 GENERAL Category（CatNo = 30）

> 数据来源：ETG.1000.6, **Table 21 – Structure Category General**

| Byte Addr | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x00 | **GroupIdx** | Unsigned8 | 组信息索引 → 指向 STRINGS Category |
| 0x01 | **ImgIdx** | Unsigned8 | 图像名称索引 → 指向 STRINGS Category |
| 0x02 | **OrderIdx** | Unsigned8 | 订单号索引 → 指向 STRINGS Category |
| 0x03 | **NameIdx** | Unsigned8 | 设备名称索引 → 指向 STRINGS Category |
| 0x04 | **Reserved** | Unsigned8 | 保留 |
| 0x05 | **CoE Details** | Unsigned8 | Bit 0: Enable SDO<br>Bit 1: Enable SDO Info<br>Bit 2: Enable PDO Assign<br>Bit 3: Enable PDO Configuration<br>Bit 4: Enable Upload at startup<br>Bit 5: Enable SDO complete access |
| 0x06 | **FoE Details** | Unsigned8 | Bit 0: Enable FoE |
| 0x07 | **EoE Details** | Unsigned8 | Bit 0: Enable EoE |
| 0x08 | **SoEChannels** | Unsigned8 | 保留 |
| 0x09 | **DS402Channels** | Unsigned8 | 保留 |
| 0x0A | **SysmanClass** | Unsigned8 | 保留 |
| 0x0B | **Flags** | Unsigned8 | Bit 0: Enable SafeOp<br>Bit 1: Enable notLRW<br>Bit 2: MboxDataLinkLayer<br>Bit 3~4: Identification Method（见 **Table 22**） |
| 0x0C | **CurrentOnEBus** | Signed16 | E-bus 电流消耗（**mA**）。负值表示向总线馈电 |
| 0x0E | **GroupIdx** | Unsigned8 | 字符串索引（兼容重复） |
| 0x0F | **Reserved** | BYTE[1] | 保留 |
| 0x10 | **Physical Port** | Unsigned16 | 物理端口描述（每 4 bits 一个端口）：<br>3:0 → Port 0, 7:4 → Port 1, 11:8 → Port 2, 15:12 → Port 3<br>0x00=未使用, 0x01=MII, 0x03=EBUS, 0x04=Fast Hot Connect |
| 0x12 | **Physical Memory Address** | Unsigned16 | 当 Identification Method = IdentPhyM 时，ID 保存的 ESC 内存地址 |
| 0x14 | **Reserved2** | BYTE[12] | 保留 |

### 5.3 Identification Methods（Table 22）

> 数据来源：ETG.1000.6, **Table 22 – Identification Methods**

| 值 | 名称 | 说明 |
|---|---|---|
| Bit 3 | **IdentALSts** | ID 选择器映射到 AL Status Code |
| Bit 4 | **IdentPhyM** | ID 选择器值映射到由 "Physical Memory Address" 指定的物理内存 |

### 5.4 FMMU Category（CatNo = 40）

> 数据来源：ETG.1000.6, **Table 23 – Structure Category FMMU**

| Byte Addr | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x00 | **FMMU0** | Unsigned8 | 0x00=未使用, 0x01=Outputs, 0x02=Inputs, 0x03=SyncM Status(Read Mailbox), 0xFF=未使用 |
| 0x01 | **FMMU1** | Unsigned8 | 同上 |
| ... | ... | ... | 若使用更多 FMMU，继续排列 |

### 5.5 SyncM Category（CatNo = 41）

> 数据来源：ETG.1000.6, **Table 24 – Structure Category SyncM for each Element**

每个 SyncManager 条目占 **8 字节**：

| Byte Addr | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x00 | **Physical Start Address** | WORD | SM 物理起始地址 |
| 0x02 | **Length** | WORD | SM 长度 |
| 0x04 | **Control Register** | Unsigned8 | 操作模式控制字（参考 SyncM Control Register） |
| 0x05 | **Status Register** | BYTE | don't care |
| 0x06 | **Enable SyncManager** | Unsigned8 | Bit 0: enable<br>Bit 1: fixed content（SyncMan 有固定内容）<br>Bit 2: virtual SyncManager（不占用硬件资源）<br>Bit 3: opOnly（仅在 OP 状态使能） |
| 0x07 | **Sync Manager Type** | BYTE | 0x00=未使用/未知<br>0x01=Mailbox Out<br>0x02=Mailbox In<br>0x03=Process Data Outputs<br>0x04=Process Data Inputs |

> **关键说明**：Byte 0x06 和 0x07 **不能**直接作为对应 ESC 寄存器的内容使用。
>
> — ETG.1000.6, **Table 24 – Structure Category SyncM** 注释

### 5.6 TXPDO / RXPDO Category（CatNo = 50 / 51）

> 数据来源：ETG.1000.6, **Table 25 – Structure Category TXPDO and RXPDO for each PDO**

每个 PDO 描述结构：

| Byte Addr | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x0000 | **PDO Index** | Unsigned16 | RxPDO: 0x1600~0x17FF；TxPDO: 0x1A00~0x1BFF |
| 0x0002 | **nEntry** | Unsigned8 | Entry 数量 |
| 0x0003 | **SyncM** | Unsigned8 | 关联的 SyncManager 编号 |
| 0x0004 | **Synchronization** | Unsigned8 | DC 同步引用 |
| 0x0005 | **NameIdx** | Unsigned8 | 对象名称索引 → STRINGS |
| 0x0006 | **Flags** | WORD | 保留给未来使用 |
| 0x0008 | **Entry 1** | 8 BYTES | 按 **Table 26** 格式重复 |
| ... | ... | ... | 重复 nEntry 次 |

### 5.7 PDO Entry 结构（Table 26）

> 数据来源：ETG.1000.6, **Table 26 – Structure PDO Entry**

每个 Entry 占 **8 字节**：

| 偏移 | 字段名 | 数据类型 | 说明 |
|---|---|---|---|
| 0x0000 | **Entry Index** | Unsigned16 | 条目 Object Index |
| 0x0002 | **Subindex** | Unsigned8 | 条目 Subindex |
| 0x0003 | **Entry Name Idx** | Unsigned8 | 条目名称索引 → STRINGS |
| 0x0004 | **Data Type** | Unsigned8 | 数据类型（CoE Object Dictionary 中的索引） |
| 0x0005 | **BitLen** | Unsigned8 | 数据长度（bit） |
| 0x0006 | **Flags** | WORD | 保留给未来使用 |

---

## 六、最小 EEPROM 内容

> 以下总结综合了 ETG.1000.6 Table 16、Table 17、Table 19 及 ET9300 相关描述。

### 6.1 绝对最小内容（仅能被主站识别）

若只需主站能扫描到从站并读取基本信息，EEPROM 至少需要包含以下字段：

| 字段 | 地址 | 说明 |
|---|---|---|
| **PDI Control** | Word 0x0000 | ESC 必须知道 PDI 类型才能与 MCU 通信 |
| **Vendor ID** | Word 0x0008 | 厂商标识 |
| **Product Code** | Word 0x000A | 产品代码 |
| **Revision Number** | Word 0x000C | 版本号 |
| **Checksum** | Word 0x0007 | 前 14 字节 CRC。若校验失败，ESC 标记 EEPROM CRC Error (0x0502.11) |

### 6.2 支持 Mailbox 通信的最小内容

若需支持 CoE/SDO 等邮箱通信（进入 PREOP 状态的基础），还需要：

| 字段 | 地址 | 说明 |
|---|---|---|
| **Mailbox Protocol** | Word 0x001C | 声明支持的邮箱协议（至少 CoE = 0x0004） |
| **Standard Rx/Tx Mailbox Offset/Size** | Word 0x0018~0x001B | 邮箱缓冲区在 ESC DPRAM 中的地址和大小 |
| **Bootstrap Rx/Tx Mailbox Offset/Size** | Word 0x0014~0x0017 | Bootstrap 模式邮箱配置（如需支持固件更新） |

### 6.3 支持 Process Data 通信的最小内容

若需支持 PDO 通信（进入 SAFEOP 和 OP 状态），**Category Sections** 中必须包含：

| Category | CatNo | 说明 |
|---|---|---|
| **SyncM** | 41 | 至少定义 SM2 (PD Outputs) 和 SM3 (PD Inputs) 的地址、长度、控制字、类型 |
| **FMMU** | 40 | 定义 FMMU 方向（Outputs/Inputs） |
| **TXPDO / RXPDO** | 50 / 51 | 定义过程数据映射关系（或依赖对象字典 0x1C12/0x1C13 动态配置） |
| **STRINGS** | 10 | 若 PDO/General 中引用了名称索引，需提供对应的字符串数据 |
| **GENERAL** | 30 | 声明 CoE Details（PDO Assign/Config 使能位）、Flags 等 |

> **关键说明**：主站在 PREOP → SAFEOP 转换时会对比自身通过 Init Commands 下发的 SM 配置与从站内部常量。若 EEPROM 中的 SII SM 信息与从站协议栈软件常量不匹配，从站将返回 **0x1D (Invalid Output Configuration)** 或 **0x1E (Invalid Input Configuration)**。
>
> — ETG.1000.6, Clause 5.3 / ET9300, Chapter 17

### 6.4 实际最小 EEPROM 容量

EEPROM 总容量由固定头部 Word 0x003E（Size）定义：

> *"size of E2PROM in [KiBit] + 1. NOTE: size = 0 means a EEPROM size of 1 KiBit."*
>
> — ETG.1000.6, **Table 16 – Slave Information Interface Area**, Size 字段

| Size 字段值 | 总容量 | 典型应用场景 |
|---|---|---|
| 0 | 1 KiBit (128 Byte) | 仅固定头部，无 Category Sections |
| 1 | 2 KiBit (256 Byte) | 简单从站，少量字符串 |
| 3 | 4 KiBit (512 Byte) | 标准从站，含 SM/PDO/FMMU 配置 |
| 7 | 8 KiBit (1024 Byte) | 复杂从站，多 PDO、完整字符串表 |
| 15 | 16 KiBit (2048 Byte) | 高复杂度从站（如 CiA402 伺服） |

> *Renesas RA8T2 的 EEPROM 容量可通过 Smart Configurator 设置为 "Under 32Kbits" 或 "Over 32Kbits"。*
>
> — Renesas R20AN0807EJ0100, Appendix C

---

## 七、EEPROM 与 ESI 文件的关系

> 数据来源：ETG.2000 S (R) V1.0.10, Clause 13.7 / ET9300, Chapter 14.2

| 项目 | 说明 |
|---|---|
| **ESI 文件** | EtherCAT Slave Information XML，从站设备描述文件（如 `RA8 EtherCAT.xml`） |
| **EEPROM 数据** | ESI 文件的一个子集，包含从站上电时 ESC 必须加载的静态配置 |
| **生成工具** | SSC Tool → [Tool] → [EEPROM Programmer] → 从 ESI 生成二进制/头文件 |
| **更新方式** | 1. SSC Tool 直接下载<br>2. TwinCAT → ESC Access → EEPROM → Hex Editor<br>3. SOEM `eepromtool` 通过 EtherCAT 总线写入 |

**关键原则**：
- ESI 是"源"，EEPROM 是"派生"
- 修改从站配置后，必须重新生成 ESI → 再生成 EEPROM 数据 → 再烧录
- 主站在线运行时**不读取 ESI 文件**，只读取从站 EEPROM 中的 SII 数据

---

## 八、EEPROM 主站可写性

> 数据来源：SOEM `eepromtool.c` / ETG.1000.2 (Physical Layer)

**主站可以通过 EtherCAT 总线更新从站 EEPROM**，前提条件：

1. **控制权切换**：主站需先通过 ESC 寄存器 0x0500 (EEPROM Config) 将 EEPROM 控制权从 PDI 切换到 Master
2. **硬件写保护**：EEPROM 芯片（如 M24C16）的 WP 引脚必须处于允许写入状态
3. **写入粒度**：按 word（2 字节）写入，每次写入后需等待 EEPROM 内部完成
4. **CRC 更新**：修改 Word 0x0000~0x0006 范围内的字段后，必须重新计算并更新 Word 0x0007 (Checksum)

---

## 九、引用文档

1. **ETG.1000.6 S (R) V1.0.4** — EtherCAT Specification – Part 6: Application Layer protocol specification (2017-09-15)
   - Table 16: Slave Information Interface Area
   - Table 17: Slave Information Interface Categories
   - Table 18: Mailbox Protocols Supported Types
   - Table 19: Categories Types
   - Table 20: Structure Category String
   - Table 21: Structure Category General
   - Table 22: Identification Methods
   - Table 23: Structure Category FMMU
   - Table 24: Structure Category SyncM
   - Table 25: Structure Category TXPDO and RXPDO
   - Table 26: Structure PDO Entry

2. **ETG.2000 S (R) V1.0.10** — EtherCAT Slave Information Specification (2018-02-01)
   - Clause 13.7: EepromType

3. Beckhoff Application Note **ET9300** — EtherCAT Slave Stack Code

4. Renesas Application Note **R20AN0807EJ0100** — Quick Start Guide: MCK-RA8T2 EtherCAT

5. **SOEM v2.0.0** 开源主站源码：`ec_type.h`, `eepromtool.c`, `slaveinfo.c`
