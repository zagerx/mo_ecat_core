# EtherCAT Datagram 在单个 ESC 中的解析流程

本文档以 **单个从站**（如 RA8T2 板子）为视角，详细拆解 ESC 硬件收到一个 EtherCAT 帧后，内部如何逐字段解析 Datagram。

---

## 一、总体流程图

```
帧从 Port0 进入 ESC
    │
    ▼
[1] 以太网层检查：目的 MAC、EtherType = 0x88A4
    │
    ▼
[2] 解析 EtherCAT 头：Length、Reserved、Type
    │
    ▼
[3] 进入 Datagram 处理循环（可能有多个 Datagram）
    │
    ▼
[4] 读取 CMD 字段 → 决定后续解析路径
    │
    ▼
[5] 根据 CMD 类型解析地址字段
    │
    ▼
[6] 地址匹配判断
    ├── 不匹配 → 跳到 [8]
    └── 匹配   → 继续 [7]
    │
    ▼
[7] 数据操作（读/写/读写）
    │
    ▼
[8] 更新 WKC（如果命中且操作成功）
    │
    ▼
[9] 检查 C bit（循环标志）→ 决定是否转发
    │
    ▼
[10] 检查 NEXT bit → 是否有下一个 Datagram
     ├── 是 → 回到 [3]
     └── 否 → 结束
```

---

## 二、逐字段详解：ESC 内部处理步骤

### 步骤 1~2：以太网层 + EtherCAT 头

ESC 硬件自动完成，不需要 MCU 干预：

| 检查项 | 条件 | 失败处理 |
|--------|------|---------|
| 目的 MAC | 广播 `FF:FF:FF:FF:FF:FF` 或匹配自身 MAC | 丢弃 |
| EtherType | `0x88A4` | 丢弃（不是 EtherCAT 帧）|
| EtherCAT Header | Length、Type 合法 | 丢弃 |

> **注意**：EtherCAT 帧的 MAC 地址通常不重要（Master 可以用任意源 MAC），关键是 EtherType = `0x88A4`。

---

### 步骤 3：进入 Datagram 处理

一个 EtherCAT 帧可以包含**多个 Datagram**，通过 `NEXT` bit 链接：

```
EtherCAT Header
    │
    ├── Datagram 1 (NEXT=1)
    │
    ├── Datagram 2 (NEXT=1)
    │
    ├── Datagram 3 (NEXT=0) ← 最后一个
```

ESC 按顺序逐个处理。

---

### 步骤 4：读取 CMD 字段（1 Byte）

ESC 读取 Datagram 的第 1 个字节（CMD），决定后续解析逻辑：

| CMD | 类型 | 地址字段格式 | ESC 后续动作 |
|-----|------|-------------|-------------|
| `0x01` | APRD | ADP (2B) + ADO (2B) | 自增式物理读 |
| `0x02` | **APWR** | ADP (2B) + ADO (2B) | 自增式物理写 |
| `0x04` | FPRD | ADP (2B) + ADO (2B) | 配置式物理读 |
| `0x05` | **FPWR** | ADP (2B) + ADO (2B) | 配置式物理写 |
| `0x07` | BRD | 忽略 | 广播读 |
| `0x08` | BWR | 忽略 | 广播写 |
| `0x0A` | LRD | ADR (4B) | 逻辑读 |
| `0x0B` | LWR | ADR (4B) | 逻辑写 |
| `0x0C` | **LRW** | ADR (4B) | 逻辑读写 |

> **关键分叉点**：CMD 决定了步骤 5 怎么解析地址。

---

### 步骤 5：根据 CMD 解析地址

#### 路径 A：自增式物理寻址（APRD / APWR）

```
读取 ADP 字段（2 Bytes）
    │
    ▼
判断：ADP == 0 ?
    ├── 是 → 本从站命中！继续步骤 6
    └── 否 → 本从站不命中
    │
    ▼
ADP = ADP + 1（无条件递增）
    │
    ▼
读取 ADO 字段（2 Bytes）→ 物理寄存器地址
```

**示例**：APWR 写 Station Address（`ADO = 0x0010`）

| 场景 | 收到 ADP | 判断 | 递增后 ADP | 结果 |
|------|---------|------|-----------|------|
| 访问第 1 个从站 | `0x0000` | == 0 ✓ | `0x0001` | **命中** |
| 访问第 2 个从站 | `0xFFFF` | ≠ 0 | `0x0000` | 不命中，但下一从站收到 0 |
| 访问第 3 个从站 | `0xFFFE` | ≠ 0 | `0xFFFF` | 不命中 |

#### 路径 B：配置式物理寻址（FPRD / FPWR）

```
读取 ADP 字段（2 Bytes）
    │
    ▼
读取 ESC 内部寄存器 0x0010（Configured Station Address）
    │
    ▼
判断：ADP == 0x0010 的值 ?
    ├── 是 → 本从站命中！
    └── 否 → 本从站不命中
    │
    ▼
读取 ADO 字段（2 Bytes）→ 物理寄存器地址
```

**示例**：FPRD 读 DL Status（`ADP = 0x1001`，`ADO = 0x0110`）

| 场景 | 收到 ADP | 0x0010 值 | 判断 | 结果 |
|------|---------|----------|------|------|
| 访问本从站 | `0x1001` | `0x1001` | 相等 ✓ | **命中** |
| 访问其他从站 | `0x1002` | `0x1001` | 不相等 | 不命中 |

#### 路径 C：广播式（BRD / BWR）

```
不需要解析地址
    │
    ▼
直接命中！所有从站都执行
```

#### 路径 D：逻辑寻址（LRD / LWR / LRW）

```
不使用 ADP/ADO！
    │
    ▼
读取 ADR 字段（4 Bytes）→ 逻辑地址
    │
    ▼
查 FMMU 表（硬件自动查找）
    │
    ▼
判断：ADR 是否落在某个 FMMU 的逻辑地址范围内？
    ├── 是 → 命中！FMMU 给出物理地址（SM 缓冲区）
    └── 否 → 不命中
```

**示例**：LRW（`ADR = 0x00000000`，LEN = 12）

| FMMU | 逻辑地址范围 | 物理地址映射 | 判断 |
|------|-------------|-------------|------|
| FMMU0 | `0x0000~0x0003` | SM2: `0x1100~0x1103` | ADR 命中 ✓ |
| FMMU1 | `0x0100~0x0103` | SM3: `0x1180~0x1183` | ADR+0x100 命中 ✓ |

> **注意**：单个 Datagram 的 ADR 是一个**连续的逻辑地址范围**，多个从站的 FMMU 可以映射到这个范围的不同片段。

---

### 步骤 6：地址匹配判断

根据步骤 5 的结果：

| 结果 | 处理 |
|------|------|
| **匹配** | 继续步骤 7（数据操作） |
| **不匹配** | 跳到步骤 8（WKC 不增加），然后步骤 9（转发） |

---

### 步骤 7：数据操作

ESC 根据 CMD 和 ADO（或 FMMU 映射出的物理地址）执行操作：

#### 7.1 读操作（APRD / FPRD / BRD / LRD）

```
根据 ADO 或 FMMU 物理地址
    │
    ▼
从 ESC RAM / 寄存器读取数据
    │
    ▼
写入 Datagram 的 DATA 区域（覆盖原有内容）
```

**示例**：APRD 读 PDI Control（`ADO = 0x0140`，LEN = 2）

```
DATA 原值：00 00
    │
    ▼
读取 ESC 寄存器 0x0140 = 0x0002（假设是 SPI 接口）
    │
    ▼
DATA 新值：02 00（little-endian）
```

#### 7.2 写操作（APWR / FPWR / BWR / LWR）

```
从 Datagram 的 DATA 区域读取数据
    │
    ▼
写入 ESC RAM / 寄存器（ADO 或 FMMU 物理地址）
    │
    ▼
DATA 保持不变（Master 发出的数据原样保留在帧中）
```

**示例**：APWR 写 Station Address（`ADO = 0x0010`，DATA = `02 10`）

```
DATA 值：02 10
    │
    ▼
写入 ESC 寄存器 0x0010 = 0x1002
    │
    ▼
DATA 仍为：02 10（不变）
```

#### 7.3 读写操作（LRW）

```
同时进行"写"和"读"：
    │
    ├── 写分支：从 DATA 取输出数据 → 写入 SM2（FMMU0 映射）
    │
    └── 读分支：从 SM3（FMMU1 映射）读取数据 → 覆盖 DATA 对应位置
```

**示例**：LRW，单个从站，输出 4B + 输入 4B

```
Master 发出 DATA：[AA][BB][CC][DD][00][00][00][00]
                      ↑输出 4B↑      ↑输入占位 4B↑
    │
    ▼
ESC 处理：
  1. FMMU0：逻辑 0x0000~0x0003 → 物理 SM2 0x1100~0x1103
     写入：AA BB CC DD → SM2
  2. FMMU1：逻辑 0x0100~0x0103 → 物理 SM3 0x1180~0x1183
     读取：SM3 = 11 22 33 44 → 放入 DATA[4:7]
    │
    ▼
转发时 DATA：[AA][BB][CC][DD][11][22][33][44]
```

---

### 步骤 8：WKC 更新

WKC 的修改规则由 CMD 决定：

| CMD | WKC 规则 | 说明 |
|-----|---------|------|
| APRD / FPRD / BRD / LRD | +1 | 读成功 |
| APWR / FPWR / BWR / LWR | +1 | 写成功 |
| LRW | **+2（写）+1（读）= +3** | 同时读写 |
| 未命中 | **不修改** | — |

**示例**：

```
APWR 写 Station Address，命中：
    WKC 原值：0x0000
    WKC 新值：0x0001

LRW Process Data，命中：
    WKC 原值：0x0000
    WKC 新值：0x0003
```

> **注意**：WKC 位于 Datagram 末尾，ESC 硬件自动修改，不需要 MCU 参与。

---

### 步骤 9：转发决策（C bit 检查）

ESC 检查 Datagram 中的 **C bit**（循环帧标志）：

| C bit | 含义 | ESC 动作 |
|-------|------|---------|
| `0` | 帧还没绕环一周 | **转发到下一端口** |
| `1` | 帧已循环一次 | **不再转发**（避免无限循环）|

**单个从站的场景**：

```
Master ──Port0──► [RA8 从站] ──Port1──► （无下一从站）
                                      │
                                      ▼
                                    帧自动反向
                                      │
                                      ▼
Master ◄──Port0── [RA8 从站] ◄──Port1──
```

- 帧从 Port0 进入，处理后从 Port1 转发
- Port1 没有检测到下游从站（链路断开或未连接）
- 帧自动从 Port1 反向，从 Port0 返回 Master
- 返回过程中，ESC **不再修改任何字段**

---

### 步骤 10：NEXT bit 检查

ESC 检查 Datagram 中的 **NEXT** bit：

| NEXT | 含义 | ESC 动作 |
|------|------|---------|
| `1` | 后面还有 Datagram | 继续处理下一个 |
| `0` | 这是最后一个 Datagram | 结束处理 |

---

## 三、完整实例：APWR 写 Station Address

以你的项目为例，Master 发 **APWR** 给第 1 个从站（RA8）写入配置地址 `0x1001`。

### Master 发出的原始帧

```
Ethernet Header:
  目的 MAC: FF:FF:FF:FF:FF:FF
  源 MAC:   00:E0:4C:68:00:01
  EtherType: 88 A4

EtherCAT Header:
  Length: 0x000A (10 bytes)
  Reserved: 0
  Type: 1 (DLPDU)

Datagram:
  CMD:  0x02 (APWR)
  IDX:  0x01
  ADP:  0x0000
  ADO:  0x0010
  LEN:  0x0002
  IRQ:  0x0000
  DATA: 01 10        ← 要写入的值 0x1001（little-endian）
  WKC:  0x0000
```

### ESC 内部处理流程

```
[1] 收到帧，检查 EtherType = 0x88A4 → 通过

[2] 解析 EtherCAT Header，Length=10，Type=1 → 合法

[3] 进入 Datagram 处理（NEXT=0，只有一个 Datagram）

[4] 读取 CMD = 0x02 → APWR（自增式物理写）
    → 走"自增式物理寻址"路径

[5] 读取 ADP = 0x0000
    判断：ADP == 0 ? → YES！本从站命中
    ADP = 0x0000 + 1 = 0x0001
    读取 ADO = 0x0010 → 目标：Configured Station Address 寄存器

[6] 地址匹配 → 匹配 ✓

[7] 数据操作（写）：
    从 DATA 读取 2 字节：01 10
    写入 ESC 寄存器 0x0010 = 0x1001
    DATA 保持不变：01 10

[8] WKC 更新：
    APWR 命中 → WKC + 1
    WKC: 0x0000 → 0x0001

[9] C bit 检查：
    C = 0 → 帧未循环 → 转发到 Port1
    （Port1 无下游设备 → 自动反向 → 从 Port0 返回）

[10] NEXT = 0 → 无更多 Datagram → 处理结束
```

### 返回 Master 时的帧

```
Datagram:
  CMD:  0x02 (APWR)      ← 不变
  IDX:  0x01              ← 不变
  ADP:  0x0001           ← 已被从站1递增（原 0x0000 → 0x0001）
  ADO:  0x0010           ← 不变
  LEN:  0x0002           ← 不变
  IRQ:  0x0000           ← 不变
  DATA: 01 10            ← 不变（APWR 的 DATA 不会被修改）
  WKC:  0x0001           ← 已递增（0 → 1）
```

Master 收到后检查：
- WKC = 1 → 有一个从站响应了 → 写入成功
- 如果 WKC = 0 → 没有从站命中 → 错误（比如 ADP 算错了）

---

## 四、完整实例：LRW 交换 Process Data

Master 发 **LRW** 进行 PDO 交换。

### 前提条件（已由 SSC 配置好）

| FMMU | 逻辑地址范围 | 物理地址 | 映射对象 |
|------|-------------|---------|---------|
| FMMU0 | `0x0000~0x0003` | `0x1100~0x1103` | SM2（RxPDO，4B 输出）|
| FMMU1 | `0x0100~0x0103` | `0x1180~0x1183` | SM3（TxPDO，4B 输入）|

### Master 发出的原始帧

```
Datagram:
  CMD:  0x0C (LRW)
  IDX:  0x03
  ADR:  0x00000000       ← 逻辑地址（4 Bytes，不是 ADP+ADO）
  LEN:  0x0008           ← 8 字节 DATA
  IRQ:  0x0000
  DATA: 0F 00 01 02 00 00 00 00
          ↑输出 4B↑  ↑输入占位 4B↑
  WKC:  0x0000
```

### ESC 内部处理流程

```
[4] 读取 CMD = 0x0C → LRW（逻辑读写）
    → 走"逻辑寻址"路径

[5] 不使用 ADP/ADO！
    读取 ADR = 0x00000000
    查 FMMU0：逻辑 0x0000~0x0003 命中 → 物理 0x1100 (SM2)
    查 FMMU1：逻辑 0x0100~0x0103 命中 → 物理 0x1180 (SM3)
    （注意：ADR+0x0100 对应 FMMU1）

[6] 地址匹配 → FMMU 命中 ✓

[7] 数据操作（同时读写）：
    写分支：DATA[0:3] = 0F 00 01 02 → 写入 SM2 0x1100~0x1103
    读分支：读取 SM3 0x1180~0x1183（假设当前值是 05 03 00 00）
            → 覆盖 DATA[4:7]
    
    DATA 变化：
      原值：0F 00 01 02 00 00 00 00
      新值：0F 00 01 02 05 03 00 00
                ↑输出不变↑  ↑输入填入↑

[8] WKC 更新：
    LRW 命中 → 写成功 +2，读成功 +1
    WKC: 0x0000 → 0x0003

[9] C = 0 → 转发到 Port1 → 自动反向返回

[10] NEXT = 0 → 结束
```

### 返回 Master 时的帧

```
Datagram:
  CMD:  0x0C (LRW)
  IDX:  0x03
  ADR:  0x00000000       ← 不变
  LEN:  0x0008           ← 不变
  DATA: 0F 00 01 02 05 03 00 00  ← 输入区已被填入
  WKC:  0x0003           ← 已递增（0 → 3）
```

Master 收到后：
- 看到 `grp->outputs[0] = 0x0F`（输出原样返回）
- 看到 `grp->inputs[0] = 0x05`（从 SM3 读到的输入）
- WKC = 3 → 验证通过

---

## 五、字段变化速查表（单个从站视角）

| 命令 | CMD | ADP 变化 | ADO/ADR 变化 | DATA 变化 | WKC 变化 |
|------|-----|---------|-------------|----------|---------|
| **APRD** | 不变 | 收到后 +1 | 不变 | **命中时填入读取数据** | 命中 +1 |
| **APWR** | 不变 | 收到后 +1 | 不变 | **不变**（命中时读取 DATA 写入寄存器） | 命中 +1 |
| **FPRD** | 不变 | 不变 | 不变 | **命中时填入读取数据** | 命中 +1 |
| **FPWR** | 不变 | 不变 | 不变 | **不变** | 命中 +1 |
| **BRD** | 不变 | 不变 | 不变 | **所有从站 OR 数据** | 每站 +1 |
| **LRW** | 不变 | 不变 | 不变 | **输出区不变，输入区被填入** | 命中 +3 |

> **返回路径上**：所有字段**不再变化**，原样传回 Master。

---

## 六、MCU/SSC 在什么时候介入？

| 操作 | 谁执行 | 说明 |
|------|--------|------|
| 帧接收、CMD 解析、ADP 判断 | **ESC 硬件** | 全自动 |
| FMMU 地址映射 | **ESC 硬件** | 全自动 |
| 数据读写（ESC RAM ↔ Datagram） | **ESC 硬件** | 全自动 |
| WKC 递增 | **ESC 硬件** | 全自动 |
| 帧转发 | **ESC 硬件** | 全自动 |
| **SM2 数据读取**（如 LED 控制） | **MCU 软件** | `APPL_OutputMapping()` |
| **SM3 数据写入**（如 DIP 读取） | **MCU 软件** | `APPL_InputMapping()` |
| **状态机转换**（Init→PreOp→SafeOp→Op） | **SSC 软件** | `AL_ControlInd()` |

**核心结论**：Datagram 的解析、地址匹配、数据搬运、WKC 计算，全部是 **ESC 硬件自动完成**的。MCU/SSC 只负责后续的数据解释和状态管理。
