#ifndef HUMANOID_TOPOLOGY_H
#define HUMANOID_TOPOLOGY_H

#include <stddef.h>

#include "humanoid_config.h"
#include "mo_ecat/mo_ecat_master.h"

/**
 * @brief 已与扫描从站匹配的运行时关节
 */
struct humanoid_joint {
	const char *name;                     /**< 关节逻辑名称 */
	enum humanoid_group_id group;         /**< 所属功能组 */
	size_t slave_index;                   /**< 本次扫描对应的从站下标 */
	struct humanoid_slave_match identity; /**< 从站稳定身份 */
};

/**
 * @brief 一个机器人功能组的关节范围
 */
struct humanoid_group {
	enum humanoid_group_id id; /**< 功能组标识 */
	const char *name;          /**< 功能组名称 */
	size_t joint_start;        /**< joints 数组中的起始下标 */
	size_t joint_count;        /**< 功能组包含的关节数量 */
};

/**
 * @brief 人形机器人运行时逻辑拓扑
 *
 * 主站 reset 或重新发现总线后，该对象必须释放并重新构建。
 */
struct humanoid_topology {
	struct humanoid_joint *joints; /**< 已匹配关节数组 */
	size_t joint_count;            /**< 已匹配关节数量 */
	struct humanoid_group *groups; /**< 功能组数组 */
	size_t group_count;            /**< 功能组数量 */
};

/**
 * @brief 根据主站扫描结果和应用配置构建逻辑拓扑
 *
 * 后续实现应校验从站缺失、重复匹配和型号不一致等错误。
 */
int humanoid_topology_build(struct mo_ecat_master *master,
			    const struct humanoid_config *config,
			    struct humanoid_topology *topology);

/**
 * @brief 释放 humanoid_topology_build() 分配的运行时资源
 */
void humanoid_topology_release(struct humanoid_topology *topology);

#endif /* HUMANOID_TOPOLOGY_H */

