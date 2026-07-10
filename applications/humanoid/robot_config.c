/**
 * @file robot_config.c
 * @brief 人形机器人配置文件解析预留实现
 *
 * 后续在这里将 YAML/JSON 等配置文件转换为 robot_config。
 */

#include "robot_config.h"

static const struct robot_joint_config s_default_joints[] = {
	{
		.joint_name = "left_shoulder_pitch",
		.group = HUMANOID_GROUP_LEFT_ARM,
		.match =
			{
				.alias = 0,
				.position = 1,
				.vendor_id = 0x00000766,
				.product_code = 0x00002001,
			},
	},
};

static const struct robot_config s_default_config = {
	.name = "humanoid",
	.joints = s_default_joints,
	.joint_count = sizeof(s_default_joints) / sizeof(s_default_joints[0]),
};

const struct robot_config *robot_default_config(void)
{
	return &s_default_config;
}
