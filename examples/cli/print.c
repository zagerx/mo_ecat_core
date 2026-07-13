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
	case MO_ECAT_MASTER_STATE_INIT:     return "INIT";
	case MO_ECAT_MASTER_STATE_IDLE:     return "IDLE";
	case MO_ECAT_MASTER_STATE_DISCOVERED:return "DISCOVERED";
	case MO_ECAT_MASTER_STATE_FAULT:    return "FAULT";
	default:                            return "UNKNOWN";
	}
}

static const char *al_state_name(enum mo_ecat_al_state state)
{
	switch (state) {
	case MO_ECAT_AL_STATE_INIT:      return "INIT";
	case MO_ECAT_AL_STATE_PRE_OP:    return "PRE_OP";
	case MO_ECAT_AL_STATE_SAFE_OP:   return "SAFE_OP";
	case MO_ECAT_AL_STATE_OP:        return "OP";
	case MO_ECAT_AL_STATE_BOOTSTRAP: return "BOOTSTRAP";
	default:                         return "UNKNOWN";
	}
}

static const char *yes_no(int value)
{
	return value ? "yes" : "no";
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
		printf("Diagnostics unavailable (state: %s). Run 'discover' first.\n",
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
		const struct mo_ecat_slave *slave = mo_ecat_master_get_slave(master, i);
		if (!slave) {
			continue;
		}
		printf("  [%zu] %s\n", i,
		       slave->name[0] ? slave->name : "<unnamed>");
		printf("       position=%u alias=%u dc=%s\n",
		       slave->position, slave->alias, yes_no(slave->has_dc));
		printf("       vendor=0x%08X product=0x%08X revision=0x%08X\n",
		       slave->vendor_id, slave->product_code, slave->revision_number);
		printf("       al=%s online=%s operational=%s error=%s code=0x%04X\n",
		       al_state_name(slave->state.al_state),
		       yes_no(slave->state.online),
		       yes_no(slave->state.operational),
		       yes_no(slave->state.error),
		       slave->state.al_status_code);
		printf("       mailbox proto=0x%04X wr=0x%04X/%u rd=0x%04X/%u\n",
		       slave->mailbox.protocol,
		       slave->mailbox.write_address,
		       slave->mailbox.write_size,
		       slave->mailbox.read_address,
		       slave->mailbox.read_size);
		printf("       protocols: CoE=%s FoE=%s EoE=%s SoE=%s\n",
		       yes_no(slave->has_coe),
		       yes_no(slave->has_foe),
		       yes_no(slave->has_eoe),
		       yes_no(slave->has_soe));
		for (size_t sm = 0; sm < MO_ECAT_MAX_SM; ++sm) {
			if (slave->sm[sm].length == 0) {
				continue;
			}
			printf("       SM[%zu]: addr=0x%04X len=%u flags=0x%08X type=%u\n",
			       sm,
			       slave->sm[sm].start_address,
			       slave->sm[sm].length,
			       slave->sm[sm].flags,
			       slave->sm[sm].type);
		}
		printf("       FMMU: ");
		for (size_t fmmu = 0; fmmu < MO_ECAT_MAX_FMMU; ++fmmu) {
			printf("[%zu]=0x%02X ", fmmu, slave->fmmu[fmmu].function);
		}
		printf("\n");
		printf("       default PDO entries: %zu\n", slave->pdo_entry_count);
		for (size_t pdo = 0; pdo < slave->pdo_entry_count; ++pdo) {
			const struct mo_ecat_pdo_entry_info *entry =
				&slave->pdo_entries[pdo];
			printf("         %s PDO=0x%04X object=0x%04X:%02X bits=%u\n",
			       entry->direction == MO_ECAT_PDO_OUTPUT ? "OUT" : "IN",
			       entry->pdo_index, entry->index, entry->subindex,
			       entry->bit_length);
		}
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
			const struct mo_ecat_slave *slave =
				mo_ecat_master_get_slave(robot->master, joint->slave_index);

			printf("    %s -> Slave[%zu]",
			       joint->name, joint->slave_index);
			if (slave && slave->name[0]) {
				printf(" (%s)", slave->name);
			}
			printf("\n");
		}
	}
}

void print_pdo_ref(struct mo_ecat_master *master, size_t idx)
{
	size_t count = mo_ecat_master_get_pdo_ref_count(master);
	if (idx >= count) {
		printf("PDO ref index out of range (count=%zu)\n", count);
		return;
	}

	const struct mo_ecat_pdo_ref *ref = mo_ecat_master_get_pdo_ref(master, idx);
	const char *dir = (ref->direction == MO_ECAT_PDO_INPUT) ? "IN" : "OUT";

	printf("PDO[%zu]: slave=%zu, index=0x%04X, subindex=%u, "
	       "dir=%s, offset=%u.%u, bits=%u, gen=%u\n",
	       idx, ref->slave_index, ref->index, ref->subindex,
	       dir, ref->byte_offset, ref->bit_offset, ref->bit_length,
	       ref->generation);

	if (ref->direction == MO_ECAT_PDO_INPUT) {
		const void *p = mo_ecat_pdo_input(master, ref);
		if (p) {
			printf("  input value @ %p\n", p);
		}
	} else {
		void *p = mo_ecat_pdo_output(master, ref);
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
	       "  discover          request bus discovery from IDLE\n"
	       "  reset             release resources and return to IDLE\n"
	       "  diag              print discovered slave information\n"
	       "  topology          build and print robot\n"
	       "  pdo <idx>         print PDO reference information\n"
	       "  exit              quit\n");
}
