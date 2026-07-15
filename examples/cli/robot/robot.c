/**
 * @file robot.c
 * @brief CLI 机器人运行时对象构建
 *
 * 后续在这里完成扫描从站与 robot_config 的匹配和校验。
 */

#include "robot.h"

#include <string.h>

#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_topology.h"

static const char *group_name(enum robot_group_id group)
{
	switch (group) {
	case ROBOT_GROUP_TORSO:
		return "torso";
	case ROBOT_GROUP_LEFT_ARM:
		return "left_arm";
	case ROBOT_GROUP_RIGHT_ARM:
		return "right_arm";
	case ROBOT_GROUP_LEFT_LEG:
		return "left_leg";
	case ROBOT_GROUP_RIGHT_LEG:
		return "right_leg";
	case ROBOT_GROUP_HEAD:
		return "head";
	default:
		return "unknown";
	}
}

static int node_matches(const struct mo_ecat_node_info *node,
			 const struct robot_slave_identity *identity)
{
	return node && identity && node->position == identity->position &&
	       node->vendor_id == identity->vendor_id &&
	       node->product_code == identity->product_code;
}

static int find_slave(struct mo_ecat_master *master, const struct robot_slave_identity *identity,
		      size_t *slave_index)
{
	size_t count;
	size_t found_index = 0;
	int found = 0;

	if (!master || !identity || !slave_index) {
		return -1;
	}

	count = mo_ecat_master_get_node_count(master);
	for (size_t i = 0; i < count; ++i) {
		struct mo_ecat_node_info node;

		if (mo_ecat_master_get_node_info(master, i, &node) < 0) {
			continue;
		}
		if (!node_matches(&node, identity)) {
			continue;
		}
		if (found) {
			return -1;
		}
		found = 1;
		found_index = i;
	}

	if (!found) {
		return -1;
	}

	*slave_index = found_index;
	return 0;
}

static int slave_is_assigned(const struct robot *robot, size_t joint_count, size_t slave_index)
{
	for (size_t i = 0; i < joint_count; ++i) {
		if (robot->joints[i].slave_index == slave_index) {
			return 1;
		}
	}

	return 0;
}

void robot_release(struct robot *robot)
{
	if (!robot) {
		return;
	}

	memset(robot, 0, sizeof(*robot));
}

int robot_build(struct robot *robot, struct mo_ecat_master *master,
		const struct robot_config *config)
{
	size_t joint_index = 0;
	size_t group_index = 0;

	{
		enum mo_ecat_master_state state = mo_ecat_master_get_state(master);
		if (!robot || !master || !config || !config->name || !config->joints ||
		    config->joint_count == 0 ||
		    state != MO_ECAT_MASTER_STATE_IDLE) {
			return -1;
		}
	}
	for (size_t i = 0; i < config->joint_count; ++i) {
		if (!config->joints[i].joint_name || config->joints[i].group >= ROBOT_GROUP_COUNT) {
			return -1;
		}
	}

	robot_release(robot);
	if (config->joint_count > MO_ROBOT_MAX_JOINTS) {
		return -1;
	}

	for (enum robot_group_id group = ROBOT_GROUP_TORSO; group < ROBOT_GROUP_COUNT; ++group) {
		size_t group_start = joint_index;

		for (size_t i = 0; i < config->joint_count; ++i) {
			const struct robot_joint_config *joint_config = &config->joints[i];
			size_t slave_index;

			if (joint_config->group != group) {
				continue;
			}
			if (find_slave(master, &joint_config->identity, &slave_index) < 0 ||
			    slave_is_assigned(robot, joint_index, slave_index)) {
				goto fail;
			}

			robot->joints[joint_index++] = (struct robot_joint){
				.name = joint_config->joint_name,
				.group = group,
				.slave_index = slave_index,
				.identity = joint_config->identity,
			};
		}

		if (joint_index != group_start) {
			robot->groups[group_index++] = (struct robot_group){
				.id = group,
				.name = group_name(group),
				.joint_start = group_start,
				.joint_count = joint_index - group_start,
			};
		}
	}

	if (joint_index != config->joint_count) {
		goto fail;
	}

	robot->name = config->name;
	robot->master = master;
	robot->joint_count = joint_index;
	robot->group_count = group_index;
	return 0;

fail:
	robot_release(robot);
	return -1;
}
