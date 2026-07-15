/**
 * @file robot_config.c
 * @brief CLI 默认机器人配置
 *
 * 机器人应用库本身不持有默认配置；其他应用可链接自己的配置实现。
 */

#include "robot_config.h"

static const struct robot_joint_config s_default_joints[] = {
	{
		.joint_name = "left_shoulder_pitch",
		.group = ROBOT_GROUP_LEFT_ARM,
		.identity =
			{
				.position = 1,
				.vendor_id = 0x00000766,
				.product_code = 0x00002001,
			},
	},
};

static const struct robot_config s_default_config = {
	.name = "robot",
	.joints = s_default_joints,
	.joint_count = sizeof(s_default_joints) / sizeof(s_default_joints[0]),
};

const struct robot_config *robot_default_config(void)
{
	return &s_default_config;
}
