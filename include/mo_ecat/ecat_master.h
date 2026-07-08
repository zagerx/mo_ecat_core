#ifndef MO_ECAT_ECAT_MASTER_H
#define MO_ECAT_ECAT_MASTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EC_MASTER_MAX_NAME_LEN 80

/** @brief 主站应用层状态 */
enum ec_master_state {
    EC_MASTER_STATE_INIT = 0,       /**< 初始：尚未扫描总线 */
    EC_MASTER_STATE_READY,          /**< 准备：已扫描/映射，等待启动 */
    EC_MASTER_STATE_RUNNING,        /**< 运行：周期过程数据运行中 */
    EC_MASTER_STATE_FAULT,          /**< 故障 */
    EC_MASTER_STATE_CONTROLLED,     /**< 受控：被外部控制器接管 */
    EC_MASTER_STATE_COUNT
};

/** @brief 从站信息（对外只暴露必要字段） */
struct ec_slave_info {
    uint16_t position;
    uint16_t alias;
    uint32_t vendor_id;
    uint32_t product_code;
    uint16_t state;
    char     name[EC_MASTER_MAX_NAME_LEN + 1];
};

/** @brief EtherCAT 主站对象（不透明类型） */
struct ec_master;

/**
 * @brief 创建主站对象
 * @param ifname 网卡接口名，如 "eth0"
 * @return 成功返回主站指针，失败返回 NULL
 */
struct ec_master *ec_master_create(const char *ifname);

/**
 * @brief 销毁主站对象
 * @param master 主站对象
 */
void ec_master_destroy(struct ec_master *master);

/**
 * @brief 启动主站状态机（触发 init 状态）
 * @param master 主站对象
 * @return 0 表示启动成功（异步），<0 表示失败
 */
int ec_master_start(struct ec_master *master);

/**
 * @brief 停止主站周期运行
 * @param master 主站对象
 */
void ec_master_stop(struct ec_master *master);

/**
 * @brief 调度一次主站状态机
 * @details 应在固定周期调用，例如 1ms。
 *          内部会处理状态切换、SOEM 收发等。
 * @param master 主站对象
 */
void ec_master_run_cycle(struct ec_master *master);

/**
 * @brief 获取当前主站状态
 * @param master 主站对象
 * @return 当前状态 ID
 */
enum ec_master_state ec_master_get_state(const struct ec_master *master);

/**
 * @brief 获取扫描到的从站数量
 * @param master 主站对象
 * @return 从站数量（>=0），<0 表示失败
 */
int ec_master_get_slave_count(const struct ec_master *master);

/**
 * @brief 获取指定从站信息
 * @param master   主站对象
 * @param position 从站位置（SOEM 从 1 开始）
 * @param info     输出缓冲区
 * @return 0 成功，<0 失败
 */
int ec_master_get_slave_info(const struct ec_master *master, uint16_t position,
                             struct ec_slave_info *info);

/**
 * @brief 获取当前 DC 参考时间
 * @param master 主站对象
 * @return DC 时间（未启用 DC 时返回 0）
 */
int64_t ec_master_get_dc_time(const struct ec_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_ECAT_MASTER_H */
