#ifndef MO_ECAT_BACKEND_H
#define MO_ECAT_BACKEND_H

#include "mo_ecat/mo_ecat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* backend 结构体定义已位于公共头文件 mo_ecat_types.h */

/* 便捷内联包装 */
static inline int mo_ecat_backend_open(struct mo_ecat_backend *backend,
				       const struct mo_ecat_config *config)
{
	return backend->ops->open(backend, config);
}

static inline int mo_ecat_backend_configure(struct mo_ecat_backend *backend,
					    const struct mo_ecat_config *config,
					    struct mo_ecat_process_image *image,
					    struct mo_ecat_pdo_ref *pdo_refs, size_t pdo_ref_count,
					    struct mo_ecat_slave *slaves, size_t slave_count)
{
	return backend->ops->configure(backend, config, image, pdo_refs, pdo_ref_count, slaves,
				       slave_count);
}

static inline int mo_ecat_backend_activate(struct mo_ecat_backend *backend)
{
	return backend->ops->activate(backend);
}

static inline int mo_ecat_backend_cycle_begin(struct mo_ecat_backend *backend,
					      struct mo_ecat_cycle_result *result)
{
	return backend->ops->cycle_begin(backend, result);
}

static inline int mo_ecat_backend_cycle_end(struct mo_ecat_backend *backend,
					    struct mo_ecat_cycle_result *result)
{
	return backend->ops->cycle_end(backend, result);
}

static inline int mo_ecat_backend_read_diagnostics(struct mo_ecat_backend *backend,
						   struct mo_ecat_slave_state *states,
						   size_t state_count)
{
	return backend->ops->read_diagnostics(backend, states, state_count);
}

static inline int mo_ecat_backend_deactivate(struct mo_ecat_backend *backend)
{
	return backend->ops->deactivate(backend);
}

static inline void mo_ecat_backend_close(struct mo_ecat_backend *backend)
{
	backend->ops->close(backend);
}

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_BACKEND_H */
