# 主站设计说明（C 版本）

## 1. 设计目标

基于 SOEM 封装一套纯 C 的主站对象层，用于替代现有 C++ 实现，降低维护成本。

- 不重复实现 EtherCAT 底层协议，复用 SOEM 的 `ecx_contextt`。
- 提供清晰的主站生命周期管理和应用层状态机。
- 对外暴露简洁、稳定的 C API。

## 2. 主站对象结构

```c
struct slave_info {
    /* 从站静态信息，待后续补充 */
};

struct slave {
    struct slave_info info;
};

struct slave_group {
    struct slave *slaves;   /* 根据扫描信息动态确定大小 */
    int  slave_count;//从站数量
    char name[32]; //组名
};

#define GROUP_NUM 1

typedef struct ec_master {
    /* SOEM 上下文 */
    ecx_contextt context;

    /* 网卡 */
    char ifname[IFNAMSIZ];

    /* 从站组，group_num 根据配置文件获取 */
    struct slave_group group[GROUP_NUM];

    /* 过程数据映像 */
    uint8_t *iomap;
    size_t   iomap_size;

    /* 分布式时钟 */
    int     dc_enabled;
    int64_t dc_time;
    int     dc_ref_slave;

    /* 用户回调数据 */
    void *user_data;
} ec_master_t;
```

## 3. 主站状态机

主站状态设计如下：

| 状态 | 说明 |
|---|---|
| 初始化状态 | 主站刚创建，尚未扫描总线 |
| 准备状态 | 已完成从站扫描与 PDO 映射，等待启动 |
| 运行状态 | 周期过程数据正在运行 |
| 故障状态 | 发生错误，需复位或重新配置 |
| 受控状态 | 被外部控制器接管后的状态 |

约束：

- 主站运行时必须处于上述某一个状态。
- 每个状态的转换必须有明确的进入动作和退出动作。

具体实现：后续完善。
