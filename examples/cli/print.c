/**
 * @file print.c
 * @brief CLI 输出辅助函数实现
 */

#include <stdio.h>

#include "print.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_topology.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "robot.h"

const char *state_name(enum mo_ecat_master_state state)
{
	switch (state) {
	case MO_ECAT_MASTER_STATE_INIT:
		return "INIT";
	case MO_ECAT_MASTER_STATE_IDLE:
		return "IDLE";
	case MO_ECAT_MASTER_STATE_READY:
		return "READY";
	case MO_ECAT_MASTER_STATE_RUNNING:
		return "RUNNING";
	case MO_ECAT_MASTER_STATE_DEBUG_SLAVE:
		return "DEBUG";
	case MO_ECAT_MASTER_STATE_FAULT:
		return "FAULT";
	default:
		return "UNKNOWN";
	}
}

static const char *yes_no(int value)
{
	return value ? "yes" : "no";
}

static const char *al_state_name(enum mo_ecat_node_al_state state)
{
	switch (state) {
	case MO_ECAT_NODE_AL_STATE_INIT:
		return "INIT";
	case MO_ECAT_NODE_AL_STATE_PRE_OP:
		return "PRE-OP";
	case MO_ECAT_NODE_AL_STATE_SAFE_OP:
		return "SAFE-OP";
	case MO_ECAT_NODE_AL_STATE_OP:
		return "OP";
	case MO_ECAT_NODE_AL_STATE_BOOTSTRAP:
		return "BOOT";
	default:
		return "UNKNOWN";
	}
}

void print_state(struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state = mo_ecat_master_get_state(master);

	printf("Master state : %s\n", state_name(state));
	printf("Node count   : %zu\n", mo_ecat_master_get_node_count(master));
}

void print_diagnostics(struct mo_ecat_master *master)
{
	size_t count = mo_ecat_master_get_node_count(master);
	if (count == 0) {
		printf("No slaves discovered. Run 'scan' first.\n");
		return;
	}

	printf("Discovered slaves: %zu\n", count);
	for (size_t i = 0; i < count; ++i) {
		struct mo_ecat_node_info node;
		if (mo_ecat_master_get_node_info(master, i, &node) < 0) {
			continue;
		}
		printf("  [%zu] %s\n", i, node.name[0] ? node.name : "<unnamed>");
		printf("       position=%u alias=%u dc=%s\n", node.position, node.alias,
		       yes_no(node.dc_supported));
		printf("       vendor=0x%08X product=0x%08X revision=0x%08X\n", node.vendor_id,
		       node.product_code, node.revision_number);
		printf("       state=%s error=%s online=%s operational=%s al_status=0x%04X\n",
		       al_state_name(node.state.al_state), yes_no(node.state.has_error),
		       yes_no(node.state.is_online), yes_no(node.state.is_operational),
		       node.state.al_status_code);
	}
}

void print_robot(const struct robot *robot)
{
	if (!robot || !robot->master || robot->group_count == 0) {
		printf("No robot has been built.\n");
		return;
	}

	printf("Robot: %s\n", robot->name);
	for (size_t i = 0; i < robot->group_count; ++i) {
		const struct robot_group *group = &robot->groups[i];

		printf("  %s:\n", group->name);
		for (size_t j = 0; j < group->joint_count; ++j) {
			const struct robot_joint *joint = &robot->joints[group->joint_start + j];
			struct mo_ecat_node_info node;
			int has_node = mo_ecat_master_get_node_info(robot->master,
								    joint->slave_index, &node) == 0;

			printf("    %s -> Node[%zu]", joint->name, joint->slave_index);
			if (has_node && node.name[0]) {
				printf(" (%s)", node.name);
			}
			printf("\n");
		}
	}
}

void print_pdo_entry(struct mo_ecat_master *master, size_t idx)
{
	size_t count = mo_ecat_master_get_pdo_entry_count(master);
	if (idx >= count) {
		printf("PDO entry index out of range (count=%zu)\n", count);
		return;
	}

	struct slave_pdo_entry record;
	if (mo_ecat_master_get_pdo_entry(master, idx, &record) < 0) {
		printf("PDO entry index out of range (count=%zu)\n", count);
		return;
	}
	const char *dir = (record.spec.direction == MO_ECAT_PDO_INPUT) ? "IN" : "OUT";

	printf("PDO entry[%zu]: slave=%zu, object_index=0x%04X, object_subindex=%u, "
	       "dir=%s, bits=%u\n",
	       idx, record.slave_index, record.spec.object_index, record.spec.object_subindex, dir,
	       record.spec.bit_length);
}

void print_help(void)
{
	printf("Commands:\n"
	       "  help              show this help\n"
	       "  state             show master state and node count\n"
	       "  scan              request bus scan from IDLE\n"
	       "  configure         configure previously scanned bus\n"
	       "  activate          activate bus after READY\n"
	       "  deactivate        stop cyclic operation from RUNNING\n"
	       "  reset             release resources and return to IDLE\n"
	       "  diag              print discovered node information\n"
	       "  topology          build and print robot\n"
	       "  pdo <idx>         print PDO entry information\n"
	       "  exit              quit\n");
}
