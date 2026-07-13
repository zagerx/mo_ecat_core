#ifndef ROBOT_H
#define ROBOT_H

#include <stddef.h>

#include "robot_config.h"
#include "mo_ecat/mo_ecat_master.h"

/**
 * @brief 已与扫描从站匹配的运行时关节
 */
struct robot_joint {
	const char *name;                     /**< 关节逻辑名称 */
	enum robot_group_id group;            /**< 所属功能组 */
	size_t slave_index;                   /**< 本次扫描对应的从站下标 */
	struct robot_slave_identity identity; /**< 从站稳定身份 */
};

/**
 * @brief 一个机器人功能组的关节范围
 */
struct robot_group {
	enum robot_group_id id; /**< 功能组标识 */
	const char *name;       /**< 功能组名称 */
	size_t joint_start;     /**< joints 数组中的起始下标 */
	size_t joint_count;     /**< 功能组包含的关节数量 */
};

/**
 * @brief 机器人运行时对象
 *
 * 主站 reset 或重新发现总线后，该对象必须释放并重新构建。
 *
 * 关节和功能组使用固定大小数组，避免运行期动态分配。
 */
struct robot {
	const char *name;              /**< 机器人实例名称 */
	struct mo_ecat_master *master; /**< 关联主站，不拥有其生命周期 */
	struct robot_joint joints[MO_ROBOT_MAX_JOINTS]; /**< 已匹配关节数组 */
	size_t joint_count;                             /**< 已匹配关节数量 */
	struct robot_group groups[MO_ROBOT_MAX_GROUPS]; /**< 功能组数组 */
	size_t group_count;                             /**< 功能组数量 */
};

/**
 * @brief 根据主站扫描结果和应用配置构建逻辑拓扑
 *
 * 后续实现应校验从站缺失、重复匹配和型号不一致等错误。
 */
int robot_build(struct robot *robot, struct mo_ecat_master *master,
		const struct robot_config *config);

/**
 * @brief 释放 robot_build() 分配的运行时资源
 */
void robot_release(struct robot *robot);

#endif /* ROBOT_H */
