#ifndef HUMANOID_NAMES_H
#define HUMANOID_NAMES_H

/**
 * @brief 人形机器人功能组标识
 *
 * 该枚举属于应用层，不应泄漏到 EtherCAT 核心库。
 */
enum humanoid_group_id {
	HUMANOID_GROUP_TORSO,     /**< 躯干 */
	HUMANOID_GROUP_LEFT_ARM,  /**< 左臂 */
	HUMANOID_GROUP_RIGHT_ARM, /**< 右臂 */
	HUMANOID_GROUP_LEFT_LEG,  /**< 左腿 */
	HUMANOID_GROUP_RIGHT_LEG, /**< 右腿 */
	HUMANOID_GROUP_HEAD,      /**< 头部 */
	HUMANOID_GROUP_COUNT      /**< 功能组数量 */
};

#endif /* HUMANOID_NAMES_H */

