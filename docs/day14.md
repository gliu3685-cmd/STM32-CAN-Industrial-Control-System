# Day14 - FreeRTOS 综合架构设计（Phase 2 收官）

## 1. 今日目标（Objectives）

本日把 Day5 ~ Day13 学到的 FreeRTOS 知识**综合起来**，搭建三节点 CAN
工业控制系统的最终 RTOS 框架，作为 Phase 3（CAN 通信）的软件基础。

今日完成：

* 系统任务划分与单一职责设计
* 优先级分配原则（实时性要求 → RMS 思想）
* 同步机制选型（Queue / EventGroup / Timer / Mutex 各司其职）
* 基于"数据新鲜度"的节点在线检测（故障监控）
* 收敛学习期演示任务，正式系统任务上线

最终框架：

```
                    FreeRTOS Scheduler
                          │
   ┌──────────┬───────────┼───────────┬──────────┬──────────┐
 CAN Rx Task ControlTask MotorTask FaultMonitor CanTxTask DebugTask
   (P4)        (P5)       (P3)       (P4)        (P2)      (P1)
      │           │          │           │          │         │
      └─ Queue ───┘          └─ Queue ───┘          │         │
             EventGroup（心跳/告警）←──── Timer(1s) ──┘         │
             Mutex 保护共享状态 SysState_t ←────────────────────┘
```

---

## 2. 理论学习（Theory）

### 2.1 任务划分：单一职责

把"系统要干什么"拆成互相独立的小任务，每个任务只做一件事：

| 任务 | 职责 | 数据来源 |
|---|---|---|
| CAN Rx | 收帧、解析、更新共享状态 | 接收队列 |
| Control | 控制逻辑（越限检测、指令计算） | 事件组/共享状态 |
| Motor | 执行电机指令 | 指令队列 |
| Fault Monitor | 超时/越限巡检 | 心跳事件 |
| CAN Tx | 周期发送 | 定时/状态 |
| Debug | 状态打印 | 共享状态 |

好处：每个任务小而清晰，改一处不影响其他；问题定位快。

### 2.2 优先级分配：实时性优先

工业实时系统经验法则（源自速率单调调度 RMS 思想）：

* **周期越短、越紧急的任务，优先级越高**；
* 控制闭环（Control）> 数据入口（CAN Rx）= 安全监控（Fault Monitor）
  > 执行机构（Motor）> 周期发送/模拟源 > 调试打印；
* 调试类任务永远最低，不能影响控制实时性。

### 2.3 同步机制选型：各司其职

| 机制 | 用在哪 | 为什么 |
|---|---|---|
| 队列 | CAN 帧、电机指令 | 任务间**传数据**，且解耦生产/消费速率 |
| 事件组 | 心跳、新数据、故障告警 | 一个任务等**多个条件**（AND/OR） |
| 软件定时器 | 1s 心跳 | 周期任务不占任务栈、不忙等 |
| 互斥锁 | 共享状态 SysState_t | 多任务读写同一结构，防数据竞争 |

选型口诀：**传数据用队列，发通知用事件组，周期触发用定时器，
保护共享变量用互斥锁。**

### 2.4 故障监控：数据新鲜度 = 节点在线

工业现场判断"节点还活着"的标准做法：记录每个节点**最后收到数据的时间**，
定时检查是否超过阈值（如 1.5s）。超时即判离线并报警。这就是分布式系统的
"软看门狗"，比硬件看门狗更早、更细。

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2
* USB-TTL（CH340G）

软件：

* STM32CubeIDE
* FreeRTOS Kernel V10.3.1（CMSIS_V1，heap_4）
* HAL Library

工程：`f407_blink`（主工程，与仓库 master_node 同步）

当前阶段：Phase 2 收官，即将进入 Phase 3 CAN 通信

---

## 4. 实验实现

### 4.1 新增 app_arch.c / app_arch.h

核心数据结构：

```c
typedef struct {
    uint32_t node_id;      /* 节点 ID：0/1 */
    uint8_t  data[8];
    uint8_t  dlc;
    uint32_t tick;         /* 收帧时刻 */
} CanFrame_t;

typedef struct {
    uint32_t uptime_tick;
    uint8_t  node_online[2];
    int32_t  node_temp[2];
    uint16_t adc, speed;
    uint32_t last_rx_tick[2];
    uint8_t  fault_temp, fault_offline;
} SysState_t;
```

### 4.2 任务与同步对象创建

```c
xCanRxQueue     = xQueueCreate(8, sizeof(CanFrame_t));
xMotorQueue     = xQueueCreate(4, sizeof(uint16_t));
xSysEvents      = xEventGroupCreate();
xSysMutex       = xSemaphoreCreateMutex();
xHeartbeatTimer = xTimerCreate("hbeat", pdMS_TO_TICKS(1000),
                               pdTRUE, NULL, HeartbeatCallback);

xTaskCreate(SimNodeTask,      "sim",   256, NULL, 2, NULL);
xTaskCreate(CanRxTask,        "canrx", 256, NULL, 4, NULL);
xTaskCreate(ControlTask,      "ctrl",  256, NULL, 5, NULL);
xTaskCreate(MotorTask,        "motor", 128, NULL, 3, NULL);
xTaskCreate(FaultMonitorTask, "fault", 128, NULL, 4, NULL);
xTaskCreate(CanTxTask,        "cantx", 128, NULL, 2, NULL);
xTaskCreate(DebugTask,        "debug", 256, NULL, 1, NULL);
```

### 4.3 数据流

* `SimNodeTask`（模拟两个 F103 节点）：每 300ms 发一帧温度帧 + 一帧
  ADC/速度帧到 CAN 接收队列，并置位 `EVT_NEW_FRAME`；
* `CanRxTask`：阻塞收队列，解析帧更新 `SysState_t`（互斥锁保护）；
* `ControlTask`：等新数据/心跳事件，温度 ≥80℃ 置故障，计算目标速度
  下发指令队列；
* `MotorTask`：接收指令并打印；
* `FaultMonitorTask`：每 1s 心跳检查 `last_rx_tick`，超 1.5s 判离线；
* `CanTxTask`：每 1s 模拟发送一帧主节点心跳；
* `DebugTask`：每 2s 打印系统状态与堆水位。

### 4.4 演示：节点掉线

`SimNodeTask` 中节点 0 每帧温度 +2℃，发满 25 帧（约 7.5s 到 80℃）后
停止发送——模拟节点 0 掉线。预期先看到温度告警，再看到离线告警。

---

## 5. 实验过程（Experiment）

烧录后打开串口（115200, 8N1），预期现象（数字以实际为准）：

```
== Day14 System Architecture started ==
[CAN_RX] node=0 dlc=2 data=1E 00 00 00        ← 30℃
[CAN_RX] node=1 dlc=4 data=00 00 E8 03        ← adc=0 speed=1000
[MOTOR] target speed = 1150
[CAN_TX] master heartbeat seq=0
[DEBUG] up=1 node0=ON(32C) node1=ON adc=0 speed=1000 fault= heap=xxxx
...
（约 7.5s 后）
[CTRL] temp over limit (80), reduce speed!     ← 温度告警
（节点 0 停发，1.5s 后）
[FAULT] node offline detected!                 ← 离线告警
[DEBUG] ... node0=OFF ... fault=TEMP OFFLINE
```

验证要点：

* 所有 `[CAN_RX]` / `[MOTOR]` / `[CAN_TX]` / `[DEBUG]` 打印无交错断行
  （UART 互斥锁生效）；
* 温度从 30℃ 每帧 +2℃，约 7.5s 触发 `temp over limit`；
* 节点 0 停止上报后，1.5s 内被 FaultMonitor 判离线；
* `[DEBUG]` 的 `heap=` 数值稳定，无持续下降（无内存泄漏）。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：学习期累积了 7 个演示任务（Memory/Prio/Event/Fault 等），继续叠加
新任务会导致 heap（15KB）不足。

原因：演示任务多为"一次性实验"，与正式架构职责重叠。

解决：Day14 收敛系统——移除 Day3~13 的演示任务创建（代码文件保留归档），
保留故障防线（栈溢出钩子、malloc 钩子、HardFault 解析、configASSERT），
由 `DebugTask` 接管堆水位监控职责。

### Problem 2

问题：多任务读写共享状态 `SysState_t`，直接访问会数据竞争。

原因：RTOS 下任务随时可能被抢占，读写非原子结构可能读到半新半旧数据。

解决：所有对 `SysState_t` 的访问统一加 `xSysMutex`；打印类访问先取
系统锁再取 UART 锁，锁顺序固定，避免死锁。

---

## 7. 今日成果（Result）

完成：

* [x] 系统任务划分（6 个正式任务，单一职责）
* [x] 优先级分配（Control 5 > CAN Rx/Fault 4 > Motor 3 > Tx/Sim 2 > Debug 1）
* [x] Queue/EventGroup/Timer/Mutex 综合运用
* [x] 数据新鲜度节点在线检测（1.5s 超时判离线）
* [x] 温度越限告警与控制指令联动
* [x] 收敛演示任务，保留故障防线
* [x] 编译通过（0 errors, 0 warnings）

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    ├── CAN Rx Task（收帧解析 → 共享状态）
    ├── Control Task（控制逻辑/越限检测）
    ├── Motor Task（电机指令执行）
    ├── Fault Monitor Task（节点在线巡检）
    ├── CAN Tx Task（周期心跳发送）
    ├── Debug Task（状态/堆水位打印）
    ├── Sim Node Task（模拟 F103 数据源，Day15 移除）
    └── Timer Service Task（1s 心跳）
```

---

## 8. 工程总结（Engineering Summary）

## 架构设计先于编码

正式系统的关键不是"能跑"，而是"跑得清楚"：每个任务职责单一、优先级
有依据、同步机制各司其职、故障能被检测和定位。Day14 的框架就是
CAN 阶段的"施工图"——后续只需把模拟数据源换成真实 CAN 外设，
任务骨架不动。

## 共享状态是并发系统的薄弱点

一个结构体被 6 个任务读写，互斥锁是底线；更进一步的设计是"任务间只通过
队列/事件传数据，不共享内存"（消息驱动架构），面试时可作为加分扩展。

---

## 9. 面试问答（Interview Prep）

### Q1：多任务系统怎么划分任务？优先级怎么定？

答题要点：

* 按职责拆分（收/处理/执行/监控/调试），单一职责；
* 优先级按实时性：周期短、影响安全的任务高优先级（RMS 思想）；
* 控制 > 数据入口 = 安全监控 > 执行 > 发送 > 调试。

### Q2：队列、信号量、事件组、互斥锁怎么选？

答题要点：

* 队列：传数据（有内容）；信号量：计数/通知（无内容）；
* 事件组：多条件等待（AND/OR）；互斥锁：保护共享变量（有所有权、可继承）；
* 一句话：数据用队列，条件用事件组，周期用定时器，共享变量用互斥锁。

### Q3：怎么判断从节点是否"活着"？

答题要点：

* 记录最后收到数据的时间（数据新鲜度），超时判离线；
* 用心跳帧 + 超时阈值实现"软看门狗"，比硬件看门狗更细；
* 注意超时阈值要大于心跳周期（如心跳 300ms，阈值 1.5s）。

### Q4：RTOS 下多个任务读写共享变量会怎样？

答题要点：

* 任务随时被抢占，非原子读写可能读到不一致数据（数据竞争）；
* 解决：互斥锁/临界区保护，或改成消息传递架构避免共享；
* 进阶：锁顺序固定防死锁、短临界区防优先级反转。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day14 system architecture (CAN-ready task framework, Phase2 wrap-up)"
```

---

## 11. 下一步计划（Next Step）

Day15 起进入 **Phase 3：CAN 通信**：

* F407 CAN1 外设配置（波特率、过滤器、中断）
* 真实 CAN 收发替换 SimNodeTask
* 双 F103 节点（传感器节点 + 电机节点）联调
* 自定义 CAN 应用层协议（帧 ID 规划、数据格式）
