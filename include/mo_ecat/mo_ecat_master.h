/*
 * mo_ecat_master.h - 主站生命周期与调度接口
 *
 * 本头文件只包含主站最核心的生命周期函数。
 * 状态查询、节点信息、周期数据访问分别位于独立头文件。
 */

#ifndef MO_ECAT_MASTER_H
#define MO_ECAT_MASTER_H

#include "mo_ecat/mo_ecat_common.h"
#include "mo_ecat/mo_ecat_master_config.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_cyclic.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

/**
 * enum mo_ecat_master_cmd - 主站状态机请求命令
 * @MO_ECAT_MASTER_CMD_NONE:       无命令
 * @MO_ECAT_MASTER_CMD_SCAN:       扫描总线
 * @MO_ECAT_MASTER_CMD_CONFIGURE:  配置 DC 并建立周期数据映射
 * @MO_ECAT_MASTER_CMD_ACTIVATE:   激活周期运行
 * @MO_ECAT_MASTER_CMD_DEACTIVATE: 停止周期运行
 * @MO_ECAT_MASTER_CMD_RESET:      复位到空闲
 */
enum mo_ecat_master_cmd {
    MO_ECAT_MASTER_CMD_NONE,
    MO_ECAT_MASTER_CMD_SCAN,
    MO_ECAT_MASTER_CMD_CONFIGURE,
    MO_ECAT_MASTER_CMD_ACTIVATE,
    MO_ECAT_MASTER_CMD_DEACTIVATE,
    MO_ECAT_MASTER_CMD_RESET
};

/**
 * enum mo_ecat_master_error - 主站最近一次进入 FAULT 的原因
 * @MO_ECAT_MASTER_ERROR_NONE:                         无错误
 * @MO_ECAT_MASTER_ERROR_DISCOVER_FAILED:              扫描总线失败
 * @MO_ECAT_MASTER_ERROR_READ_CYCLIC_DESCRIPTION_FAILED: 读取周期数据描述失败
 * @MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED:          配置 DC 失败
 * @MO_ECAT_MASTER_ERROR_CONFIGURE_CYCLIC_MAPPING_FAILED: 建立周期数据映射失败
 * @MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED:              激活周期运行失败
 * @MO_ECAT_MASTER_ERROR_BUS_FAULT:                    总线故障
 */
enum mo_ecat_master_error {
    MO_ECAT_MASTER_ERROR_NONE,
    MO_ECAT_MASTER_ERROR_DISCOVER_FAILED,
    MO_ECAT_MASTER_ERROR_READ_CYCLIC_DESCRIPTION_FAILED,
    MO_ECAT_MASTER_ERROR_CONFIGURE_DC_FAILED,
    MO_ECAT_MASTER_ERROR_CONFIGURE_CYCLIC_MAPPING_FAILED,
    MO_ECAT_MASTER_ERROR_ACTIVATE_FAILED,
    MO_ECAT_MASTER_ERROR_BUS_FAULT,
};

/**
 * mo_ecat_master_create - 创建并初始化单主站对象
 * @config:    主站配置指针，生命周期由调用者保证
 * @callback:  RUNNING 状态中的周期控制回调，允许为 NULL
 * @user_data: 回调的私有数据
 *
 * 为单主站对象分配内存，并完成字段初始化、配置指针绑定和周期回调注册。
 * 单主站场景下，重复调用会失败并返回 NULL。
 *
 * Return: 成功返回主站对象指针，失败返回 NULL
 */
struct mo_ecat_master *mo_ecat_master_create(
    const struct mo_ecat_master_config *config,
    mo_ecat_cyclic_callback callback,
    void *user_data);

/**
 * mo_ecat_master_destroy - 销毁主站对象
 * @master: 主站对象
 *
 * 释放后端资源、关闭后端，并释放由 mo_ecat_master_create() 分配的内存。
 */
void mo_ecat_master_destroy(struct mo_ecat_master *master);

/**
 * mo_ecat_master_dispatch - 调度主站状态机
 * @master: 主站对象
 *
 * 必须由唯一的周期调度线程以固定周期调用。该线程独占主站运行态、后端与
 * 周期数据区域；RUNNING 状态中的过程数据收发和控制回调也由该线程执行。
 */
void mo_ecat_master_dispatch(struct mo_ecat_master *master);

/**
 * mo_ecat_master_write_cmd - 写入主站状态机命令
 * @master: 主站对象
 * @cmd:    请求命令；不允许写入 MO_ECAT_MASTER_CMD_NONE
 *
 * 通过原子命令槽写入一条状态机请求，不直接执行后端动作。新命令可以覆盖
 * 尚未消费的旧命令；核心库不保证命令可靠投递，也不保存异步执行结果。
 *
 * Return: 0 表示命令已写入，非 0 表示参数无效
 */
int mo_ecat_master_write_cmd(struct mo_ecat_master *master,
                             enum mo_ecat_master_cmd cmd);

/**
 * mo_ecat_master_get_error_code - 获取主站最近一次进入 FAULT 的原因
 * @master: 主站对象
 *
 * 该返回值只在状态为 FAULT 时具有诊断意义；命令未被接受时不会更新。
 *
 * Return: 错误码
 */
enum mo_ecat_master_error mo_ecat_master_get_error_code(
    const struct mo_ecat_master *master);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_MASTER_H */
