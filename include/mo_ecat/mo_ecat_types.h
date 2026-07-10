#ifndef MO_ECAT_TYPES_H
#define MO_ECAT_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MO_ECAT_MAX_NAME_LEN     80
#define MO_ECAT_MAX_IFNAME_LEN   64
#define MO_ECAT_MAX_SLAVES       16
#define MO_ECAT_MAX_PDO_ENTRIES  32
#define MO_ECAT_MAX_SM           8
#define MO_ECAT_MAX_FMMU         4

/* ==================== 主站生命周期状态 ==================== */

enum mo_ecat_master_state {
    MO_ECAT_MASTER_STATE_INIT,
    MO_ECAT_MASTER_STATE_IDLE,
    MO_ECAT_MASTER_STATE_DISCOVERED,
    MO_ECAT_MASTER_STATE_FAULT
};

/* ==================== AL 状态 ==================== */

enum mo_ecat_al_state {
    MO_ECAT_AL_STATE_INIT,
    MO_ECAT_AL_STATE_PRE_OP,
    MO_ECAT_AL_STATE_SAFE_OP,
    MO_ECAT_AL_STATE_OP,
    MO_ECAT_AL_STATE_BOOTSTRAP,
    MO_ECAT_AL_STATE_UNKNOWN
};

struct mo_ecat_slave_state {
    enum mo_ecat_al_state al_state;
    int                   error;
    uint16_t              al_status_code;
    int                   online;
    int                   operational;
};

/* ==================== 配置模型 ==================== */

enum mo_ecat_pdo_direction {
    MO_ECAT_PDO_INPUT,
    MO_ECAT_PDO_OUTPUT
};

struct mo_ecat_pdo_entry_config {
    uint16_t index;
    uint8_t  subindex;
    uint8_t  bit_length;
    enum mo_ecat_pdo_direction direction;
};

struct mo_ecat_dc_config {
    int      enabled;
    uint32_t assign_activate;
    uint32_t sync0_cycle_ns;
    int32_t  sync0_shift_ns;
};

/**
 * @brief 主站启动选项
 *
 * 启动与扫描阶段只需要网卡名称；从站 PDO、DC 等配置由后续的
 * 用户配置阶段提供。
 */
struct mo_ecat_master_options {
    char interface_name[MO_ECAT_MAX_IFNAME_LEN + 1];
};

/**
 * @brief 用户从站配置
 *
 * 配置模型采用固定大小数组，避免指针嵌套和动态分配。
 * 调用者直接填充字段即可，后续可由配置文件解析结果生成。
 */
struct mo_ecat_slave_config {
    uint16_t alias;
    uint16_t position;

    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;

    char     name[MO_ECAT_MAX_NAME_LEN + 1];

    struct mo_ecat_pdo_entry_config pdo_entries[MO_ECAT_MAX_PDO_ENTRIES];
    size_t pdo_entry_count;

    int dc_active;
};

/**
 * @brief 用户配置
 *
 * 自包含的固定大小结构，可直接用 = 赋值或 memcpy 拷贝。
 */
struct mo_ecat_user_config {
    struct mo_ecat_slave_config slaves[MO_ECAT_MAX_SLAVES];
    size_t slave_count;
};

/* ==================== 运行时类型 ==================== */

struct mo_ecat_process_image {
    uint8_t *memory;
    size_t   size;
    uint32_t generation;
    int      active;
};

struct mo_ecat_pdo_ref {
    size_t   slave_index;
    uint16_t pdo_index;
    uint16_t entry_index;

    uint32_t byte_offset;
    uint8_t  bit_offset;
    uint8_t  bit_length;
    enum mo_ecat_pdo_direction direction;
    uint32_t generation;
};

/**
 * @brief 从站默认 PDO 映射项
 *
 * 在扫描后的 PDO 信息读取阶段，从从站的 PDO 分配对象中读取。
 * 这是从站当前声明的映射描述，不包含过程数据映像中的字节偏移；
 * 后者仅在后续完成过程数据映像配置后才会产生。
 */
struct mo_ecat_pdo_entry_info {
    uint16_t pdo_index;  /**< PDO 映射对象索引，例如 0x1600 / 0x1A00 */
    uint16_t index;      /**< 被映射对象索引 */
    uint8_t  subindex;   /**< 被映射对象子索引 */
    uint8_t  bit_length; /**< 映射位宽 */
    enum mo_ecat_pdo_direction direction; /**< 主站视角的数据方向 */
};

/**
 * @brief 从站邮箱参数
 *
 * 由 SOEM 扫描阶段从 SII/EEPROM 读取，供后续邮箱通信（CoE/SDO 等）使用。
 */
struct mo_ecat_slave_mailbox {
    uint16_t protocol;      /**< 支持的邮箱协议位图（SOEM mbx_proto） */
    uint16_t write_address; /**< 邮箱写起始地址（mbx_wo） */
    uint16_t write_size;    /**< 邮箱写长度（mbx_l） */
    uint16_t read_address;  /**< 邮箱读起始地址（mbx_ro） */
    uint16_t read_size;     /**< 邮箱读长度（mbx_rl） */
};

/**
 * @brief Sync Manager 配置
 *
 * 由 SOEM 扫描阶段从 SII 读取，后续 PDO 映射和邮箱通信会用到。
 */
struct mo_ecat_sync_manager {
    uint16_t start_address; /**< SM 物理起始地址 */
    uint16_t length;        /**< SM 长度 */
    uint32_t flags;         /**< SM 标志（方向、激活等） */
    uint8_t  type;          /**< SM 类型（邮箱输入/输出、PDO 输入/输出等） */
};

/**
 * @brief FMMU 功能分配
 *
 * 由 SOEM 扫描阶段从 SII 读取，0xff 表示未分配。
 */
struct mo_ecat_fmmu {
    uint8_t function; /**< FMMU 功能码 */
};

struct mo_ecat_slave {
    uint16_t position;
    uint16_t alias;

    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;

    char     name[MO_ECAT_MAX_NAME_LEN + 1];
    int      has_dc;
    uint32_t propagation_delay_ns;

    struct mo_ecat_slave_state state;

    struct mo_ecat_slave_mailbox mailbox; /**< 邮箱参数 */
    int      has_coe;  /**< 支持 CoE（CANopen over EtherCAT） */
    int      has_foe;  /**< 支持 FoE（File over EtherCAT） */
    int      has_eoe;  /**< 支持 EoE（Ethernet over EtherCAT） */
    int      has_soe;  /**< 支持 SoE（Servo over EtherCAT） */

    struct mo_ecat_sync_manager sm[MO_ECAT_MAX_SM]; /**< Sync Manager 配置 */
    struct mo_ecat_fmmu fmmu[MO_ECAT_MAX_FMMU];     /**< FMMU 功能分配 */

    struct mo_ecat_pdo_entry_info pdo_entries[MO_ECAT_MAX_PDO_ENTRIES];
    size_t pdo_entry_count; /**< 当前从站默认 PDO 映射项数量 */
};

struct mo_ecat_cycle_result {
    int      backend_error;
    int      link_up;

    uint32_t expected_wkc;
    uint32_t actual_wkc;

    int64_t  dc_time_ns;
    int      dc_time_valid;

    int      diagnostics_required;
};

/* 前向声明 */
struct mo_ecat_master;

#ifdef __cplusplus
}
#endif

#endif /* MO_ECAT_TYPES_H */
