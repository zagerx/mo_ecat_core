#ifndef MO_ECAT_TOPOLOGY_H
#define MO_ECAT_TOPOLOGY_H

#include "mo_ecat/mo_ecat_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mo_ecat_master;

enum mo_ecat_node_al_state {
    MO_ECAT_NODE_AL_STATE_INIT,
    MO_ECAT_NODE_AL_STATE_PRE_OP,
    MO_ECAT_NODE_AL_STATE_SAFE_OP,
    MO_ECAT_NODE_AL_STATE_OP,
    MO_ECAT_NODE_AL_STATE_BOOTSTRAP,
    MO_ECAT_NODE_AL_STATE_UNKNOWN
};

struct mo_ecat_node_state {
    enum mo_ecat_node_al_state al_state;
    int error;
    uint16_t al_status_code;
    int online;
    int operational;
};

/**
 * @brief 从站信息的公开只读视图
 *
 * 由 mo_ecat_master_get_node_info() 复制返回，避免向应用层暴露内部数组指针。
 */
struct mo_ecat_node_info {
    uint16_t position;
    uint16_t alias;
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    char name[MO_ECAT_MAX_NAME_LEN + 1];
    int has_dc;
    struct mo_ecat_node_state state; /**< 节点运行时诊断状态 */
};

/**
 * @file mo_ecat_topology.h
 * @brief 总线拓扑节点信息接口
 */

/**
 * @brief 获取已发现节点数量
 */
size_t mo_ecat_master_get_node_count(const struct mo_ecat_master *master);

/**
 * @brief 获取指定从站信息的只读副本
 *
 * 调用方提供 info 缓冲区，核心复制数据。返回的指针不指向内部数组，
 * 因此在调用返回后即使发生状态迁移也不会悬空。调用方不得在主站扫描、RESET 或
 * 重新配置期间并发调用本函数。
 *
 * @return 0 成功，非 0 失败
 */
int mo_ecat_master_get_node_info(const struct mo_ecat_master *master,
                                 size_t index, struct mo_ecat_node_info *info);

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_TOPOLOGY_H */
