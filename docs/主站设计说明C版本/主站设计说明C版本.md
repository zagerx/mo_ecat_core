# 主站设计说明（C 版本）

## 1. 设计目标

基于 SOEM 封装一套纯 C 的主站对象层，用于替代现有 C++ 实现，降低维护成本。

- 不重复实现 EtherCAT 底层协议，复用 SOEM 的 `ecx_contextt`。
- 提供清晰的主站生命周期管理，主站状态必须由统一状态机框架驱动。
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
    /* 主站生命周期状态机 */
    struct statemachine sm;
    enum mo_ecat_master_state state;

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

主站从创建到销毁必须始终处于一个明确状态下。状态切换必须使用 `src/common/statemachine/statemachine.c/.h`，不能在业务流程中散落直接赋值状态字段。

主站状态设计如下：

| 状态 | 说明 |
|---|---|
| 初始化状态 `INIT` | 主站刚创建，状态机和内部字段初始化中 |
| 空闲状态 `IDLE` | 主站未持有已配置的总线资源，可以执行配置 |
| 准备状态 `READY` | 已完成从站扫描与 PDO 映射，过程数据区有效，等待启动 |
| 运行状态 `RUNNING` | 周期过程数据正在运行 |
| 降级状态 `DEGRADED` | 周期仍运行，但出现短时 WKC、链路或后端异常 |
| 故障状态 `FAULT` | 发生严重或连续错误，需复位、释放资源或重新创建 |
| 受控状态 `CONTROLLED` | 可选扩展：被外部控制器接管后的状态，第一版不实现 |

约束：

- 主站运行时必须处于上述某一个状态。
- 每个状态的转换必须有明确的进入动作和退出动作。
- `struct ec_master` / `struct mo_ecat_master` 内部必须持有 `struct statemachine sm`。
- 对外可见状态 ID 只允许在状态函数的 `ENTER` 阶段更新。
- 所有迁移必须由状态函数调用 `sm_transition()` 完成，禁止业务流程直接写 `master->state = ...`。
- 非法迁移必须返回错误，不允许调用后端，不允许改变 PDO 映射。

推荐迁移关系：

```text
INIT
  └─ 初始化完成 → IDLE

IDLE
  ├─ configure 成功 → READY
  └─ configure 失败 → FAULT

READY
  ├─ activate 成功 → RUNNING
  └─ close/destroy → IDLE

RUNNING
  ├─ deactivate 成功 → READY
  ├─ 短时异常 → DEGRADED
  └─ 严重异常 → FAULT

DEGRADED
  ├─ 周期恢复正常 → RUNNING
  ├─ deactivate 成功 → READY
  └─ 连续异常达到阈值 → FAULT

FAULT
  └─ reset/destroy 释放资源 → IDLE 或重新创建
```

实现要求：

业务 API 不直接切换状态，只保存请求参数并提交命令，例如 `CONFIGURE`、`ACTIVATE`、`DEACTIVATE`、`RESET`。迁移是否合法由当前状态函数在自己的 `USER_STATUS` 或自定义子阶段判断，合法时完成资源准备、执行对应动作并调用 `sm_transition()`，非法时拒绝命令。

状态机调度必须有且只有一个固定周期入口：

```c
void mo_ecat_master_dispatch(struct mo_ecat_master *master);
```

应用主循环或管理线程每周期调用一次该函数。`mo_ecat_master_cycle_begin()` 和 `mo_ecat_master_cycle_end()` 只负责 EtherCAT 过程数据收发，不隐式调用 `sm_dispatch()`。

主站状态函数必须独立实现，例如：

```c
void mo_ecat_master_state_running(struct statemachine *sm)
{
    switch (sm->phase) {
    case ENTER:
        /* 设置 RUNNING 状态 ID，执行进入动作 */
        sm->phase = USER_STATUS;
        break;
    case USER_STATUS:
        /* 运行态持续动作，由 sm_dispatch() 周期调度 */
        break;
    case EXIT:
        /* 离开 RUNNING 前的动作 */
        break;
    }
}
```

因此，主站状态机实现至少包含：

- `src/mo_ecat_master_states.h`
- `src/mo_ecat_master_states.c`
- `src/common/statemachine/statemachine.h`
- `src/common/statemachine/statemachine.c`
