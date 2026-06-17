# FMMU 说明

## 1. FMMU 概述

**FMMU（Fieldbus Memory Management Unit，现场总线内存管理单元）** 是 EtherCAT 从站 ESC 内部的地址映射单元。

它的作用是把 EtherCAT 帧中的**逻辑地址**映射到 ESC 内部 RAM 的**物理地址**，从而实现主站对从站过程数据的按位/按字节寻址。

```text
主站 LRW 帧
    │
    │ 逻辑地址 0x00000000
    ▼
┌─────────┐
│  FMMU0  │ ──→ 物理地址 0x1100 (SM2，输出)
└─────────┘
    │ 逻辑地址 0x00000004
    ▼
┌─────────┐
│  FMMU1  │ ──→ 物理地址 0x1400 (SM3，输入)
└─────────┘
```

## 2. FMMU 寄存器地址依据

### 2.1 项目内依据

**R-IN32 ESC CMSIS 头文件**：

`ra/fsp/src/bsp/cmsis/Device/RENESAS/Include/R7KA8T2LF_core0.h`

```c
__IM uint32_t FMMU0_L_START_ADR;  /*!< (@ 0x00000600) FMMU Logical Start Address 0 Register */
__IM uint16_t FMMU0_LEN;          /*!< (@ 0x00000604) FMMU Length 0 Register                */
__IM uint8_t  FMMU0_L_START_BIT;  /*!< (@ 0x00000606) FMMU Logical Start Bit 0 Register     */
__IM uint8_t  FMMU0_L_STOP_BIT;   /*!< (@ 0x00000607) FMMU Logical Stop Bit 0 Register      */
__IM uint16_t FMMU0_P_START_ADR;  /*!< (@ 0x00000608) FMMU Physical Start Address 0 Register*/
__IM uint8_t  FMMU0_P_START_BIT;  /*!< (@ 0x0000060A) FMMU Physical Start Bit 0 Register    */
```

**项目文档**：

`docs/note/上电扫描.md`

```c
ecx_BWR(&context->port, 0x0000, ECT_REG_FMMU0, 16 * 3, &zbuf, EC_TIMEOUTRET3);
```

> 清零 FMMU 映射（`0x0600` 起）

### 2.2 通用依据

- **ETG.1000.4** — EtherCAT Data Link Layer Protocol Specification
- **Beckhoff ET1100 数据手册**

ESC 寄存器地址空间中，`0x0600` ~ `0x06FF` 区域用于 FMMU 配置。

## 3. FMMU 寄存器布局

每个 FMMU 占用 **16 bytes**：

| 寄存器 | 偏移 | 大小 | 说明 |
|--------|------|------|------|
| Logical Start Address | +0 | 4 bytes | 逻辑起始地址 |
| Length | +4 | 2 bytes | 映射长度 |
| Logical Start Bit | +6 | 1 byte | 逻辑起始位 |
| Logical Stop Bit | +7 | 1 byte | 逻辑结束位 |
| Physical Start Address | +8 | 2 bytes | 物理起始地址 |
| Physical Start Bit | +10 | 1 byte | 物理起始位 |
| Type | +11 | 1 byte | 0x01=Input, 0x02=Output |
| Activate | +12 | 1 byte | 0x01=激活 |
| 保留 | +13 ~ +15 | 3 bytes | 填 0 |

FMMU 基址计算：

```text
FMMU0: 0x0600 + 0 * 16 = 0x0600
FMMU1: 0x0600 + 1 * 16 = 0x0610
FMMU2: 0x0600 + 2 * 16 = 0x0620
...
```

## 4. 本项目 FMMU 配置实例

### 4.1 从站信息

- 从站：Renesas RA8T2 + R-IN32 ESC + Beckhoff SSC SampleAppl
- 输出 PDO：`0x7000` OutputCounter，32 bits → 4 bytes
- 输入 PDO：`0x6000` InputCounter，32 bits → 4 bytes
- SM2：`0x1100`，4 bytes，输出
- SM3：`0x1400`，4 bytes，输入

### 4.2 FMMU0（输出）

| 字段 | 值 | 说明 |
|------|-----|------|
| LogStart | `0x00000000` | 主站分配的逻辑地址起点 |
| Length | `4` | 输出 PDO 长度 |
| LogStartBit | `0` | 按字节对齐 |
| LogStopBit | `7` | 每个字节用满 8 bits |
| PhysStart | `0x1100` | SM2 物理地址 |
| PhysStartBit | `0` | 按字节对齐 |
| Type | `0x02` | Output |
| Activate | `0x01` | 使能 |

完整 16 bytes：

```text
00 00 00 00  04 00  00 07  00 11  00 02 01  00 00 00
└LogStart─┘ └Len┘ └bits┘ └Phys┘ P T A └保留┘
```

### 4.3 FMMU1（输入）

| 字段 | 值 | 说明 |
|------|-----|------|
| LogStart | `0x00000004` | 紧接输出之后 |
| Length | `4` | 输入 PDO 长度 |
| PhysStart | `0x1400` | SM3 物理地址 |
| Type | `0x01` | Input |

完整 16 bytes：

```text
04 00 00 00  04 00  00 07  00 14  00 01 01  00 00 00
```

## 5. 数据来源说明

| 字段 | 数据来源 |
|------|----------|
| `LogStart` | 主站分配。本项目代码中 `PDO_LOG_ADDR = 0x00000000` |
| `Length` | 来自 PDO 映射对象：`0x1600` / `0x1A00` 中 `0x70000020` / `0x60000020`，即 32 bits = 4 bytes |
| `PhysStart` | 来自 ESI XML / EEPROM：`temp/RA8 EtherCAT.xml` 中 `<Sm StartAddress="#x1100">` / `<Sm StartAddress="#x1400">` |
| `Type` | 由 PDO 方向决定：`0x1C12`（RxPDO，输出）→ Output；`0x1C13`（TxPDO，输入）→ Input |
| `Activate` | 配置完成后写 1 使能 |

## 6. 手动配置代码对应关系

`MaterEC/src/pdo_manual.c` 中：

```c
#define PDO_LOG_ADDR  0x00000000U
#define SM2_ADDR      0x1100
#define SM3_ADDR      0x1400

/* FMMU0：输出 */
write_fmmu(&ctx, 1, 0, PDO_LOG_ADDR,     4, SM2_ADDR, 2);

/* FMMU1：输入 */
write_fmmu(&ctx, 1, 1, PDO_LOG_ADDR + 4, 4, SM3_ADDR, 1);
```

`write_fmmu()` 内部填充 `ec_fmmut` 结构：

```c
fmmu.LogStart     = log_start;
fmmu.LogLength    = log_len;
fmmu.LogStartbit  = 0;
fmmu.LogEndbit    = 7;
fmmu.PhysStart    = phys_start;
fmmu.PhysStartBit = 0;
fmmu.FMMUtype     = fmmu_type;   // 2=output, 1=input
fmmu.FMMUactive   = 1;
```

然后通过 `ecx_FPWR()` 写入 ESC：

```c
ecx_FPWR(&ctx->port, configadr,
         0x0600 + fmmu_idx * 16, sizeof(fmmu), &fmmu, EC_TIMEOUTRET);
```

## 7. Wireshark 279 号帧解析

**Frame 279** 是主机向从站写入 FMMU0 配置的请求帧：

```text
No. 279 0.049105704  01:01:01:01:01:01 → Broadcast  ECAT 44
'FPWR': Len: 16, Adp 0x1001, Ado 0x600, Wc 0
```

展开数据段：

```text
FMMU: 00000000040000070011000201000000
```

逐字段解析：

| 字节 | 值 | 字段 | 含义 |
|------|-----|------|------|
| 0~3 | `00 00 00 00` | LogStart | `0x00000000` |
| 4~5 | `04 00` | Length | `0x0004` = 4 |
| 6 | `00` | LogStartBit | bit 0 |
| 7 | `07` | LogStopBit | bit 7 |
| 8~9 | `00 11` | PhysStart | `0x1100` |
| 10 | `00` | PhysStartBit | bit 0 |
| 11 | `02` | Type | Output |
| 12 | `01` | Activate | 使能 |
| 13~15 | `00 00 00` | 保留 | 0 |

**Frame 280** 是从站响应：

```text
No. 280 0.049380864  03:01:01:01:01:01 → Broadcast  ECAT 60
'FPWR': Len: 16, Adp 0x1001, Ado 0x600, Wc 1
```

`WKC=1` 表示从站成功接收并写入了 16 bytes 的 FMMU0 配置。

## 8. 常见问题

### 8.1 逻辑地址可以改吗？

可以。逻辑地址由主站分配，只要不与其他 FMMU 冲突即可。但修改 `LogStart` 后，`ecx_LRW` 访问的逻辑地址也必须同步修改。

### 8.2 物理地址可以改吗？

不建议。SM 物理地址（`0x1100`、`0x1400`）由从站 ESI/EEPROM 决定，必须按从站规定配置。

### 8.3 为什么每个 FMMU 占 16 bytes？

这是 EtherCAT 规范定义的寄存器布局。即使实际有效字段只有 13 bytes，也按 16 bytes 对齐，方便通过 `FMMU 索引 × 16` 计算地址。

### 8.4 逻辑地址由谁分配？主站的分配原则是什么？

**逻辑地址完全由主站统一分配**，ESC 只是被动接收主站写入 FMMU 寄存器的映射关系。

主站分配原则：

1. **不重叠**：同一 EtherCAT 网络中，任意两个 FMMU 映射的逻辑地址范围不能重叠。
2. **连续紧凑**：通常把所有从站的 PDO 按顺序连续排列，减少 LRW 帧长度。
3. **同一从站输出/输入相邻**：方便用一个 LRW 帧同时完成写输出和读输入。
4. **按 group 独立分配**：SOEM 中每个 group 有独立的逻辑地址空间。

SOEM 的 `ecx_config_map_group()` 会自动完成分配，通常从逻辑地址 `0` 开始，输出 PDO 在前，输入 PDO 在后。

| 地址类型 | 由谁决定 | 能否修改 |
|----------|----------|----------|
| 逻辑地址（LogStart） | 主站 | 可改 |
| 物理地址（PhysStart） | 从站 ESI/EEPROM | 不建议改 |

### 8.5 LogStartBit / LogStopBit 是什么意思？

这两个字段用于 **位级别的 FMMU 映射控制**。

| 字段 | 说明 |
|------|------|
| `LogStartBit` | 逻辑起始地址字节中，从第几位开始映射（0~7） |
| `LogStopBit` | 逻辑结束地址字节中，映射到第几位结束（0~7） |

**举例**：

```text
LogStart     = 0x00000000
LogLength    = 1
LogStartBit  = 2
LogStopBit   = 4
```

表示映射逻辑地址 `0x00000000` 的 **bit 2 ~ bit 4**，共 3 bits：

```text
字节 0x00000000:
  bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0
                    ↑____↑
                  映射这 3 位
```

**本项目为什么用 0 和 7**：

因为 `OutputCounter0x7000` / `InputCounter0x6000` 都是 32 bits 整数，按字节对齐：

```text
LogStart    = 0x00000000
LogLength   = 4
LogStartBit = 0
LogStopBit  = 7
```

表示映射 `4 × 8 = 32 bits`。

### 8.6 PhysStart 0x1100 是怎么来的？

`PhysStart = 0x1100` 来自**从站 ESC 的 EEPROM/ESI 配置**。

**直接来源**：

`temp/RA8 EtherCAT.xml`：

```xml
<Sm DefaultSize="4" StartAddress="#x1100" ControlByte="#x64" Enable="1">Outputs</Sm>
<Sm DefaultSize="4" StartAddress="#x1400" ControlByte="#x20" Enable="1">Inputs</Sm>
```

**数据流向**：

```text
ESI XML / EEPROM SII
       │
       ▼
SOEM ecx_config_init() 读取
       │
       ▼
写入 FMMU0.PhysStart = 0x1100
写入 FMMU1.PhysStart = 0x1400
```

SM 物理地址指向 ESC 内部 RAM 的实际位置，由从站硬件/固件设计决定，**主站不能随意修改**。如果改错地址，从站应用程序就无法正确读写过程数据。

### 8.7 Type 字段的 Output / Input 是相对于主站还是从站？

**相对于主站。**

| FMMU Type | 主站操作 | 从站操作 | 典型应用 |
|-----------|----------|----------|----------|
| **Output (0x02)** | 主站 **写** | 从站 **读** | 主站控制从站 LED |
| **Input (0x01)** | 主站 **读** | 从站 **写** | 从站上报拨码开关状态 |

对应 PDO 术语：

| 术语 | 主站视角 | 从站视角 | 相关对象 |
|------|----------|----------|----------|
| **Output** | 主站输出 | 从站接收（RxPDO） | `0x1C12`, `0x1600`, `0x7000` |
| **Input** | 主站输入 | 从站发送（TxPDO） | `0x1C13`, `0x1A00`, `0x6000` |

`OutputCounter0x7000` 和 `InputCounter0x6000` 中的 Output/Input 也是从**主站视角**命名的：

- `OutputCounter0x7000`：主站输出给从站 → 控制 LED
- `InputCounter0x6000`：主站从从站读入 → 读取 DipSw
