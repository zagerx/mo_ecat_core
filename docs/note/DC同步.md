# DC（Distributed Clock）分布式时钟同步

## 一、为什么需要 DC？

普通 EtherCAT 主站按固定周期发送 LRW 帧，这个周期由**主站操作系统调度**决定：

```text
主站周期: 1ms → 1ms → 1ms → ...
           │     │     │
           ▼     ▼     ▼
         从站A  从站A  从站A
```

问题：

- 主站周期会有抖动（jitter），比如 0.98ms、1.02ms、0.99ms
- 多个从站之间无法精确同步
- 对于伺服、运动控制等应用，需要亚微秒级同步

**DC 的作用**：让所有从站共享同一个高精度参考时钟，各从站按这个时钟产生硬件中断，实现精确同步。

---

## 二、ESC 内部时钟结构

每个 EtherCAT 从站控制器（ESC）内部都有高精度时钟模块，关键寄存器如下：

| 地址 | 寄存器 | 长度 | 作用 |
|------|--------|------|------|
| `0x0910` | System Time | 8 bytes | ESC 当前系统时间（64-bit，单位 ns） |
| `0x0920` | System Time Offset | 8 bytes | 时间偏移补偿值 |
| `0x0928` | System Time Delay | 4 bytes | 传播延迟补偿值 |
| `0x092C` | System Time Difference | 4 bytes | 与参考时钟的差值 |
| `0x0930` | Speed Counter Start | 2 bytes | 漂移补偿起始计数 |
| `0x0980` | Sync0 Cycle Time | 4 bytes | Sync0 周期 |
| `0x0988` | Sync0 Start Time | 8 bytes | Sync0 首次触发时间 |
| `0x0998` | Sync0 Activation | 1 byte | Sync0 使能 |

System Time 的计算：

```text
System Time = 本地时钟计数器 + System Time Offset + 漂移补偿
```

---

## 三、DC 同步的三个核心问题

要让所有从站的 System Time 一致，必须解决三个问题：

```text
1. 初始偏移不同   →  用 Offset Compensation 解决
2. 晶振频率不同   →  用 Drift Compensation 解决
3. 帧传播延迟不同 →  用 Propagation Delay Compensation 解决
```

---

## 四、传播延迟补偿（Propagation Delay）

### 4.1 为什么需要传播延迟补偿？

假设网络拓扑：

```text
主站 ──[t1]── 从站A（参考时钟）──[t2]── 从站B
```

当主站读取从站A和从站B的时间时：

- 读取从站A的帧需要传播时间 `t1`
- 读取从站B的帧需要传播时间 `t1 + t2`

如果不补偿，从站B的时间看起来总是比从站A"老" `t2`。

### 4.2 EtherCAT 如何测量传播延迟

EtherCAT 的每个 ESC 端口都有硬件时间戳功能。DC 延迟测量利用 **Distributed Clock Unit Control** 寄存器，通过以下步骤：

**第一步：主站发送一个特殊的延迟测量帧**

该帧经过所有从站时，每个 ESC 记录：

- 帧进入 ESC 的时间戳 `T_in`
- 帧离开 ESC 的时间戳 `T_out`

**第二步：计算每个链段的延迟**

```text
链段延迟 = (T_out_从站A - T_in_从站A) / 2
```

因为 EtherCAT 帧是双向环回，所以每个链段的单向延迟等于往返差的一半。

**第三步：计算每个从站到参考时钟的总延迟**

以从站A为参考：

```text
从站B 的延迟 = t2
从站C 的延迟 = t2 + t3
...
```

### 4.3 传播延迟补偿实例

假设测得：

```text
主站到从站A延迟: 100 ns
主站到从站B延迟: 150 ns
```

以从站A为参考时钟，从站B 相对于参考时钟的传播延迟为：

```text
t_B = 150 ns - 100 ns = 50 ns
```

主站写入从站B的 **System Time Delay** 寄存器：

```text
System Time Delay_B = 50 ns
```

表示：从站B 的 System Time 应该比参考时钟晚 50 ns，这样当主站"同时"读取两者时，实际物理时间才一致。

---

## 五、偏移补偿（Offset Compensation）

### 5.1 初始偏移测量

传播延迟测完后，主站开始读取各从站的 System Time：

```text
读取从站A System Time: T_A = 1000000000 ns
读取从站B System Time: T_B = 1000000100 ns
```

### 5.2 计算偏移

考虑传播延迟后，从站B 的实际时间应该等于：

```text
T_B_corrected = T_B - System Time Delay_B
              = 1000000100 - 50
              = 1000000050 ns
```

与参考时钟 A 的差值：

```text
Offset = T_A - T_B_corrected
       = 1000000000 - 1000000050
       = -50 ns
```

### 5.3 写入偏移

主站写入从站B的 **System Time Offset**：

```text
System Time Offset_B = Offset = -50 ns
```

写入后：

```text
从站B 的 System Time = 本地计数 + (-50 ns) = 与从站A一致
```

---

## 六、漂移补偿（Drift Compensation）

### 6.1 为什么需要漂移补偿？

晶振不可能完全相同。假设：

```text
从站A 晶振: 100.000 MHz
从站B 晶振: 100.0001 MHz   ← 快了 1 ppm
```

即使初始偏移为 0，1 秒后：

```text
从站B 比 从站A 快 1 μs
10 秒后快 10 μs
```

### 6.2 漂移测量

主站周期性读取从站时间：

```text
t=0s:  从站B - 从站A = 0 ns
t=1s:  从站B - 从站A = 1000 ns
t=2s:  从站B - 从站A = 2000 ns
```

漂移速率：

```text
drift = 2000 ns / 2 s = 1 ppm
```

### 6.3 漂移补偿

ESC 内部有 **Speed Counter**，通过调整本地时钟的计数速率来补偿漂移：

```text
Speed Counter Start = 原始值 × (1 - drift)
```

例如从站B 快了 1 ppm，就让它每计数 1000000 个 tick 少走 1 个 tick，实现长期同步。

SOEM 的 `ecx_configdc()` 会周期性执行这个测量-补偿循环。

---

## 七、Sync0 / Sync1 信号生成

### 7.1 原理

所有从站的 System Time 同步后，ESC 可以产生基于 System Time 的硬件脉冲。

配置：

```text
Sync0 Start Time  = 1000000000 ns
Sync0 Cycle Time  = 1000000 ns   (1 ms)
```

Sync0 触发时刻：

```text
1000000000 ns
1000001000 ns
1000002000 ns
1000003000 ns
...
```

### 7.2 多从站同步效果

由于所有从站的 System Time 已经同步到 < 100 ns，它们产生的 Sync0 脉冲也会几乎同时：

```text
从站A Sync0: ━┓    ━┓    ━┓
从站B Sync0: ━┓    ━┓    ━┓   ← 与 A 几乎重合
从站C Sync0: ━┓    ━┓    ━┓   ← 与 A 几乎重合
```

典型同步精度：**50 ns ~ 100 ns**。

### 7.3 从站应用响应

从站 SSC 代码中通常有 Sync0 中断处理函数：

```c
void ECAT_Sync0_Isr(void)
{
    // 在每个 Sync0 中断中执行控制算法
    APPL_Application();
}
```

这样所有从站的控制算法都在同一时刻执行。

---

## 八、DC 同步完整流程图

```text
[INIT]
   │
   ▼
[拓扑扫描] ──→ 知道从站数量、顺序、配置地址
   │
   ▼
[选择参考时钟] ──→ 通常选择第一个从站
   │
   ▼
[传播延迟测量] ──→ 计算每个从站到参考时钟的延迟
   │                 写入 System Time Delay
   ▼
[初始偏移测量] ──→ 读取各从站 System Time
   │                 计算 Offset
   ▼
[写入 Offset] ──→ 写入 System Time Offset
   │
   ▼
[漂移补偿] ──→ 周期性测量漂移
   │            调整 Speed Counter
   ▼
[配置 Sync0] ──→ 设置周期和起始时间
   │              从站按 Sync0 同步执行
   ▼
[OP]
```

---

## 九、SOEM 中的 DC 实现

SOEM 的 `ecx_configdc()` 自动完成上述大部分工作：

```c
/* 自动选择参考时钟，测量延迟，写入 offset */
ecx_configdc(&ctx);

/* 配置 Sync0 周期 1ms */
ecx_dcsync0(&ctx, 1, TRUE, 1000000, 0);
```

主循环中读取 DC 时间：

```c
printf("DCtime = %llu ns\n", ctx.DCtime);
```

`ctx.DCtime` 是参考时钟的 System Time，精度 1 ns。

---

## 十、常见误区

| 误区 | 正确理解 |
|------|----------|
| DC 同步后所有从站晶振频率相同 | 否，晶振频率仍然不同，只是通过补偿让 System Time 一致 |
| DC 只需要配置一次 | 否，漂移补偿需要周期性进行 |
| 主站周期由 DC 决定 | 否，DC 提供参考时间，主站周期仍由主站控制，但建议与 Sync0 对齐 |
| 单从站也能看到分布式同步 | 否，多从站才能体现分布式同步优势 |

---

## 十一、实验建议

理解了原理后，建议按这个顺序做实验：

1. **单站读取 DC 时间**（验证 `ctx.DCtime`）
2. **对比系统时间抖动 vs DC 时间抖动**
3. **配置 Sync0 并观察 LED 翻转稳定性**
4. **多从站时用示波器测量 Sync0 同步精度**
