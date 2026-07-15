#ifndef SLAVE_PRIV_H
#define SLAVE_PRIV_H

#include "mo_ecat/mo_ecat_topology.h"

/* 后端扫描缓存的固定容量，不属于公开 API 约束。 */
#define MASTER_MAX_PDO_ENTRIES 32
#define MASTER_MAX_SM          8
#define MASTER_MAX_FMMU        4

struct master_slave_pdo_entry {
	uint16_t pdo_index;
	uint16_t object_index;
	uint8_t object_subindex;
	uint8_t bit_length;
	enum mo_ecat_cyclic_direction direction;
};

struct master_slave_mailbox {
	uint16_t protocol;
	uint16_t write_address;
	uint16_t write_size;
	uint16_t read_address;
	uint16_t read_size;
};

struct master_slave_sync_manager {
	uint16_t start_address;
	uint16_t length;
	uint32_t flags;
	uint8_t type;
};

struct master_slave_fmmu {
	uint8_t function;
};

struct master_slave_base_info {
	uint16_t position;
	uint16_t alias;
	uint32_t vendor_id;
	uint32_t product_code;
	uint32_t revision_number;
	char name[MO_ECAT_MAX_NAME_LEN + 1];
	int dc_supported;
	uint32_t propagation_delay_ns;
	struct master_slave_mailbox mailbox;
	int has_coe;
	int has_foe;
	int has_eoe;
	int has_soe;
	struct master_slave_sync_manager sm[MASTER_MAX_SM];
	struct master_slave_fmmu fmmu[MASTER_MAX_FMMU];
};

/* 核心层保存的完整从站扫描结果；应用层只能取得摘要副本。 */
struct master_slave {
	struct master_slave_base_info base_info;
	struct mo_ecat_node_state state;
	struct master_slave_pdo_entry pdo_entries[MASTER_MAX_PDO_ENTRIES];
	size_t pdo_entry_count;
};

#endif /* SLAVE_PRIV_H */
