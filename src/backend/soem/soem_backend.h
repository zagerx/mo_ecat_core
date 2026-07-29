/*
 * backend/soem/soem_backend.h - SOEM 后端私有接口
 *
 * 定义 SOEM 后端上下文结构及后端实现使用的回调函数。
 */

#ifndef SOEM_BACKEND_H
#define SOEM_BACKEND_H

#include "../backend.h"
#include "soem/soem.h"

/**
 * SOEM_BACKEND_IOMAP_SIZE - SOEM 后端 IOmap 缓冲区大小
 */
#define SOEM_BACKEND_IOMAP_SIZE 2048

/**
 * struct soem_backend_context - SOEM 后端私有上下文
 * @context: SOEM 上下文对象
 * @iomap: PDO 数据 IOmap 缓冲区
 * @pdo_image_size: 实际 PDO 映像大小
 * @expected_wkc: 期望的工作计数器
 * @opened: 是否已打开
 * @dc_configured: 是否已配置 DC
 * @pdo_mapping_ready: PDO 映射是否已就绪
 */
struct soem_backend_context {
	ecx_contextt context;
	uint8_t iomap[SOEM_BACKEND_IOMAP_SIZE];
	size_t pdo_image_size;
	uint32_t expected_wkc;
	int opened;
	int dc_configured;
	int pdo_mapping_ready;
};

struct soem_backend_context *soem_backend_context_get(struct backend_instance *backend);

enum mo_ecat_node_al_state soem_backend_node_al_state(uint16_t soem_state);

enum backend_error soem_backend_open(struct backend_instance *backend,
				     const struct mo_ecat_master_config *config);

enum backend_error soem_backend_load_slave_info(struct backend_instance *backend,
					 size_t *slave_count);

void soem_backend_close(struct backend_instance *backend);

enum backend_error soem_backend_translate_slave_info(struct backend_instance *backend,
						     struct slave *slaves, size_t slave_count);

enum backend_error soem_backend_read_pdo_entries(struct backend_instance *backend,
					  struct slave *slaves, size_t slave_count);

enum backend_error soem_backend_configure_dc(struct backend_instance *backend);

enum backend_error soem_backend_build_pdo_mapping(struct backend_instance *backend,
						   struct master_pdo_entry_mapping *entries,
						   size_t entry_count);

enum backend_error soem_backend_get_pdo_image(struct backend_instance *backend,
					      struct master_pdo_image *image);

enum backend_error soem_backend_activate(struct backend_instance *backend);

enum backend_error soem_backend_cyclic_receive(struct backend_instance *backend,
						struct mo_ecat_cyclic_result *result);

enum backend_error soem_backend_cyclic_send(struct backend_instance *backend,
					     struct mo_ecat_cyclic_result *result);

enum backend_error soem_backend_read_all_slave_states(struct backend_instance *backend,
						      struct slave *slaves, size_t slave_count);

enum backend_error soem_backend_read_single_slave_state(struct backend_instance *backend,
							 size_t slave_index,
							 struct mo_ecat_node_state *state);

enum backend_error soem_backend_deactivate(struct backend_instance *backend);

#endif /* SOEM_BACKEND_H */
