#ifndef HUMANOID_CONFIG_H
#define HUMANOID_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "humanoid_names.h"

/**
 * @brief 从站稳定身份匹配条件
 *
 * 不使用运行期 slave_index，避免总线拓扑变化后配置绑定到错误从站。
 */
struct humanoid_slave_match {
	uint16_t alias;        /**< EtherCAT alias 地址 */
	uint16_t position;     /**< 物理总线位置 */
	uint32_t vendor_id;    /**< 厂商 ID */
	uint32_t product_code; /**< 产品码 */
};

/**
 * @brief 单个机器人关节的配置描述
 */
struct humanoid_joint_config {
	const char *joint_name;            /**< 关节逻辑名称 */
	enum humanoid_group_id group;      /**< 所属机器人功能组 */
	struct humanoid_slave_match match; /**< 对应 EtherCAT 从站 */
};

/**
 * @brief 人形机器人配置
 *
 * 后续由 YAML/JSON 等配置文件解析器填充。
 */
struct humanoid_config {
	const struct humanoid_joint_config *joints; /**< 关节配置数组 */
	size_t joint_count;                         /**< 关节配置数量 */
};

/**
 * @brief 获取用于当前示例的静态默认配置
 *
 * 后续接入配置文件解析器后，可由解析结果替代该默认配置。
 */
const struct humanoid_config *humanoid_default_config(void);

#endif /* HUMANOID_CONFIG_H */
