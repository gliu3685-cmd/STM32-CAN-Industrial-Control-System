# CAN 代码与原理详解（Day 15-21 全链路）

> 目的：面试/复习用的 CAN 实现全讲解。从外设初始化到应用层任务，按五层结构走完三节点的全部 CAN 代码。

---

## 1. 总图：每个节点的三层软件结构

三块板子的 CAN 代码结构完全对称，只是参数和职责不同：

```text
应用层    app_arch.c / app_can.c 发送任务、接收任务、命令应答（FreeRTOS）
应用驱动  app_can.c  CanStart / 发送函数 / 中断回调（手写核心）
外设层    can.c     HAL_CAN_Init + GPIO + 中断（CubeMX 生成 + 手动修）
```

物理层（板子外面）：TJA1050 收发器把 MCU 的 TX/RX 数字电平转成 CANH/CANL 差分信号，再挂到 500kbps 总线上。

---

## 2. 第一层：外设初始化（can.c）

### 2.1 时钟与引脚（MSP）

F407 与 F103 的最大差异：

```c
/* F407：PA11=RX、PA12=TX 都配 AF_PP + AF9 */
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;

/* F103：PA11=RX 必须配输入模式 + 上拉；PA12=TX 配推挽输出 */
GPIO_InitStruct.Pin  = GPIO_PIN_11;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;   /* 关键！不是 AF_PP */
GPIO_InitStruct.Pull = GPIO_PULLUP;
```

原理：F4 有"复用功能选择寄存器"，一个引脚可以按 AF9 路由到 CAN 外设，所以 RX/TX 都能用 AF_PP。F1 **没有**这个寄存器，CAN_RX 本质就是普通输入引脚——照抄 F4 的 AF_PP 会导致 CAN 内核读不到 RXD 电平，初始化卡死（Day 17 修复）。

隐藏细节：F103 的 can.c 里 PA12 其实没显式设 Mode（继承了 PA11 的输入模式），真正生效的推挽输出是 `app_can.c` 的 `CanStart()` 里重新初始化 PA12 时配的。系统能跑是因为 CanStart 在收发前先执行。属于"能工作但不干净"的代码，可清理。

### 2.2 内核参数（HAL_CAN_Init）

F407 与 F103 位时序不同，但波特率都是 500k：

| 参数 | F407 | F103 | 说明 |
|---|---|---|---|
| Prescaler | 6 | 4 | 分频 |
| BS1 | 11 | 15 | 相位段 1 |
| BS2 | 2 | 2 | 相位段 2 |
| 波特率 | 42M/6/14=500k | 36M/4/18=500k | 一致即可通信 |
| 采样点 | 85.7% | 88.9% | 短总线兼容 |

配置差异备注：

* F407 `AutoBusOff=DISABLE`（总线关闭后不自动恢复，需软件处理，遗留改进点）；F103 `ENABLE`
* F407 `AutoRetransmission=DISABLE`（NART=1，发送出错不自动重发）

---

## 3. 第二层：过滤器与启动（CanStart）

过滤器是"哪几帧进我接收 FIFO"的门卫，掩码模式公式：

```text
(收到ID & Mask) == (Filter & Mask)  → 接收，否则丢弃
```

```c
filter.FilterIdHigh     = (uint16_t)(0x102U << 5);   /* 目标 ID 左移 5 位 */
filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);   /* 掩码全 1 = 精确匹配 */
filter.FilterBank       = 0U;                        /* F407: Bank0 收 0x102 */
```

为什么 ID 要 `<< 5`：11 位 ID 在 32 位过滤器寄存器里右对齐到高位，低 5 位不参与比较，需左移 5 位对齐。Mask=0x7FF 表示 11 位全部相等，等价于精确接收。

各节点过滤配置：

| 节点 | 接收 ID | Bank |
|---|---|---|
| F407 主控 | 0x102（Node1 温度）、0x202（Node2 速度） | Bank0 / Bank1 |
| Node1 | 0x101（命令） | Bank0 |
| Node2 | 0x201（命令） | Bank0 |

随后 `HAL_CAN_Start()` 启动外设，`HAL_CAN_ActivateNotification()` 开启 RX0 接收中断 + 错误中断 + 总线关闭中断 + LEC 错误码中断。F103 的 CanStart 还额外清 `DBF` 调试冻结位（Debug 模式下 ST-LINK 会把 CAN 内核冻住）。

---

## 4. 第三层：发送路径

### 4.1 邮箱机制

bxCAN 有 3 个发送邮箱，`HAL_CAN_AddTxMessage()` 把帧放进空邮箱，硬件自动按优先级发出；3 个邮箱都满时返回失败（`add mailbox failed`）。

### 4.2 F407 主控心跳帧

```c
tx.StdId = 0x100U;            /* 11 位标准 ID */
tx.IDE   = CAN_ID_STD;        /* 标准帧 */
tx.RTR   = CAN_RTR_DATA;      /* 数据帧，不是遥控帧 */
tx.DLC   = 8U;                /* 8 字节负载 */
data[0] = seq & 0xFF;         /* 序号低字节 */
data[2] = 0x55; data[3] = 0xAA;  /* 状态字 */
```

帧格式在代码里的映射：`StdId` = 仲裁段、`DLC` = 数据段长度、`IDE/RTR` = 帧类型。命令帧 `CanSendCommand()` 结构相同，只是 ID 换成 0x101/0x201、负载换成"命令码 + 参数"（0x201 的 data[1..2] = 目标速度小端）。

### 4.3 Node1/Node2 数据帧

`CanSendSensorData()` / `CanSendMotorData()` 设 StdId/DLC，把温度或速度拆成小端字节填进 data。

---

## 5. 第四层：接收路径（核心）

### 5.1 中断只投递

F407 收到 0x102/0x202 触发 RX0 中断，回调里做的事非常少：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, frame.data);  /* 从 FIFO 取帧 */
    frame.node_id = (rx.StdId == 0x202U) ? 1U : 0U;             /* ID→节点号映射 */
    xQueueSendFromISR(xCanRxQueue, &frame, &xHigherPriorityTaskWoken);  /* 投递 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);               /* 唤醒任务 */
}
```

原则：ISR 里不打印、不解析、不锁互斥量，只把帧塞进队列就返回。中断会打断所有任务，占用越久影响越大。

### 5.2 任务消费并更新状态表

```c
xQueueReceive(xCanRxQueue, &frame, portMAX_DELAY);
xSemaphoreTake(xSysMutex, portMAX_DELAY);      /* 保护共享状态表 */
gSys.last_rx_tick[frame.node_id] = frame.tick; /* 更新时间戳 = 心跳 */
gSys.node_online[frame.node_id]  = 1;
if (frame.node_id == 0) gSys.node_temp[0] = frame.data[0];
else                    gSys.speed = frame.data[0] | (frame.data[1] << 8);
xSemaphoreGive(xSysMutex);
```

"中断生产、任务消费"：接收处理从中断移到低优先级任务，慢一点没关系，但不能卡中断。

### 5.3 从节点收到命令 → 应答

```c
/* Node1 */
if (frame.id == CAN_CMD_ID && frame.data[0] == 0x01U)  /* 收到"请求上报" */
{
    CanSendSensorData(g_temp);   /* 立即回一帧 0x102 */
}

/* Node2：解析目标速度、更新 g_speed 并回 0x202，速度保持 5 秒 */
if (frame.id == CAN_CMD_ID && frame.data[0] == 0x01U)
{
    g_speed = (uint16_t)(frame.data[1] | ((uint16_t)frame.data[2] << 8));
    g_override_until = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
    CanSendMotorData(g_speed);
}
```

---

## 6. 第五层：FreeRTOS 把 CAN 组织成系统

三个与 CAN 相关的任务配合：

| 任务 | 优先级 | 职责 | 机制 |
|---|---|---|---|
| CanTxTask | 2 | 1s 心跳 + 每 5s 命令 | vTaskDelay 定时 |
| CanRxTask | 4 | 队列消费、更新状态表 | Queue + Mutex |
| FaultMonitorTask | 4 | `now - last_rx_tick > 5000ms` 判离线 | EventGroup + Mutex |

`HeartbeatCallback`（软件定时器 1s）递增 uptime 并置心跳事件——整个系统的"心跳节奏"由 FreeRTOS 定时器打拍子。

---

## 7. 错误处理

* F407 `HAL_CAN_ErrorCallback`：打印 ErrorCode 与 LEC 状态（诊断用）
* F103 `HAL_CAN_ErrorCallback`：**恢复 State 为 READY**——F1 HAL 错误中断后会把状态机锁死为 ERROR，不恢复则发送永远失败（Day 16 教训）
* 错误详情（LEC/TEC/REC）由发送任务每 5s 读 ESR 打印，不在中断里 printf

Day 19 实验 D 中 `LEC=3` 即 ACK 错误：发 0x100 时总线上无人应答，硬件在 ACK 槽检测不到显性位。

---

## 8. 原理 ↔ 代码对照表

| 原理概念 | 代码位置 |
|---|---|
| 波特率/采样点 | can.c `Prescaler/TimeSeg1/TimeSeg2` |
| 帧格式（ID/DLC/RTR/IDE） | `CAN_TxHeaderTypeDef` 字段 |
| 过滤器掩码 | `CanStart()` Filter 寄存器，ID 左移 5 位 |
| 非破坏性仲裁 | 硬件自动（bxCAN 引擎） |
| 错误计数器 TEC/REC | ESR 寄存器，任务周期读取 |
| 总线关闭恢复 | `AutoBusOff`（F103 开 / F407 关） |
| 中断最小化 | RX0 回调只做 `xQueueSendFromISR` |
| 数据新鲜度 | `last_rx_tick` + 5s 超时 |
| 请求/应答 | 0x101/0x201 命令 + 立即应答 |

---

## 9. 面试追问三连

### Q1：为什么接收要用队列而不是直接在中断里处理？

答题要点：

* 中断会抢占所有任务，处理越久系统抖动越大
* 队列让中断只花几微秒投递，解析与业务交给任务
* 任务可阻塞挂起，不空转浪费 CPU

### Q2：F1 和 F4 的 CAN 配置为什么不一样？

答题要点：

* F1 无 AF 选择寄存器，CAN_RX 必须配普通输入模式
* F4 有 AF 选择寄存器，RX/TX 都能配 AF_PP
* 移植时照抄配置是最常见的坑

### Q3：怎么判断是节点故障还是总线故障？

答题要点：

* 单节点离线：仅该节点超时，主控收发正常
* 总线/主控故障：主控发帧报 ACK 错误（LEC=3）、所有节点一起离线
* 靠错误计数与超时的交叉信息定位

---

## 10. 相关文件

* `firmware/master_node/Core/Src/app_can.c`：F407 过滤器/心跳/命令帧/接收回调
* `firmware/master_node/Core/Src/app_arch.c`：RTOS 任务架构与离线检测
* `firmware/sensor_node/Core/Src/app_can.c`：Node1 温度上报与命令应答
* `firmware/motor_node/Core/Src/app_can.c`：Node2 速度上报与命令应答
* `firmware/*/Core/Src/can.c`：CAN 外设初始化（位时序/引脚/中断）
