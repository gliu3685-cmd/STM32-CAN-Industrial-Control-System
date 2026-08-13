# Day16 - F103 节点 CAN 收发（三节点通信第一步）

## 1. 今日目标（Objectives）

本日继续 **Phase 3：CAN 通信系统**，重点掌握：

* 搭建两个 F103 节点工程（Node1 传感器 / Node2 电机）：CAN1 + USART1 + FreeRTOS
* 掌握 F103 与 F407 的 CAN 差异：APB1 时钟、波特率重算、AFIO 重映射
* 用 ID 掩码模式实现"精确过滤"（Node 只收自己的命令帧）
* 启动 F407 ↔ Node1 两节点联调（硬件问题未完全解决，详见 §7）

---

## 2. 理论学习（Theory）

### 2.1 F103 与 F407 的 CAN 差异

外设核心相同（bxCAN），HAL 函数名完全一样，Day15 的代码模板可以搬。但有两处必须重算：

**① APB1 时钟（波特率参数）**

| | F407 | F103 |
|---|---|---|
| SYSCLK | 168MHz | 72MHz |
| APB1 | 42MHz | **36MHz** |

波特率公式：`波特率 = APB1 / (Prescaler × (1 + BS1 + BS2))`

F103 重算 500 kbit/s：`36MHz / 500k = 72 tq/bit`，取 `Prescaler=4` → 9MHz → 18 tq：
**BS1=15、BS2=2、SJW=1**，采样点 = (1+15)/18 ≈ 88.9%。

同一波特率，F407 用 `PSC=6, BS1=11, BS2=2`，F103 用 `PSC=4, BS1=15, BS2=2`——参数不同、波特率相同。

**② AFIO 重映射（关键坑）**

F103 的 CAN1 引脚**默认是 PB8/PB9**，PA11/PA12 属于"重映射引脚"。要用 PA11/PA12 必须在 MSP 里开启：

```c
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_CAN1_1();   /* CAN1 重映射到 PA11(RX)/PA12(TX) */
```

F407 没有这个机制（PA11/PA12 直接是 CAN1 的复用功能），这是 F1 特有的坑。

### 2.2 FreeRTOS 在 F103 上的移植

F103C8T6 只有 20KB RAM / 64KB Flash，配置要抠：

* `configTOTAL_HEAP_SIZE = 8KB`（F407 是 15360）
* `configMAX_PRIORITIES = 5`
* 移植 port 用 `portable/GCC/ARM_CM3`（M3 内核）
* 任务栈 128~256 字；F103 只有 CAN1（无 CAN2）

### 2.3 过滤器：ID 掩码精确过滤

掩码模式匹配公式：`(ID & Mask) == (Filter & Mask)` 才接收

* 全收：`Mask = 0`
* 精确收一个 ID：`Mask = 0x7FF`（11 位全比较）

标准帧 11 位 ID 在 32 位过滤器里放在 `FilterIdHigh` 的 bit15~bit5，要左移 5 位：

```c
filter.FilterIdHigh     = (uint16_t)(0x101U << 5);   /* 只收 0x101 */
filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);   /* 11 位全比较 */
```

### 2.4 协议帧 ID 分配（三节点约定）

| ID | 方向 | 内容 |
|---|---|---|
| 0x100 | Master → 总线 | 主控心跳（Day15 已有） |
| 0x101 | Master → Node1 | 命令 |
| 0x102 | Node1 → Master | 传感器数据（温度） |
| 0x201 | Master → Node2 | 命令 |
| 0x202 | Node2 → Master | 电机状态（速度） |
| 0x301 | Node → 总线 | 节点心跳（Day18 用） |

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6 最小系统板（Master，Day15 固件）
* STM32F103C8T6 最小系统板 ×2（Node1 传感器 / Node2 电机）
* TJA1050 CAN 收发器模块 ×3（每块板一个）
* USB-CAN 分析仪（PCAN-View，500 kbit/s）
* USB-TTL (CH340G)、ST-Link V2

软件：

* STM32CubeIDE 2.2.0 + STM32Cube FW_F1 V1.8.7
* PCAN-View

工程：`STM32-CAN-Industrial-Control-System/firmware/sensor_node`（Node1）、`firmware/motor_node`（Node2）

当前阶段：Phase 3 CAN 通信系统（Day 16）

---

## 4. F103 工程搭建（sensor_node / motor_node）

基于本地 `f103_blink_node1/node2` 扩展：CAN1（PA11/PA12，500k）+ USART1（PA9/PA10，115200）+ FreeRTOS（CMSIS_V1）。

### 4.1 CAN 外设初始化（can.c）

```c
hcan1.Instance = CAN1;
hcan1.Init.Prescaler = 4;
hcan1.Init.Mode = CAN_MODE_NORMAL;
hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
hcan1.Init.TimeSeg1 = CAN_BS1_15TQ;   /* 36MHz / 4 / (1+15+2) = 500k */
hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;

/* MSP：时钟 + GPIO(PA11/PA12) + AFIO 重映射 + NVIC(RX0 优先级 5) */
__HAL_RCC_CAN1_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_CAN1_1();
```

### 4.2 应用层（app_can.c，Node1 版）

```c
/* 启动：精确过滤 0x101 + 开启 RX0 中断 */
filter.FilterIdHigh     = (uint16_t)(CAN_CMD_ID << 5);
filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);
...
HAL_CAN_ConfigFilter(&hcan1, &filter);
HAL_CAN_Start(&hcan1);
HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | ...);

/* 发送：0x102 数据帧（temp 小端） */
tx.StdId = CAN_DATA_ID;  /* 0x102 */
tx.DLC   = 2;
HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox);

/* 接收：RX0 中断 → 队列 → CanRxTask（ISR 安全投递） */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, frame.data);
    frame.id  = rx.StdId;
    frame.dlc = rx.DLC;
    xQueueSendFromISR(xCanRxQueue, &frame, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

Node2 与 Node1 同构，协议改为收 0x201、发 0x202（速度 0~1000 递增）。

---

## 5. FreeRTOS 任务结构（freertos.c）

```text
CanRxTask (P1)：xQueueReceive → 打印 [NODE1] RX cmd=...
CanTxTask (P0)：CanStart() → 每 1s 发 [NODE1] TX 0x102 temp=...
defaultTask (P0)：LED PC13 1s 闪烁 + 启动打印
```

静态分配：`vApplicationGetIdleTaskMemory` / `vApplicationGetTimerTaskMemory`；
钩子：`vApplicationMallocFailedHook` / `vApplicationStackOverflowHook`。

---

## 6. 实验过程（Experiment）

### 实验 1：两个 F103 工程命令行编译

现象：`sensor_node` 与 `motor_node` 均编译通过（0 errors, 0 warnings）。

资源占用（Debug）：

* Node1：text 32644 B / bss 12500 B（Flash 64KB / RAM 20KB 内）
* Node2：text 32660 B / bss 12500 B

验证：stm32cubeidec 无头模式构建成功。

### 实验 2：F407 + USB-CAN 基线（阶段 B）

现象：PCAN-View 每秒收到 ID=100h；F407 串口 `[CAN_TX] id=0x100 seq=...`，无 LEC=3。

验证：总线与分析仪链路正常，进入两节点联调。

### 实验 3：F407 ↔ Node1 两节点联调（阶段 C，未通过）

现象：Node1 串口持续 `[NODE1] TX FAIL st=1 LEC=0 TEC=0 REC=0 State=5`；PCAN-View 只有 100h。

验证：未通过。诊断见 §7 Problem 3。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：F103 的 CAN 信号没到 PA11/PA12

问题：Node1 发送 `add mailbox failed`，PCAN 收不到 0x102。

原因：F103 的 CAN1 默认引脚是 PB8/PB9，PA11/PA12 需要 AFIO 重映射（`__HAL_AFIO_REMAP_CAN1_1()`），can.c 漏了。

解决：MSP 里补 `__HAL_RCC_AFIO_CLK_ENABLE()` + `__HAL_AFIO_REMAP_CAN1_1()`。

### Problem 2：F1 HAL 错误中断把 State 锁死成 ERROR

问题：诊断显示 `State=5`，`HAL_CAN_AddTxMessage` 持续返回 HAL_ERROR。

原因：F1 HAL 在错误中断触发时把 `hcan->State` 置为 `HAL_CAN_STATE_ERROR(5)` 且不自动恢复（F4 HAL 无此行为，所以 F407 正常）。

解决：新增 `HAL_CAN_ErrorCallback`，打印错误码并手动恢复 `hcan->State = HAL_CAN_STATE_READY`。

### Problem 3（未解决）：CAN 卡在初始化模式（INAK=1）

问题：重映射修复后仍 `TX FAIL st=1 State=5`，且无 CAN_ERR 打印。

诊断数据：

```text
BTR=0x001E0003   → 位时序正确（PSC=4, BS1=15, BS2=2），时钟/初始化成功
MCR.INRQ=0       → 已请求退出初始化模式
MSR.INAK=1       → 硬件确认仍在初始化模式，退不出
TSR=0x1C000000   → 邮箱全空（TME0/1/2=1），但 HAL 因 State=ERROR 拒绝发送
```

原因分析：`HAL_CAN_Start` 等待 INAK 清除超时 → State=ERROR → 后续发送全失败。INAK 卡死指向 Node1 侧 TJA1050 收发器/接线异常（VCC 是否 5V、TXD 是否虚接、模块本身坏），而非代码——因为 BTR 证明 CAN 外设初始化正常。

待验证：换一块 TJA1050 模块；万用表量 VCC-GND=5V；拔掉 CANH/CANL 单独跑看 State 是否恢复 2。

---

## 8. 今日成果（Result）

完成：

* [x] 搭建 Node1（sensor_node）F103 工程：CAN1 500k + USART1 + FreeRTOS，编译通过
* [x] 搭建 Node2（motor_node）F103 工程：收 0x201 / 发 0x202，编译通过
* [x] 掌握 F103 波特率重算（APB1=36MHz）与 AFIO 重映射
* [x] Node 侧过滤器精确过滤（Mask=0x7FF）
* [x] 定位 F1 HAL State=ERROR 不恢复的坑并修复
* [ ] F407 ↔ Node1 两节点联调（被 Node1 收发器/硬件问题阻塞，挂起）

当前三节点架构：

```text
         CAN 总线（CANH/CANL，500 kbit/s）
    ┌───────────┬───────────┬───────────┐
    │           │           │           │
 F407 Master  F103 Node1  F103 Node2  USB-CAN
  0x100 心跳  0x102 数据  0x202 数据   分析仪
  收 0x102/   收 0x101    收 0x201
  0x202
```

---

## 9. 工程总结（Engineering Summary）

## F1 与 F4 的 HAL 行为差异

同一套 HAL API，不同系列行为可能不同：F1 的 CAN 错误中断会锁死 State，F4 不会；F1 的引脚复用需要 AFIO 重映射，F4 直接用复用功能表。移植代码时必须查对应系列的参考手册与 HAL 源码。

## 寄存器诊断优于"现象猜测"

`add mailbox failed` 这类 HAL 层错误信息不够用，直接读外设寄存器（BTR/MSR/TSR/ESR）能快速区分：时钟问题、初始化问题、还是总线错误。BTR=0x001E0003 直接证明配置正确，把嫌疑从代码转移到硬件。

---

## 10. 面试问答（Interview Prep）

### Q1：F103 和 F407 都是 500 kbit/s，CAN 配置参数为什么不同？

答题要点：

* 波特率由 APB1 时钟决定：F407=42MHz，F103=36MHz
* 公式 `波特率 = APB1 / (Prescaler × (1+BS1+BS2))`，参数必须按时钟重算
* 采样点 = (1+BS1)/(1+BS1+BS2)，一般取 75%~87.5%

### Q2：F103 用 PA11/PA12 做 CAN 为什么需要重映射？

答题要点：

* F103 的 CAN1 默认引脚是 PB8/PB9
* PA11/PA12 是重映射选项，需 `__HAL_AFIO_REMAP_CAN1_1()` + AFIO 时钟
* F407 无此机制，PA11/PA12 直接是 CAN1 复用功能

### Q3：HAL_CAN_AddTxMessage 返回 HAL_OK 就代表帧发出去了吗？

答题要点：

* 只代表帧进入发送邮箱，真正发出要看发送完成中断/邮箱状态
* F1 HAL 的 State 可能因错误中断锁死，需在 ErrorCallback 恢复
* 检查 `hcan->State` 与 ESR 错误计数是排查发送问题的第一手信息

---

## 11. Git 提交

```bash
git add .
git commit -m "feat: Day16 F103 node CAN (sensor_node/motor_node projects, AFIO remap, F1 HAL state fix)"
```

---

## 12. 下一步计划（Next Step）

Day17：

* 换 TJA1050 模块 / 万用表验证 Node1 收发器供电，打通两节点联调
* Node2 上线，三节点互发互收（100h / 102h / 202h）
* F407 过滤器改为精确过滤（只收 0x102 / 0x202，两个 filter bank）

为 Day18 心跳检测（0x301）与节点离线检测建立基础。
