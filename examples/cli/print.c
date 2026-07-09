/**
 * @file print.c
 * @brief CLI 输出辅助函数实现
 */

#include <stdio.h>

#include "print.h"
#include "mo_ecat/mo_ecat_master_state.h"
#include "mo_ecat/mo_ecat_slave.h"
#include "mo_ecat/mo_ecat_pdo.h"

const char *state_name(enum mo_ecat_master_state state)
{
	switch (state) {
	case MO_ECAT_MASTER_STATE_INIT:     return "INIT";
	case MO_ECAT_MASTER_STATE_IDLE:     return "IDLE";
	case MO_ECAT_MASTER_STATE_READY:    return "READY";
	case MO_ECAT_MASTER_STATE_RUNNING:  return "RUNNING";
	case MO_ECAT_MASTER_STATE_DEGRADED: return "DEGRADED";
	case MO_ECAT_MASTER_STATE_FAULT:    return "FAULT";
	default:                            return "UNKNOWN";
	}
}

void print_state(struct mo_ecat_master *master)
{
	struct mo_ecat_cycle_result result = {0};
	(void)mo_ecat_master_get_cycle_result(master, &result);

	enum mo_ecat_master_state state = mo_ecat_master_get_state(master);

	printf("state: %s (%d) | slaves: %zu | WKC: %u/%u | DC: %lld | diag_required: %d\n",
	       state_name(state), state,
	       mo_ecat_master_get_slave_count(master),
	       result.actual_wkc, result.expected_wkc,
	       (long long)result.dc_time_ns,
	       result.diagnostics_required);
}

void print_diagnostics(struct mo_ecat_master *master)
{
	int rc = mo_ecat_master_read_diagnostics(master);
	if (rc < 0) {
		printf("read_diagnostics failed: %d\n", rc);
		return;
	}

	size_t count = mo_ecat_master_get_slave_count(master);
	for (size_t i = 0; i < count; ++i) {
		const struct mo_ecat_slave *slave = mo_ecat_master_get_slave(master, i);
		if (!slave) {
			continue;
		}
		printf("  Slave[%zu]: %s, al_state=%d, online=%d, op=%d, err=%d, code=0x%04X\n",
		       i, slave->name,
		       slave->state.al_state,
		       slave->state.online,
		       slave->state.operational,
		       slave->state.error,
		       slave->state.al_status_code);
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

	printf("PDO[%zu]: slave=%zu, pdo=0x%04X, entry=%u, "
	       "dir=%s, offset=%u.%u, bits=%u, gen=%u\n",
	       idx, ref->slave_index, ref->pdo_index, ref->entry_index,
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
	       "  help                show this help\n"
	       "  state               print master state and last cycle result\n"
	       "  config <ifname>     submit CONFIGURE command\n"
	       "  activate            submit ACTIVATE command\n"
	       "  deactivate          submit DEACTIVATE command\n"
	       "  reset               submit RESET command\n"
	       "  diag                call read_diagnostics() and print slave states\n"
	       "  pdo <idx>           print PDO ref info\n"
	       "  exit                quit\n");
}
