# EtherCAT Slave Information (ESI) 文件规范

> **来源说明**：本文档全部内容来源于 **ETG.2000 S (R) V1.0.10** — *EtherCAT Slave Information (ESI) Specification* (2018-02-01)，以下简称 "ETG.2000"。
>
> 辅助参考：Beckhoff Application Note **ET9300**。

---

## 一、ESI 文件概述

ESI（EtherCAT Slave Information）文件是从站的 XML 格式设备描述文件，扩展名通常为 `.xml`。它描述了从站的硬件能力、通信参数、过程数据映射、对象字典等信息，供主站配置工具和 SSC EEPROM 生成器使用。

> *"The EtherCAT Slave Information (ESI) describes the EtherCAT device capabilities (e.g. object dictionary entries and process data mapping)."*
>
> — ETG.2000, Clause 4.1

**关键原则**：
- ESI 是**源文件**，EEPROM 中的 SII 数据是 ESI 的**派生子集**
- 主站在线运行时**不读取 ESI**，只读取从站 EEPROM 中的 SII 数据
- 修改从站配置后，必须重新生成 ESI → 生成 EEPROM 数据 → 烧录到从站

---

## 二、ESI 文件总体结构

> 数据来源：ETG.2000, **Clause 6 – XML structure**

ESI 文件是一个 XML 文档，根元素为 `<EtherCATInfo>`，整体层次结构如下：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<EtherCATInfo>
  <Vendor>
    <Id>Vendor ID</Id>
    <Name>Vendor Name</Name>
  </Vendor>
  <Descriptions>
    <Groups>
      <Group>
        <Type>Group Type</Type>
        <Name>Group Name</Name>
        <ImageData16x14>...</ImageData16x14>
      </Group>
    </Groups>
    <Devices>
      <Device>
        <Type ProductCode="..." RevisionNo="...">Device Type</Type>
        <Name>...</Name>
        <Sm ... />          <!-- SyncManager 定义 -->
        <Fmmu ... />        <!-- FMMU 定义 -->
        <Mailbox>           <!-- 邮箱配置 -->
          <CoE ... />
          <FoE ... />
        </Mailbox>
        <TxPdo ...>         <!-- 输入过程数据 -->
          <Entry ... />
        </TxPdo>
        <RxPdo ...>         <!-- 输出过程数据 -->
          <Entry ... />
        </RxPdo>
        <Dc ... />          <!-- DC 分布式时钟 -->
        <Eeprom>            <!-- EEPROM 配置 -->
          <ByteSize>...</ByteSize>
          <ConfigData>...</ConfigData>
          <BootStrap>...</BootStrap>
        </Eeprom>
      </Device>
    </Devices>
  </Descriptions>
</EtherCATInfo>
```

---

## 三、必须包含的信息

以下按元素层级逐一说明 ESI 中**必须或关键**的字段。若缺失这些字段，主站将无法正确配置从站或从站无法进入 SAFEOP/OP 状态。

### 3.1 根元素与命名空间

| 元素 | 属性 | 说明 |
|---|---|---|
| `<EtherCATInfo>` | `xmlns:xx` | XML 命名空间声明。ETG.2000 定义了多个 Schema 版本 |

> — ETG.2000, Clause 6.1

### 3.2 Vendor（厂商信息）

| 元素 | 数据类型 | 说明 |
|---|---|---|
| `<Vendor>` | Container | 厂商信息容器 |
| `├─ <Id>` | Unsigned32 | **Vendor ID**（厂商标识符），对应 CoE 0x1018:01 |
| `└─ <Name>` | String | 厂商名称，如 "Renesas Electronics" |

> — ETG.2000, Clause 6.2 / **Table 3 – ESI Vendor Element**

### 3.3 Device（设备描述）—— 核心部分

`<Device>` 是 ESI 中**最重要的元素**，每个从站型号对应一个 `<Device>` 节点。

#### 3.3.1 设备标识

| 元素 | 属性/内容 | 说明 |
|---|---|---|
| `<Type>` | `ProductCode` (Unsigned32) | 产品代码，对应 CoE 0x1018:02 |
| | `RevisionNo` (Unsigned32) | 版本号，对应 CoE 0x1018:03 |
| | 文本内容 | 设备类型字符串 |
| `<Name>` | `LcId` (可选) | 本地化 ID，如 `LcId="1033"` 表示英文 |
| | 文本内容 | 设备名称，如 "RA8 EtherCAT" |

> — ETG.2000, Clause 6.3 / **Table 4 – ESI Device Element**

#### 3.3.2 SyncManager（SM）定义 —— **必须！**

`<Sm>` 定义了从站的 SyncManager 配置，是**进入 SAFEOP/OP 状态的基础**。

> 数据来源：ETG.2000, **Clause 6.3.1 / Table 5 – ESI Sm Element**

| 属性 | 数据类型 | 说明 |
|---|---|---|
| `DefaultSize` | Unsigned16 | 默认大小（Byte） |
| `StartAddress` | Hex | SM 物理起始地址（ESC DPRAM 偏移） |
| `ControlByte` | Hex | 控制字节，定义 SM 操作模式 |
| `Enable` | Boolean | 是否使能（1=使能，0=禁用） |

**典型 SM 配置（4 个 SM）**：

| SM | 用途 | 典型 StartAddress | 典型 ControlByte |
|---|---|---|---|
| SM0 | Mailbox Out（主站→从站） | 0x1000 | 0x26 |
| SM1 | Mailbox In（从站→主站） | 0x1080 | 0x22 |
| SM2 | Process Data Outputs（主站→从站） | 0x1100 | 0x24 |
| SM3 | Process Data Inputs（从站→主站） | 0x1180 | 0x20 |

> **关键**：若缺少 SM2/SM3 定义，从站无法支持过程数据通信，主站会在 PREOP→SAFEOP 转换时返回 **0x1E (Invalid Input Configuration)** 或 **0x1D (Invalid Output Configuration)**。
>
> — ET9300, Chapter 17

#### 3.3.3 FMMU 定义 —— **必须！**

`<Fmmu>` 定义了 FMMU（Fieldbus Memory Management Unit）的用途映射。

> 数据来源：ETG.2000, **Clause 6.3.2 / Table 6 – ESI Fmmu Element**

| 属性 | 数据类型 | 说明 |
|---|---|---|
| 文本内容 | String | FMMU 用途，可选值：`Outputs`、`Inputs`、`MBoxState` |

**典型 FMMU 配置**：

```xml
<Fmmu>Outputs</Fmmu>    <!-- FMMU0 映射输出过程数据 -->
<Fmmu>Inputs</Fmmu>     <!-- FMMU1 映射输入过程数据 -->
<Fmmu>MBoxState</Fmmu>  <!-- FMMU2 映射邮箱状态（可选） -->
```

#### 3.3.4 Mailbox（邮箱配置）

`<Mailbox>` 定义了从站支持的邮箱协议，是**进入 PREOP 状态的基础**。

> 数据来源：ETG.2000, **Clause 6.3.3 / Table 7 – ESI Mailbox Element**

| 子元素 | 属性 | 说明 |
|---|---|---|
| `<CoE>` | `SdoInfo` (Boolean) | 支持 SDO Info 服务 |
| | `PdoAssign` (Boolean) | 支持 PDO 动态映射（PDO Assign） |
| | `PdoConfig` (Boolean) | 支持 PDO 配置 |
| | `CompleteAccess` (Boolean) | 支持 SDO 完全访问 |
| | `UploadAtStartup` (Boolean) | 启动时自动上传 |
| `<FoE>` | (无属性) | 支持 File over EtherCAT |
| `<EoE>` | (无属性) | 支持 Ethernet over EtherCAT |
| `<SoE>` | (无属性) | 支持 Servo Drive over EtherCAT |

> **注意**：若声明了 `<CoE>`，则必须同时确保对象字典（0x1C12/0x1C13）和 PDO 映射正确配置。

#### 3.3.5 TxPdo / RxPdo（过程数据定义）—— **进入 OP 状态必须！**

> 数据来源：ETG.2000, **Clause 6.3.4 / Table 8 – ESI TxPdo/RxPdo Element**

| 属性 | 数据类型 | 说明 |
|---|---|---|
| `Fixed` | Boolean | 是否为固定映射（1=固定，0=可动态配置） |
| `Mandatory` | Boolean | 是否强制使用 |
| `Sm` | Unsigned8 | 关联的 SyncManager 编号 |

**子元素 `<Entry>`**：

| 属性 | 数据类型 | 说明 |
|---|---|---|
| `Index` | Hex | 对象字典索引（如 0x6000） |
| `SubIndex` | Hex | 子索引（如 0x01） |
| `BitLen` | Unsigned8 | 数据位宽 |
| `DataType` | String | 数据类型（如 `BIT1`、`UINT8`、`INT16` 等） |
| `Name` | String | 条目名称 |

**典型 PDO 配置示例**：

```xml
<RxPdo Fixed="1" Mandatory="1" Sm="2">
  <Index>#x1600</Index>
  <Name>Outputs</Name>
  <Entry>
    <Index>#x7000</Index>
    <SubIndex>1</SubIndex>
    <BitLen>8</BitLen>
    <DataType>UINT8</DataType>
    <Name>Output Byte 1</Name>
  </Entry>
</RxPdo>

<TxPdo Fixed="1" Mandatory="1" Sm="3">
  <Index>#x1A00</Index>
  <Name>Inputs</Name>
  <Entry>
    <Index>#x6000</Index>
    <SubIndex>1</SubIndex>
    <BitLen>8</BitLen>
    <DataType>UINT8</DataType>
    <Name>Input Byte 1</Name>
  </Entry>
</TxPdo>
```

#### 3.3.6 DC（Distributed Clock）配置

> 数据来源：ETG.2000, **Clause 6.3.5 / Table 9 – ESI Dc Element**

若从站支持分布式时钟（DC），需定义 `<Dc>` 元素：

| 子元素 | 属性 | 说明 |
|---|---|---|
| `<OpMode>` | `Name` | 操作模式名称 |
| | `Desc` | 描述 |
| `<AssignActivate>` | Hex | DC 激活字（写入 0x0981 寄存器） |
| `<CycleTimeSync0>` | 数值 | Sync0 周期（ns） |
| `<ShiftTimeSync0>` | 数值 | Sync0 偏移（ns） |
| `<CycleTimeSync1>` | 数值 | Sync1 周期（ns） |

#### 3.3.7 EEPROM 配置

> 数据来源：ETG.2000, **Clause 6.3.6 / Table 10 – ESI Eeprom Element**

| 子元素 | 说明 |
|---|---|
| `<ByteSize>` | EEPROM 总容量（Byte） |
| `<ConfigData>` | 配置数据（PDI Control、PDI Config、Vendor/Product/Rev/Serial ID、Mailbox 偏移/大小等），按 **ETG.1000.6 Table 16** 格式组织 |
| `<BootStrap>` | Bootstrap 模式下的邮箱配置，包含 `<Send>` 和 `<Recv>` 子元素 |

> **注意**：`<ConfigData>` 的内容与 SII 固定头部完全对应，由 SSC Tool 根据 ESI 中的设备信息自动生成。

---

## 四、对象字典（Object Dictionary）

> 数据来源：ETG.2000, **Clause 6.3.7 / Table 11 – ESI Dictionary Element**

ESI 中可以内嵌完整的 CoE 对象字典，供主站离线浏览和配置。对象字典以 `<Dictionary>` → `<Object>` 的层级组织：

| 元素 | 属性 | 说明 |
|---|---|---|
| `<Object>` | `Index` | 对象索引（如 0x1600、0x1C12、0x6000） |
| | `SubIndex` | 子索引（仅对子条目） |
| | `Type` | 数据类型（如 `UINT8`、`UINT16`、`ARRAY`、`RECORD`） |
| | `BitSize` | 位宽 |
| | `Access` | 访问权限（`ro`=只读、`rw`=读写、`wo`=只写） |
| `<Name>` | | 对象名称 |
| `<Data>` | | 默认值（Hex 格式） |
| `<Info>` | | 描述信息 |

**关键对象**：

| 索引 | 说明 |
|---|---|
| 0x1000 | Device Type |
| 0x1018 | Identity Object（Vendor ID、Product Code、Revision、Serial） |
| 0x1C00 | SyncManager Communication Type |
| 0x1C12 | RxPDO Assign（输出 PDO 映射） |
| 0x1C13 | TxPDO Assign（输入 PDO 映射） |
| 0x1600~0x17FF | RxPDO 映射 |
| 0x1A00~0x1BFF | TxPDO 映射 |

---

## 五、信息优先级与一致性要求

> 数据来源：ETG.2000, Clause 4.2 / ET9300, Chapter 14

### 5.1 信息一致性

ESI 文件中的以下信息**必须与从站固件中的常量完全一致**：

| ESI 中定义 | 从站固件中对应 |
|---|---|
| `<Sm>` StartAddress / DefaultSize | SSC 源码中的 SM 配置常量 |
| `<TxPdo>` / `<RxPdo>` Index / Entry | SSC 源码中的 PDO 映射表 |
| `<Mailbox>` 协议声明 | SSC 栈编译选项（如 `COE_SUPPORTED`、`FOE_SUPPORTED`） |
| `<Type>` ProductCode / RevisionNo | SSC 源码中的 `aESibuf` 或 `u32ProdCode`、`u32Revision` |

若不一致，主站在 PREOP→SAFEOP 转换时会检测到配置不匹配，返回 **AL Status Code 0x1D / 0x1E**。

### 5.2 最小必需信息总结

| 状态转换 | 必需信息 |
|---|---|
| **INIT → PREOP** | `<Mailbox>`（至少 CoE）、`<Sm>` SM0/SM1、`<Eeprom>` ConfigData |
| **PREOP → SAFEOP** | `<Sm>` SM2/SM3、`<Fmmu>` Outputs/Inputs、`<TxPdo>` / `<RxPdo>` |
| **SAFEOP → OP** | 对象字典 0x1C12/0x1C13 中的 PDO 映射与 ESI 一致 |
| **支持 DC** | `<Dc>` AssignActivate、CycleTimeSync0 |

---

## 六、引用文档

1. **ETG.2000 S (R) V1.0.10** — EtherCAT Slave Information (ESI) Specification (2018-02-01)
   - Clause 4: General
   - Clause 6: XML structure
   - Table 3: ESI Vendor Element
   - Table 4: ESI Device Element
   - Table 5: ESI Sm Element
   - Table 6: ESI Fmmu Element
   - Table 7: ESI Mailbox Element
   - Table 8: ESI TxPdo/RxPdo Element
   - Table 9: ESI Dc Element
   - Table 10: ESI Eeprom Element
   - Table 11: ESI Dictionary Element

2. **ETG.1000.6 S (R) V1.0.4** — EtherCAT Specification – Part 6: Application Layer protocol specification
   - Table 16: Slave Information Interface Area（EEPROM 固定头部格式）

3. Beckhoff Application Note **ET9300** — EtherCAT Slave Stack Code
