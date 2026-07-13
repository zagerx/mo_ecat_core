#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 机器人功能组标识
 *
 * 该枚举属于应用层，不应泄漏到 EtherCAT 核心库。
 * 后续由 YAML 配置文件统一管理。
 */
enum robot_group_id {
	ROBOT_GROUP_TORSO,     /**< 躯干 */
	ROBOT_GROUP_LEFT_ARM,  /**< 左臂 */
	ROBOT_GROUP_RIGHT_ARM, /**< 右臂 */
	ROBOT_GROUP_LEFT_LEG,  /**< 左腿 */
	ROBOT_GROUP_RIGHT_LEG, /**< 右腿 */
	ROBOT_GROUP_HEAD,      /**< 头部 */
	ROBOT_GROUP_COUNT      /**< 功能组数量 */
};

/**
 * @brief 机器人关节数量上限
 *
 * 运行时关节数量不能超过该值；超过时 robot_build() 会失败。
 * 后续由 YAML 配置文件统一管理。
 */
#define MO_ROBOT_MAX_JOINTS 32

/**
 * @brief 机器人功能组数量上限
 */
#define MO_ROBOT_MAX_GROUPS ROBOT_GROUP_COUNT

/**
 * @brief 从站稳定身份
 *
 * 不使用运行期 slave_index，避免总线拓扑变化后配置绑定到错误从站。
 */
struct robot_slave_identity {
	uint16_t alias;        /**< EtherCAT alias 地址 */
	uint16_t position;     /**< 物理总线位置 */
	uint32_t vendor_id;    /**< 厂商 ID */
	uint32_t product_code; /**< 产品码 */
};

/**
 * @brief 单个机器人关节的配置描述
 */
struct robot_joint_config {
	const char *joint_name;               /**< 关节逻辑名称 */
	enum robot_group_id group;            /**< 所属机器人功能组 */
	struct robot_slave_identity identity; /**< 对应 EtherCAT 从站身份 */
};

/**
 * @brief 机器人配置
 *
 * 后续由 YAML/JSON 等配置文件解析器填充。
 */
struct robot_config {
	const char *name;                        /**< 机器人实例名称 */
	const struct robot_joint_config *joints; /**< 关节配置数组 */
	size_t joint_count;                      /**< 关节配置数量 */
};

/**
 * @brief 获取用于当前示例的静态默认配置
 *
 * 后续接入配置文件解析器后，可由解析结果替代该默认配置。
 */
const struct robot_config *robot_default_config(void);

#endif /* ROBOT_CONFIG_H */
