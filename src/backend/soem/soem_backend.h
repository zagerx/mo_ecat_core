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

/**
 * soem_backend_context_get - 获取 SOEM 后端上下文
 * @backend: 后端实例指针
 *
 * Return: SOEM 后端上下文指针
 */
struct soem_backend_context *soem_backend_context_get(struct backend_instance *backend);

/**
 * soem_backend_node_al_state - 转换 SOEM 状态到核心层节点状态
 * @soem_state: SOEM 原始状态值
 *
 * Return: 对应的节点 AL 状态
 */
enum mo_ecat_node_al_state soem_backend_node_al_state(uint16_t soem_state);

/**
 * soem_backend_open - 打开 SOEM 后端
 * @backend: 后端实例指针
 * @config: 主站配置指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_open(struct backend_instance *backend,
		      const struct mo_ecat_master_config *config);

/**
 * soem_backend_load_slave_info - 加载从站信息
 * @backend: 后端实例指针
 * @slave_count: 用于返回从站数量的指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_load_slave_info(struct backend_instance *backend, size_t *slave_count);

/**
 * soem_backend_close - 关闭 SOEM 后端
 * @backend: 后端实例指针
 */
void soem_backend_close(struct backend_instance *backend);

/**
 * soem_backend_translate_slave_info - 转换 SOEM 从站信息到核心层结构
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_translate_slave_info(struct backend_instance *backend,
				     struct master_slave *slaves, size_t slave_count);

/**
 * soem_backend_read_pdo_entries - 读取 SOEM 从站默认 PDO 映射条目
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_read_pdo_entries(struct backend_instance *backend,
				  struct master_slave *slaves, size_t slave_count);

/**
 * soem_backend_configure_dc - 配置 SOEM 后端分布式时钟
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_configure_dc(struct backend_instance *backend);

/**
 * soem_backend_build_pdo_mapping - 建立 SOEM 后端 PDO 映射
 * @backend: 后端实例指针
 * @entries: PDO entry 映射数组
 * @entry_count: PDO entry 数量
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_build_pdo_mapping(struct backend_instance *backend,
				   struct master_pdo_entry_mapping *entries, size_t entry_count);

/**
 * soem_backend_get_pdo_image - 获取 SOEM 后端 PDO 数据区域
 * @backend: 后端实例指针
 * @image: 用于返回 PDO 数据映像的指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_get_pdo_image(struct backend_instance *backend,
			       struct master_pdo_image *image);

/**
 * soem_backend_activate - 激活 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_activate(struct backend_instance *backend);

/**
 * soem_backend_cyclic_receive - SOEM 后端周期接收
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_cyclic_receive(struct backend_instance *backend,
				 struct mo_ecat_cyclic_result *result);

/**
 * soem_backend_cyclic_send - SOEM 后端周期发送
 * @backend: 后端实例指针
 * @result: 周期结果指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_cyclic_send(struct backend_instance *backend,
			      struct mo_ecat_cyclic_result *result);

/**
 * soem_backend_read_all_slave_states - 读取 SOEM 所有从站状态
 * @backend: 后端实例指针
 * @slaves: 核心层从站数组
 * @slave_count: 从站数量
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_read_all_slave_states(struct backend_instance *backend,
				       struct master_slave *slaves, size_t slave_count);

/**
 * soem_backend_read_single_slave_state - 读取 SOEM 单个从站状态
 * @backend: 后端实例指针
 * @slave_index: 从站索引
 * @state: 用于返回从站状态的指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_read_single_slave_state(struct backend_instance *backend,
					  size_t slave_index,
					  struct mo_ecat_node_state *state);

/**
 * soem_backend_deactivate - 停用 SOEM 后端周期通信
 * @backend: 后端实例指针
 *
 * Return: 0 成功，非 0 失败
 */
int soem_backend_deactivate(struct backend_instance *backend);

#endif /* SOEM_BACKEND_H */
