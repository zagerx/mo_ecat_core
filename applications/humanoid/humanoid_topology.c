/**
 * @file humanoid_topology.c
 * @brief 人形机器人逻辑拓扑构建预留实现
 *
 * 后续在这里完成扫描从站与 humanoid_config 的匹配和校验。
 */

#include "humanoid_topology.h"

#include <stdlib.h>
#include <string.h>

#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_slave.h"

static const char *group_name(enum humanoid_group_id group)
{
	switch (group) {
	case HUMANOID_GROUP_TORSO:
		return "torso";
	case HUMANOID_GROUP_LEFT_ARM:
		return "left_arm";
	case HUMANOID_GROUP_RIGHT_ARM:
		return "right_arm";
	case HUMANOID_GROUP_LEFT_LEG:
		return "left_leg";
	case HUMANOID_GROUP_RIGHT_LEG:
		return "right_leg";
	case HUMANOID_GROUP_HEAD:
		return "head";
	default:
		return "unknown";
	}
}

static int slave_matches(const struct mo_ecat_slave *slave,
			 const struct humanoid_slave_match *match)
{
	return slave && match && slave->alias == match->alias &&
	       slave->position == match->position &&
	       slave->vendor_id == match->vendor_id &&
	       slave->product_code == match->product_code;
}

static int find_slave(struct mo_ecat_master *master,
		      const struct humanoid_slave_match *match,
		      size_t *slave_index)
{
	size_t count;
	size_t found_index = 0;
	int found = 0;

	if (!master || !match || !slave_index) {
		return -1;
	}

	count = mo_ecat_master_get_slave_count(master);
	for (size_t i = 0; i < count; ++i) {
		const struct mo_ecat_slave *slave = mo_ecat_master_get_slave(master, i);

		if (!slave_matches(slave, match)) {
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

static int slave_is_assigned(const struct humanoid_topology *topology,
			     size_t joint_count,
			     size_t slave_index)
{
	for (size_t i = 0; i < joint_count; ++i) {
		if (topology->joints[i].slave_index == slave_index) {
			return 1;
		}
	}

	return 0;
}

void humanoid_topology_release(struct humanoid_topology *topology)
{
	if (!topology) {
		return;
	}

	free(topology->joints);
	free(topology->groups);
	memset(topology, 0, sizeof(*topology));
}

int humanoid_topology_build(struct mo_ecat_master *master,
			    const struct humanoid_config *config,
			    struct humanoid_topology *topology)
{
	size_t joint_index = 0;
	size_t group_index = 0;

	if (!master || !config || !topology || !config->joints ||
	    config->joint_count == 0 ||
	    mo_ecat_master_get_state(master) != MO_ECAT_MASTER_STATE_DISCOVERED) {
		return -1;
	}
	for (size_t i = 0; i < config->joint_count; ++i) {
		if (!config->joints[i].joint_name ||
		    config->joints[i].group >= HUMANOID_GROUP_COUNT) {
			return -1;
		}
	}

	humanoid_topology_release(topology);
	topology->joints = calloc(config->joint_count, sizeof(*topology->joints));
	topology->groups = calloc(HUMANOID_GROUP_COUNT, sizeof(*topology->groups));
	if (!topology->joints || !topology->groups) {
		goto fail;
	}

	for (enum humanoid_group_id group = HUMANOID_GROUP_TORSO;
	     group < HUMANOID_GROUP_COUNT; ++group) {
		size_t group_start = joint_index;

		for (size_t i = 0; i < config->joint_count; ++i) {
			const struct humanoid_joint_config *joint_config =
				&config->joints[i];
			size_t slave_index;

			if (joint_config->group != group) {
				continue;
			}
			if (find_slave(master, &joint_config->match, &slave_index) < 0 ||
			    slave_is_assigned(topology, joint_index, slave_index)) {
				goto fail;
			}

			topology->joints[joint_index++] = (struct humanoid_joint){
				.name = joint_config->joint_name,
				.group = group,
				.slave_index = slave_index,
				.identity = joint_config->match,
			};
		}

		if (joint_index != group_start) {
			topology->groups[group_index++] = (struct humanoid_group){
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

	topology->joint_count = joint_index;
	topology->group_count = group_index;
	return 0;

fail:
	humanoid_topology_release(topology);
	return -1;
}
