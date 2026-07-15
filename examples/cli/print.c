/**
 * @file print.c
 * @brief CLI 输出辅助函数实现
 */

#include <stdio.h>

#include "print.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "mo_ecat/mo_ecat_pdo.h"
#include "robot.h"

const char *state_name(enum mo_ecat_master_state state)
{
	switch (state) {
	case MO_ECAT_MASTER_STATE_INIT:       return "INIT";
	case MO_ECAT_MASTER_STATE_IDLE:       return "IDLE";
	case MO_ECAT_MASTER_STATE_DISCOVERED: return "DISCOVERED";
	case MO_ECAT_MASTER_STATE_READY:      return "READY";
	case MO_ECAT_MASTER_STATE_RUNNING:    return "RUNNING";
	case MO_ECAT_MASTER_STATE_FAULT:      return "FAULT";
	default:                              return "UNKNOWN";
	}
}

static const char *yes_no(int value)
{
	return value ? "yes" : "no";
}

static const char *al_state_name(enum mo_ecat_slave_al_state state)
{
	switch (state) {
	case MO_ECAT_AL_STATE_INIT:       return "INIT";
	case MO_ECAT_AL_STATE_PRE_OP:     return "PRE-OP";
	case MO_ECAT_AL_STATE_SAFE_OP:    return "SAFE-OP";
	case MO_ECAT_AL_STATE_OP:         return "OP";
	case MO_ECAT_AL_STATE_BOOTSTRAP:  return "BOOT";
	default:                          return "UNKNOWN";
	}
}

void print_state(struct mo_ecat_master *master)
{
	enum mo_ecat_master_state state = mo_ecat_master_get_state(master);

	printf("Master state : %s\n", state_name(state));
	printf("Slave count  : %zu\n", mo_ecat_master_get_slave_count(master));
}

void print_diagnostics(struct mo_ecat_master *master)
{
	int rc = mo_ecat_master_read_diagnostics(master);
	if (rc < 0) {
		printf("Diagnostics unavailable (state: %s). Run 'scan' first.\n",
		       state_name(mo_ecat_master_get_state(master)));
		return;
	}

	size_t count = mo_ecat_master_get_slave_count(master);
	if (count == 0) {
		printf("No slaves discovered.\n");
		return;
	}

	printf("Discovered slaves: %zu\n", count);
	for (size_t i = 0; i < count; ++i) {
		struct mo_ecat_slave_info slave;
		if (mo_ecat_master_get_slave_info(master, i, &slave) < 0) {
			continue;
		}
		printf("  [%zu] %s\n", i,
		       slave.base_info.name[0] ? slave.base_info.name : "<unnamed>");
		printf("       position=%u alias=%u dc=%s\n",
		       slave.base_info.position, slave.base_info.alias, yes_no(slave.base_info.has_dc));
		printf("       vendor=0x%08X product=0x%08X revision=0x%08X\n",
		       slave.base_info.vendor_id, slave.base_info.product_code, slave.base_info.revision_number);
		printf("       default PDO entries: %zu\n", slave.pdo_entry_count);
		printf("       state=%s error=%s online=%s operational=%s al_status=0x%04X\n",
		       al_state_name(slave.state.al_state),
		       yes_no(slave.state.error),
		       yes_no(slave.state.online),
		       yes_no(slave.state.operational),
		       slave.state.al_status_code);
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
			const struct robot_joint *joint =
				&robot->joints[group->joint_start + j];
			struct mo_ecat_slave_info slave;
			int has_slave = mo_ecat_master_get_slave_info(robot->master,
							      joint->slave_index,
							      &slave) == 0;

			printf("    %s -> Slave[%zu]",
			       joint->name, joint->slave_index);
			if (has_slave && slave.base_info.name[0]) {
				printf(" (%s)", slave.base_info.name);
			}
			printf("\n");
		}
	}
}

void print_pdo_entry_mapping(struct mo_ecat_master *master, size_t idx)
{
	size_t count = mo_ecat_master_get_pdo_entry_mapping_count(master);
	if (idx >= count) {
		printf("PDO entry mapping index out of range (count=%zu)\n", count);
		return;
	}

	struct mo_ecat_pdo_entry_mapping mapping;
	if (mo_ecat_master_get_pdo_entry_mapping(master, idx, &mapping) < 0) {
		printf("PDO entry mapping index out of range (count=%zu)\n", count);
		return;
	}
	const char *dir = (mapping.direction == MO_ECAT_PDO_INPUT) ? "IN" : "OUT";

	printf("PDO mapping[%zu]: slave=%zu, object_index=0x%04X, object_subindex=%u, "
	       "dir=%s, offset=%u.%u, bits=%u, gen=%u\n",
	       idx, mapping.slave_index, mapping.object_index, mapping.object_subindex,
	       dir, mapping.byte_offset, mapping.bit_offset, mapping.bit_length,
	       mapping.generation);

	if (mapping.direction == MO_ECAT_PDO_INPUT) {
		const void *p = mo_ecat_pdo_input(master, &mapping);
		if (p) {
			printf("  input value @ %p\n", p);
		}
	} else {
		void *p = mo_ecat_pdo_output(master, &mapping);
		if (p) {
			printf("  output value @ %p\n", p);
		}
	}
}

void print_help(void)
{
	printf("Commands:\n"
	       "  help              show this help\n"
	       "  state             show master state and slave count\n"
	       "  scan              request bus scan from IDLE\n"
	       "  configure         configure bus after DISCOVERED\n"
	       "  activate          activate bus after READY\n"
	       "  deactivate        stop cyclic operation from RUNNING\n"
	       "  reset             release resources and return to IDLE\n"
	       "  diag              print discovered slave information\n"
	       "  topology          build and print robot\n"
	       "  pdo <idx>         print PDO entry mapping information\n"
	       "  exit              quit\n");
}
