/*
 * mo_ecat_sync0.h - 从站 DC Sync0 周期同步配置接口
 *
 * Sync0 是 DC（分布式时钟）的周期同步输出：主站在总线配置阶段对目标
 * 从站调用一次 ecx_dcsync0() 激活，从站在每个 Sync0 周期触发硬件中断
 * 与应用调度。激活后主站运行阶段不重复调用配置函数。
 *
 * 注意：Sync0 周期/相位在配置阶段一次性写入从站 ESC 寄存器，运行期
 * 重复调用会导致 Sync0 关闭后重建，产生脉冲中断和相位跳变。
 */

#ifndef MO_ECAT_SYNC0_H
#define MO_ECAT_SYNC0_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct mo_ecat_sync0_status - 单个从站 Sync0 的读回状态
 * @active: 从站 Sync0 输出是否激活（读自 0x0981 激活位）
 * @cycle_time_ns: 读回的 Sync0 周期（ns，0x09A0）
 * @start_time_ns: 读回的 Sync0 起始时间（ns，0x0990）
 * @dc_time_ns: 从站当前 DC System Time（ns，0x0910 采样）
 */
struct mo_ecat_sync0_status {
	int active;
	uint32_t cycle_time_ns;
	uint32_t start_time_ns;
	uint64_t dc_time_ns;
};

/**
 * mo_ecat_master_sync0_configure - 激活或关闭目标从站的 Sync0 输出
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @enable: 非 0 激活，0 关闭
 * @cycle_time_ns: Sync0 周期（ns），须不小于从站声明的 MIN_PD_CYCLE_TIME
 * @shift_time_ns: Sync0 相位偏移（ns），相对参考时钟同步脉冲
 *
 * 只在总线配置阶段（CONFIGURE_DC 之后、ACTIVATE 之前）调用。运行期
 * 重复调用会先关闭当前 Sync0 再重建，导致脉冲中断，禁止使用。
 *
 * Return: 0 成功；负 errno
 */
int mo_ecat_master_sync0_configure(struct mo_ecat_master *master, size_t slave_index, int enable,
				   uint32_t cycle_time_ns, int32_t shift_time_ns);

/**
 * mo_ecat_master_sync0_status - 读回从站 Sync0 当前状态
 * @master: 主站对象指针
 * @slave_index: 目标从站下标（逻辑拓扑下标，0 起）
 * @status: 状态输出缓冲区
 *
 * 从 ESC 寄存器读回激活位、周期与起始时间，供诊断界面展示。
 *
 * Return: 0 成功；负 errno
 */
int mo_ecat_master_sync0_status(struct mo_ecat_master *master, size_t slave_index,
				struct mo_ecat_sync0_status *status);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_SYNC0_H */
