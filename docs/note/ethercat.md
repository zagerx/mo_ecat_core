# EtherCAT

EtherCAT（Ethernet for Control Automation Technology，用于控制自动化技术的以太网）是一种实时工业以太网技术，最初由Beckhoff Automation开发。EtherCAT协议已在IEC标准**IEC 61158**中公开披露，适用于自动化技术、测试测量等众多应用中的硬实时和软实时需求。

> **来源**: EtherCAT Technology Group (ETG) 官网技术介绍页 - https://www.ethercat.org/en/technology.html

---

## EtherCAT物理层

EtherCAT物理层基于标准以太网技术，主要采用 **100BASE-TX**（IEEE 802.3）物理层规范，同时支持光纤扩展、EtherCAT P以及Single Pair Ethernet（SPE）等物理层实现。ETG官方明确表示：*"The complete range of Ethernet wiring is also available for EtherCAT."*（完整的以太网布线范围均可用于EtherCAT。）

> **来源**: EtherCAT Technology Group (ETG) 官网 - https://www.ethercat.org/en/technology.html

## 协议标准对比

| 对比项               | **100BASE-TX**               | **10BASE-T**   | **10BASE-T1L**                             | **10BASE-T1S**                             | **100BASE-T1** |
| ----------------- | ---------------------------- | -------------- | ------------------------------------------ | ------------------------------------------ | -------------- |
| **标准**            | IEEE 802.3u                  | IEEE 802.3i    | IEEE 802.3cg                               | IEEE 802.3cg                               | IEEE 802.3bw   |
| **速率**            | 100 Mbps                     | 10 Mbps        | 10 Mbps                                    | 10 Mbps                                    | 100 Mbps       |
| **线对数**           | 2 对（4 根）                     | 2 对（4 根）       | 1 对（2 根）                                   | 1 对（2 根）                                   | 1 对（2 根）       |
| **编码方式**          | 4B/5B + MLT-3                | Manchester     | PAM3（4B3T）                                 | PAM3（4B3T）                                 | PAM3           |
| **连接器**           | RJ45                         | RJ45           | M8 / M12                                   | M8 / M12                                   | 汽车专用（H-MTD 等）  |
| **最大距离**          | 100 米                        | 100 米          | 1000 米                                     | 25 米（多点）                                   | 15 米           |
| **拓扑**            | 点对点 / 线型 / 星型                | 星型（需 HUB）      | 点对点                                        | 多点总线（最多 8 节点）                              | 点对点            |
| **供电**            | 无（需单独供电或 EtherCAT P）         | 无              | PoDL（数据线供电）                                | PoDL                                       | 无              |
| **全双工/半双工**       | 全双工 / 半双工                    | 半双工            | 全双工                                        | 半双工（PLCA 仲裁）                               | 全双工            |
| **应用场景**          | 工业以太网、EtherCAT 主/从站          | 早期办公网络（已淘汰）    | 工业传感器、执行器、长距离分布式 IO                        | 工业传感器总线、短距多点                               | 汽车 ADAS、摄像头、雷达 |
| **与 EtherCAT 关系** | **ETG 官方规范定义的标准物理层**，一致性测试覆盖 | 非 EtherCAT 物理层 | **物理层兼容，EtherCAT 协议可运行其上**，但 ETG 未发布官方标准增补 | **物理层兼容，EtherCAT 协议可运行其上**，但 ETG 未发布官方标准增补 | 非 EtherCAT 物理层 |
| **标准状态**          | **官方标准**                     | 传统以太网（已淘汰）     | **IEEE 标准物理层**，EtherCAT 生态**技术可行/产业推进中**   | **IEEE 标准物理层**，EtherCAT 生态**技术可行/产业推进中**   | 汽车以太网标准        |
---
## 10BASE-T1L / 10BASE-T1S PHY 芯片对比

| 标准         | 芯片型号       | 厂商                | 是否有 MAC                       | 接口                          | 数据手册/产品页链接                                                                                             |
| ------------ | -------------- | ------------------- | -------------------------------- | ----------------------------- | -------------------------------------------------------------------------------------------------------------- |
| 10BASE-T1L   | ADIN1100       | Analog Devices      | 否（纯 PHY）                     | MII, RMII, RGMII              | [ADI 官方产品页](https://www.analog.com/en/products/adin1100.html)                                              |
| 10BASE-T1L   | ADIN1110       | Analog Devices      | 是（MAC-PHY）                    | SPI, MII                      | [ADI 官方产品页](https://www.analog.com/en/products/adin1110.html)                                              |
| 10BASE-T1L   | ADIN1111       | Analog Devices      | 是（MAC-PHY）                    | SPI (25 MHz), MII             | [ADI 官方产品页](https://www.analog.com/en/products/adin1111.html)                                              |
| 10BASE-T1L   | ADIN2111       | Analog Devices      | 是（集成 MAC + 2×PHY 交换机）      | SPI                           | [ADI 官方产品页](https://www.analog.com/en/products/adin2111.html)                                              |
| 10BASE-T1L   | DP83TD510E     | Texas Instruments   | 否（纯 PHY）                     | MII, RMII, RGMII, 低功耗 RMII | [TI 官方产品页](https://www.ti.com/product/DP83TD510E)                                                          |
| 10BASE-T1S   | LAN8670        | Microchip           | 否（纯 PHY）                     | MII, SC-MII, RMII             | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8670)                                        |
| 10BASE-T1S   | LAN8671        | Microchip           | 否（纯 PHY）                     | RMII                          | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8671)                                        |
| 10BASE-T1S   | LAN8672        | Microchip           | 否（纯 PHY）                     | MII                           | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8672)                                        |
| 10BASE-T1S   | NCN26010       | onsemi              | 是（集成 MAC + PLCA RS + PHY）    | Open Alliance SPI             | [onsemi 官方产品页](https://www.onsemi.com/products/interfaces/transceivers/ethernet-transceivers/ncn26010)   |

---

| 标准              | 芯片型号              | 厂商                | 是否有 MAC  | 接口                      | 关键特性                                                      | 数据手册/产品页链接                                                                                    |
| --------------- | ----------------- | ----------------- | -------- | ----------------------- | --------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| 100BASE-T1      | **DP83TC812R-Q1** | Texas Instruments | 否（纯 PHY） | MII, RMII, RGMII        | TC-10 睡眠/唤醒，功能安全 capable，集成 LPF，与 DP83TG720 引脚兼容          | [TI 官方产品页](https://www.ti.com/product/DP83TC812R-Q1)                                          |
| 100BASE-T1      | **DP83TC812S-Q1** | Texas Instruments | 否（纯 PHY） | RGMII, SGMII            | TC-10，功能安全 capable，SGMII 版本                               | [TI 官方产品页](https://www.ti.com/product/DP83TC812S-Q1)                                          |
| 100BASE-T1      | **DP83TC813R-Q1** | Texas Instruments | 否（纯 PHY） | RGMII, SGMII            | 低功耗、小封装 (3.5×3.5 mm)，适合空间受限应用                             | [TI 官方产品页](https://www.ti.com/product/DP83TC813R-Q1)                                          |
| 100BASE-T1      | **DP83TC814R-Q1** | Texas Instruments | 否（纯 PHY） | MII, RMII, RGMII        | 低功耗，无 TC-10，功能安全 capable                                  | [TI 官方产品页](https://www.ti.com/product/DP83TC814R-Q1)                                          |
| 100BASE-T1      | **TJA1100**       | NXP               | 否（纯 PHY） | MII, RMII               | ASIL-A，OPEN Alliance TC-10 唤醒/睡眠，15m UTP                  | [NXP 官方产品页](https://www.nxp.com/products/TJA1100)                                             |
| 100BASE-T1      | **TJA1101**       | NXP               | 否（纯 PHY） | MII, RMII               | ASIL-A，成本优化，支持反向 MII，单 3.3V 供电                            | [NXP 官方文档](https://www.nxp.com/docs/en/data-sheet/TJA1101.pdf)                                |
| 100BASE-T1      | **TJA1101B**      | NXP               | 否（纯 PHY） | MII, RMII               | TJA1101 改进版，EMC 优化，ASIL-A，符合 OPEN Alliance EMC Spec 2.0   | [NXP 官方文档](https://www.nxp.com/docs/en/data-sheet/TJA1101B.pdf)                               |
| 100BASE-T1      | **TJA1103**       | NXP               | 否（纯 PHY） | MII, RMII, RGMII, SGMII | 第三代，ASIL B，IEEE 1588v2/802.1AS 时间戳，与 TJA1120 引脚兼容         | [NXP 官方产品页](https://www.nxp.com/products/TJA1103)                                             |
| 100BASE-T1      | **LAN8770**       | Microchip         | 否（纯 PHY） | MII, RMII, SC-MII       | TC10，ASIL B，电缆诊断，低功耗                                      | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8770)                            |
| 100/1000BASE-T1 | **LAN8870**       | Microchip         | 否（纯 PHY） | RGMII, SGMII            | 双速 100M/1G，TSN 就绪 (IEEE 802.1AS/1588)，TC10，ASIL B，15µA 睡眠 | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8870)                            |
| 100/1000BASE-T1 | **LAN8871**       | Microchip         | 否（纯 PHY） | RGMII                   | 双速 100M/1G，TSN 就绪，TC10，ASIL B                             | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8871)                            |
| 100/1000BASE-T1 | **LAN8872**       | Microchip         | 否（纯 PHY） | SGMII                   | 双速 100M/1G，TSN 就绪，TC10，ASIL B                             | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8872)                            |
| 100BASE-T1      | **LAN8781**       | Microchip         | 否（纯 PHY） | RGMII                   | MACsec 硬件加密，TC10，ASIL B，AEC-Q100 Grade 1                  | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8781)                            |
| 100BASE-T1      | **LAN8782**       | Microchip         | 否（纯 PHY） | SGMII                   | MACsec 硬件加密，TC10，ASIL B，AEC-Q100 Grade 1                  | [Microchip 官方产品页](https://www.microchip.com/en-us/product/LAN8782)                            |
| 100/1000BASE-T1 | **88Q2110**       | Marvell           | 否（纯 PHY） | RGMII                   | 双速 100M/1G，集成 MDI 端接电阻，支持 PTP 时间戳，40-pin QFN              | [Marvell 官方文档](https://www.marvell.com/products/transceivers.html)                            |
| 100/1000BASE-T1 | **88Q2112**       | Marvell           | 否（纯 PHY） | RGMII, SGMII            | 双速 100M/1G，集成 MDI 端接电阻，支持 PTP，48-pin QFN                  | [Marvell 官方文档](https://www.marvell.com/products/transceivers.html)                            |
| 100BASE-T1      | **BCM89811**      | Broadcom          | 否（纯 PHY） | MII, RMII, RGMII, SGMII | BroadR-Reach 技术继承者，支持 IEEE 1588，多种封装可选                    | [Broadcom 官方产品页](https://www.broadcom.com/products/ethernet-connectivity/automotive-ethernet) |
| 100BASE-T1      | **BCM89820**      | Broadcom          | 否（纯 PHY） | MII, RMII, RGMII, SGMII | 低成本版本，支持多种 MAC 接口                                         | [Broadcom 官方产品页](https://www.broadcom.com/products/ethernet-connectivity/automotive-ethernet) |
| 100BASE-T1      | **RTL9000A/B/C**  | Realtek           | 否（纯 PHY） | MII, RMII, RGMII, SGMII | 支持 100BASE-T1，TC10，低功耗                                    | [Realtek 官方产品页](https://www.realtek.com/en/products/communications-network-ics/item/rtl9000a) |
| 100/1000BASE-T1 | **RTL9010**       | Realtek           | 否（纯 PHY） | MII, RMII, RGMII, SGMII | 支持 100M/1G，TC10，内置开关稳压器，AEC-Q100 Grade 1                  | [Realtek 官方产品页](https://www.realtek.com/en/products/communications-network-ics/item/rtl9010)  |
