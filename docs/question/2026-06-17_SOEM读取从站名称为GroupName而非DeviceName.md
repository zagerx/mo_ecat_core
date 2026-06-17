# SOEM 读取从站名称为 Group Name 而非 Device Name

## 问题描述

在使用 SOEM 主站扫描从站时，`ecat_scan` 输出的从站名称为 `"Servo Amplifiers"`，而期望显示的是 Device Name `"Joint1"`。

### 现象日志

```text
[2026-06-17 16:26:34.651] [INFO] [ec_controller.cpp:123] Slave[1] info:
  name               = "Servo Amplifiers"
  vendor_id          = 0x00000766
  product_id         = 0x00002000
  revision_id        = 0x00000200
  serial_id          = 0x00000000
  ...
```

对应的 ESI 文件 `docs/RA8 EtherCAT.xml` 中定义：

```xml
<Groups>
    <Group>
        <Type>Servo Amplifiers</Type>
        <Name>Servo Amplifiers</Name>
    </Group>
</Groups>
<Devices>
    <Device>
        <Type ProductCode="#x00002000" RevisionNo="#x00000200">Joint1</Type>
        <Name>Joint1</Name>
        <GroupType>Servo Amplifiers</GroupType>
    </Device>
</Devices>
```

## 环境信息

- 项目：`MoDriverPC_EtherCAT`
- SOEM 路径：`third_party/SOEM`
- 从站：Renesas RA8 EtherCAT 从站
- 网卡：`enp0s31f6`
- EEPROM 大小：2048 bytes

## 根本原因

### 1. SOEM 默认读取策略

SOEM 在 `third_party/SOEM/src/ec_config.c` 的 `ecx_config_init()` 函数中，读取从站名称时**固定读取 STRINGS Category 的第 1 个字符串**：

```c
/* SII strings section */
if (ecx_siifind(context, slave, ECT_SII_STRING) > 0)
{
   ecx_siistring(context, context->slavelist[slave].name, slave, 1);
}
```

### 2. SSC Tool 生成 EEPROM 的字符串顺序

SSC Tool 从 ESI 生成 EEPROM 时，字符串顺序为：

| 索引 | 内容 | 来源 |
|---|---|---|
| `string[1]` | `"Servo Amplifiers"` | `<Groups>/<Group>/<Name>` |
| `string[2]` | `"Joint1"` | `<Device>/<Name>` |
| `string[3]` | `"Synchron"` | DC 模式名称 |
| `string[4]` | `"SM-Synchron"` | DC 模式描述 |
| ... | ... | ... |

同时，GENERAL Category 中的 `NameIdx` 字段正确指向 `string[2]`，即 `"Joint1"`。

### 3. 冲突点

SOEM 没有按照 ETG.1000.6 规范读取 GENERAL Category 的 `NameIdx`，而是直接读取 `string[1]`，因此得到的是 Group Name `"Servo Amplifiers"`。

从 EEPROM 实际内容验证：

```text
GENERAL category:
  GroupIdx  = 1 -> "Servo Amplifiers"
  ImgIdx    = 0 -> ""
  OrderIdx  = 2 -> "Joint1"
  NameIdx   = 2 -> "Joint1"
```

## 分析过程

1. **确认工具链**：SOEM 自带的 `eepromtool` 可以读取/写入 EEPROM，已编译在 `third_party/SOEM/build/samples/eepromtool/eepromtool`。

2. **读取 EEPROM 备份**：使用 `eepromtool` 从从站读取 EEPROM：
   ```bash
   sudo ./third_party/SOEM/build/samples/eepromtool/eepromtool enp0s31f6 1 -r backup_eeprom.bin
   ```

3. **解析 EEPROM**：编写 `parse_eeprom.py` 解析 STRINGS 和 GENERAL Category，确认：
   - `string[1] = "Servo Amplifiers"`
   - `string[2] = "Joint1"`
   - `GENERAL.NameIdx = 2`

4. **修正读取方式**：SOEM 应按 GENERAL Category 中的 `NameIdx` 字段读取设备名称。`ecx_siifind()` 返回的是 Category 头部 length 字段的起始地址，因此 GENERAL 数据区起始于 `ssigen + 2`，`NameIdx`（数据区偏移 0x03）对应地址为 `ssigen + 0x05`。

## 解决方案

修改 `third_party/SOEM/src/ec_config.c` 中读取从站名称的逻辑，使其按照 GENERAL Category 的 `NameIdx` 读取：

```c
/* SII strings section */
if (ecx_siifind(context, slave, ECT_SII_STRING) > 0)
{
   uint16 name_idx = 1;
   if (ssigen)
   {
      /* NameIdx is at data byte offset 0x03 in GENERAL category (ETG.1000.6 Table 21) */
      name_idx = ecx_siigetbyte(context, slave, ssigen + 0x05);
      if (name_idx == 0)
      {
         name_idx = 1;
      }
   }
   ecx_siistring(context, context->slavelist[slave].name, slave, name_idx);
}
```

### 编译

```bash
cd build
cmake --build . -- -j$(nproc)
```

## 验证结果

修改后重新运行 `ecat_scan`：

```text
[2026-06-17 17:06:50.891] [INFO] [ec_master.cpp:29] SOEM initialized on enp0s31f6
[2026-06-17 17:06:50.942] [INFO] [ec_master.cpp:54] 1 slave(s) found
[2026-06-17 17:06:50.942] [INFO] [ec_controller.cpp:123] Slave[1] info:
  name               = "Joint1"
  ...
```

从站名称已正确显示为 `"Joint1"`。

## 注意事项

1. **SOEM 子模块修改**：本次修改位于 `third_party/SOEM` 子模块中。如果后续更新或重新克隆 SOEM，该修改会丢失，需要重新应用。

2. **EEPROM 与 ESI 的对应关系**：
   - ESI 是源文件，EEPROM 是派生子集。
   - 修改从站配置后，必须重新生成 ESI → 生成 EEPROM 数据 → 烧录到从站。
   - 主站在线运行时只读取 EEPROM，不读取 ESI。

3. **EEPROM 烧录工具**：`third_party/SOEM/build/samples/eepromtool/eepromtool`
   - 读取：`sudo ./eepromtool ifname slave -r fname.bin`
   - 写入：`sudo ./eepromtool ifname slave -w fname.bin`
   - 需要 root 权限。

## 相关文件

- ESI 文件：`docs/RA8 EtherCAT.xml`
- EEPROM 解析脚本：`parse_eeprom.py`
- EEPROM 备份：`docs/backup_eeprom_20260617_170023.bin`
- SOEM 修改文件：`third_party/SOEM/src/ec_config.c`
- 参考文档：`docs/note/EEPROM.md`、`docs/note/ESI文件规则.md`
