# EtherCAT 主从通信流程与状态管理

> 本文基于 **ETG.1000.2**、**ETG.1000.4**、**ETG.1000.6** 和 **IEC 61158 Type 12** 规范，结合 SOEM 源码，说明 EtherCAT 主站上电后如何与从站建立通信、管理从站状态，以及处理从站掉线等异常情况。

---

## 目录

- [一、状态机说明](#一状态机说明)
- [二、官方文档](#二官方文档)
- [三、标准上电通信流程](#三标准上电通信流程)
- [四、状态转换图](#四状态转换图)
- [五、主站如何管理从站](#五主站如何管理从站)
- [六、从站掉线处理](#六从站掉线处理)
- [七、AL Status Code](#七al-status-code)
- [八、总结](#八总结)

---

## 一、状态机说明

**INIT → PRE-OP → SAFE-OP → OP 是从站的 EtherCAT State Machine（ESM）。**

主站本身没有这些状态，主站的角色是：
- **发起状态转换请求**（写 `AL Control` 寄存器）
- **轮询确认状态**（读 `AL Status` 寄存器）
- **监控异常并恢复**

```
        主站写 AL Control        主站写 AL Control        主站写 AL Control
INIT  ───────────────────► PRE-OP ───────────────────► SAFE-OP ───────────────────► OP
  ▲                        │                            │                            │
  │                        │                            │                            │
  └────────────────────────┴────────────────────────────┴────────────────────────────┘
              从站通过 AL Status 反馈当前状态
```

| 状态 | 值 | 从站行为 |
|------|-----|---------|
| **INIT** | `0x0001` | 无 Mailbox，无 PDO |
| **PRE-OP** | `0x0002` | Mailbox（SDO）可用，PDO 不可用 |
| **SAFE-OP** | `0x0004` | Mailbox 可用，输入 PDO 可用，输出 PDO 忽略 |
| **OP** | `0x0008` | Mailbox 和 PDO 都可用 |

---

## 二、官方文档

| 文档 | 编号 | 内容 |
|------|------|------|
| **EtherCAT State Machine** | ETG.1000.2 | 定义 INIT、PRE-OP、SAFE-OP、OP 四个状态及转换规则 |
| **Data Link Layer** | ETG.1000.4 | 定义 EtherCAT 帧格式、寻址、ESC 寄存器、SM/FMMU |
| **Application Layer Services** | ETG.1000.5 | 定义 CoE/FoE/EoE/SoE 等 Mailbox 服务 |
| **Application Layer Protocol** | ETG.1000.6 | 定义 SDO、对象字典、SDO Info 等协议细节 |
| **Industrial Ethernet** | IEC 61158 Type 12 | EtherCAT 的国际标准 |

> 这些文档都是**付费会员文档**，需加入 EtherCAT Technology Group（ETG）才能下载。免费公开资料可参考 [ETG 官网](https://www.ethercat.org) 的入门白皮书。

---

## 三、标准上电通信流程

```
Power On
    │
    ▼
┌─────────┐
│  INIT   │
└────┬────┘
     │
     │ 1. 检测从站数量
     │ 2. 复位 ESC
     │ 3. 分配 Configured Station Address
     │ 4. 读取 EEPROM SII
     ▼
┌─────────┐
│ PRE-OP  │  ← SDO/Mailbox 通信
└────┬────┘
     │ 5. 配置 PDO 映射
     │ 6. 配置 SM/FMMU
     │ 7. 配置 DC（可选）
     ▼
┌─────────┐
│ SAFE-OP │  ← 输入 PDO 可用，输出 PDO 忽略
└────┬────┘
     │ 8. 开始周期性 PDO 通信
     ▼
┌─────────┐
│    OP   │  ← 输入/输出 PDO 都可用
└─────────┘
```

### 阶段 1：上电与硬件初始化（Power On → INIT）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | 上电，初始化网卡 | ESC 上电，从 EEPROM 加载配置 |
| 2 | — | ESC 自检完成，默认进入 **INIT** 状态 |
| 3 | 发送 BWR `AL Control = 0x0011` | 确认进入 INIT，清除错误 |
| 4 | 发送 BRD `Type/Revision` | 响应 WKC = 从站数 |

> **ETG.1000.2 规定**：从站上电后必须处于 INIT 状态，等待主站指令。

### 阶段 2：地址分配与配置读取（INIT 内）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | BWR 清零 FMMU/SM/错误计数器 | ESC 寄存器复位 |
| 2 | APRD 读 PDI Control | 返回 PDI 类型 |
| 3 | APWR 写 Configured Station Address | 保存地址到 `0x0010` |
| 4 | APRD 读回验证 | 确认地址写入成功 |
| 5 | FPRD 读 Alias / DL Status / EEPROM Status | 返回状态信息 |
| 6 | 读取 EEPROM SII | 返回 Mailbox/PDO 配置 |

> **ETG.1000.4 规定**：Configured Station Address 寄存器（`0x0010`）必须在 PRE-OP 前配置好。

### 阶段 3：进入 PRE-OP（INIT → PRE-OP）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | FPWR `AL Control = 0x0002` | 收到 PRE-OP 请求 |
| 2 | — | 启动 Mailbox Handler：`MBX_StartMailboxHandler()` |
| 3 | — | 配置并启用 SM0/SM1 |
| 4 | — | 设置 `AL Status = 0x0002` |
| 5 | FPRD `AL Status` | 返回 0x0002 |

> **ETG.1000.2 规定**：PRE-OP 状态下，Mailbox 通信可用，但过程数据通信不可用。

### 阶段 4：Mailbox 通信（PRE-OP 内）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | FPWR 写 SM0（完整 128B） | ESC 触发 MAILBOX_WRITE_EVENT |
| 2 | — | MCU 读取请求，处理 SDO |
| 3 | — | 写响应到 SM1（128B） |
| 4 | FPRD 读 SM1 | 获取响应数据 |

> **ETG.1000.5/6 规定**：CoE SDO 服务通过 Mailbox 传输，遵循 CANopen 对象字典模型。

### 阶段 5：进入 SAFE-OP（PRE-OP → SAFE-OP）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | 通过 SDO 配置 PDO 映射（`0x1C00` 等） | — |
| 2 | 配置 SyncManager 2/3 用于 PDO | — |
| 3 | 配置 Distributed Clock（可选） | — |
| 4 | FPWR `AL Control = 0x0004` | 收到 SAFE-OP 请求 |
| 5 | — | 启动输入 Handler，但不处理输出 |
| 6 | FPRD `AL Status` | 返回 0x0004 |

> **ETG.1000.2 规定**：SAFE-OP 状态下，输入过程数据可用，输出过程数据被忽略（从站不执行输出）。

### 阶段 6：进入 OP（SAFE-OP → OP）

| 步骤 | 主站 | 从站 |
|------|------|------|
| 1 | 周期性发送过程数据帧（PDO） | — |
| 2 | FPWR `AL Control = 0x0008` | 收到 OP 请求 |
| 3 | — | 启动输出 Handler |
| 4 | — | 设置 `AL Status = 0x0008` |
| 5 | FPRD `AL Status` | 返回 0x0008 |
| 6 | 周期性 PDO 通信开始 | 周期性执行输入/输出 |

> **ETG.1000.2 规定**：OP 状态下，Mailbox 和过程数据通信都可用。

---

## 四、状态转换图

```
                    ┌─────────┐
         ┌─────────│  INIT   │◄────────┐
         │         └────┬────┘         │
         │              │              │
         │              ▼              │
         │         ┌─────────┐         │
         │         │ PRE-OP  │         │
         │         └────┬────┘         │
         │              │              │
    Error│              ▼              │Error Ack
         │         ┌─────────┐         │
         └────────►│ SAFE-OP │─────────┘
                   └────┬────┘
                        │
                        ▼
                   ┌─────────┐
                   │    OP   │
                   └─────────┘
```

---

## 五、主站如何管理从站？

### 5.1 维护从站列表

SOEM 中 `ecx_contextt` 结构体的 `slavelist[]` 数组记录每个从站：

```c
typedef struct {
    uint16 configadr;           // 配置地址，如 0x1001
    uint16 aliasadr;            // Alias
    uint16 state;               // 当前状态
    uint16 ALstatuscode;        // AL 状态码，错误时非零
    uint16 eep_man;             // Vendor ID
    uint16 eep_id;              // Product Code
    uint16 eep_rev;             // Revision
    ec_smt SM[EC_MAXSM];        // SyncManager 配置
    ec_fmmut FMMU[EC_MAXFMMU];  // FMMU 配置
    // ...
} ec_slavet;
```

### 5.2 状态检查：`ecx_statecheck()`

```c
uint16 ecx_statecheck(ecx_contextt *context, uint16 slave, 
                      uint16 reqstate, int timeout);
```

功能：循环 FPRD 读取指定从站的 `AL Status (0x0130)`，直到状态匹配或超时。

示例：等待从站进入 PRE-OP
```c
ecx_FPWRw(&ctx.port, 0x1001, ECT_REG_ALCTL, 
          htoes(EC_STATE_PRE_OP), timeout);
state = ecx_statecheck(&ctx, 1, EC_STATE_PRE_OP, EC_TIMEOUTSTATE);
if (state == EC_STATE_PRE_OP) {
    // 成功
}
```

### 5.3 批量读取状态：`ecx_readstate()`

```c
int ecx_readstate(ecx_contextt *context);
```

功能：
1. 先发送一个 BRD `AL Status` 广播
2. 如果所有从站状态一致且无错误，直接更新所有 `slavelist[].state`
3. 否则逐个 FPRD 读取每个从站的 `AL Status + AL Status Code`

这是运行时**周期性状态监控**的常用函数。

### 5.4 Watchdog 监控

ESC 内部有两种 Watchdog：

| Watchdog | 监控对象 | 超时后果 |
|----------|---------|---------|
| **PDI Watchdog** | ESC 与 MCU（PDI）通信 | 从站可能降状态 |
| **Process Data Watchdog** | PDO 通信周期 | 从站自动降回 SAFE-OP 或 INIT |

如果从站长时间未收到主站帧，Watchdog 会触发，从站自动降状态。

---

## 六、从站掉线处理

### 6.1 检测方式

| 检测方式 | 现象 | 说明 |
|---------|------|------|
| **WKC=0** | FPRD/FPWR 返回 WKC=0 | 从站未响应，可能掉线 |
| **状态检查超时** | `ecx_statecheck()` 超时 | 从站未到达目标状态 |
| **BRD 从站数变少** | BRD `AL Status` 的 WKC < slavecount | 有从站离线 |
| **AL Status Error bit** | `AL Status & 0x0010` 被置位 | 从站报告错误 |
| **Watchdog 超时** | 从站自动降状态 | PDO 通信中断 |

### 6.2 SOEM 的恢复机制

#### `ecx_recover_slave()` —— 恢复单个从站

```c
int ecx_recover_slave(ecx_contextt *context, uint16 slave, int timeout);
```

流程：
1. 用 APRD 检查物理位置上的从站
2. 比较 Alias、Vendor ID、Product Code、Revision
3. 如果匹配，重新分配原来的 ConfigAdr
4. 如果不匹配，认为该位置的从站不是原来的设备

> 用于**热插拔恢复**：从站断线重连后，主站可以通过 Alias/ID 识别并恢复它。

#### `ecx_reconfig_slave()` —— 重新配置单个从站

```c
int ecx_reconfig_slave(ecx_contextt *context, uint16 slave, int timeout);
```

流程：
1. 将目标从站切回 INIT
2. 重新配置所有 SyncManager
3. 切到 PRE-OP
4. 执行 PRE-OP → SAFE-OP 的配置钩子
5. 切到 SAFE-OP
6. 重新配置 FMMU

> 用于**从站状态异常后快速恢复**，不用重新扫描整个网络。

### 6.3 主站可选的恢复策略

| 场景 | 策略 | SOEM 函数 |
|------|------|-----------|
| 单个从站短暂掉线 | 调用 `ecx_recover_slave()` + `ecx_reconfig_slave()` | 单个恢复 |
| 单个从站状态错误 | 切回 INIT 重新配置 | `ecx_reconfig_slave()` |
| 多个从站掉线或拓扑变化 | 重新扫描整个网络 | `ecx_config_init()` |
| 关键从站不可恢复 | 标记离线，停止相关功能 | 应用层处理 |
| 网络完全断开 | 主站进入安全模式 | 应用层处理 |

---

## 七、AL Status Code

当从站无法完成状态转换时，会在 `AL Status Code (0x0134)` 寄存器中写入错误码：

| 错误码 | 含义 |
|--------|------|
| `0x0011` | Invalid mailbox configuration |
| `0x0012` | Invalid mailbox configuration PRE-OP |
| `0x001D` | Invalid output configuration |
| `0x001E` | Invalid input configuration |
| `0x001F` | Invalid watchdog configuration |
| `0x0026` | SII information does not match firmware |
| `0x0028` | Object dictionary initialization failed |

主站可以通过 `ecx_statecheck()` 读取 `slavelist[slave].ALstatuscode` 获取具体错误原因。

---

## 八、总结

> **标准的 EtherCAT 上电流程由 ETG.1000.2 定义的状态机严格控制，必须按 INIT → PRE-OP → SAFE-OP → OP 的顺序转换。每个状态转换都需要主站通过 `AL Control` 请求，从站通过 `AL Status` 确认。**
>
> **主站管理从站的核心机制：维护 `slavelist[]`、用 `ecx_statecheck()` 轮询状态、用 `ecx_readstate()` 批量监控、用 Watchdog 检测活性。**
>
> **从站掉线通过 WKC=0、状态超时、BRD 从站数减少等方式检测，恢复策略包括 `ecx_recover_slave()` 热插拔恢复、`ecx_reconfig_slave()` 重新配置、或 `ecx_config_init()` 全网络重新扫描。**
