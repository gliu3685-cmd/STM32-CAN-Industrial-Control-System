# Day15 - CAN 通信（F407 外设配置与真实收发验证）

## 1. 今日目标（Objectives）

本日进入 **Phase 3：CAN 通信系统**，重点掌握：

* 理解 CAN 协议核心：多主总线、ID 仲裁、差分信号、帧结构、终端电阻
* 配置 F407 CAN1 外设：500 kbit/s、PA11/PA12、RX0 中断接收
* 用 USB-CAN 分析仪（PCAN-View）完成真实 CAN 双向收发验证

---

## 2. 理论学习（Theory）

### 2.1 CAN 是什么

CAN（Controller Area Network）是博世 1986 年提出的**多主串行总线协议**，广泛用于汽车与工业现场。核心特性：

* **多主**：总线上任何节点都能主动发数据，没有固定主从
* **广播式**：一帧发出，所有节点都能收到，靠 ID 仲裁决定谁先发
* **差分信号**：CANH/CANL 两根线传输，共模抑制强，抗干扰
* **自愈**：出错帧自动重发，无需软件参与

对比 UART：UART 是点对点、要约定谁先说话；CAN 是一对多、谁有数据谁抢总线，天然适合多节点工业通信。

### 2.2 帧结构（标准帧 11 位 ID）

```
SOF | 仲裁段(11位ID + RTR) | 控制段(DLC) | 数据段(0~8B) | CRC | ACK | EOF
```

* **ID**：帧的门牌号，同时决定仲裁优先级，**数字越小优先级越高**
* **DLC**：数据长度，最多 8 字节
* **Data**：真正的数据负载
* **仲裁**：两节点同时发送时，ID 小的先出现显性位获胜，输的节点自动重发

### 2.3 TJA1050 收发器与终端电阻

MCU 的 CAN 控制器输出 TTL 电平，不能直接上总线。TJA1050 负责电平转换：

```
STM32 CAN_TX → TJA1050 TXD → CANH/CANL 差分信号
STM32 CAN_RX ← TJA1050 RXD ← 差分信号还原
```

总线**两端各一个 120Ω 终端电阻**，与线缆特征阻抗匹配，吸收信号反射。接多了总线负载过重，接少了长距离出错。

### 2.4 波特率计算

位时间 = (1 + BS1 + BS2) × Prescaler / APB1

F407 APB1 = 42 MHz，目标 500 kbit/s：

```
42 MHz ÷ 500k = 84 个时间量子/位
Prescaler=6 → 84÷6 = 14 tq：Sync 1 + BS1 11 + BS2 2 = 14
采样点 = (1+11)/14 ≈ 85.7%（推荐 75%~87.5%）
```

CubeMX 参数：Prescaler=6、BS1=11TQ、BS2=2TQ、SJW=1TQ。

### 2.5 过滤器

CAN 控制器硬件过滤，减轻 CPU 负担：

* **掩码模式**：掩码位为 1 必须匹配，为 0 不关心；掩码全 0 = 全接收
* **列表模式**：精确匹配固定 ID

验证阶段先全接收，后续按节点 ID 精确过滤。

### 2.6 中断接收

```
CAN1_RX0 中断 → HAL_CAN_RxFifo0MsgPendingCallback
  → xQueueSendFromISR(接收队列) + xEventGroupSetBitsFromISR
  → CanRxTask 处理（打印/更新共享状态）
```

ISR 只做最少的事，重活交给任务——嵌入式标准模式。

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6 最小系统板（Master）
* TJA1050 CAN 收发器模块 ×1
* USB-CAN 分析仪（深圳尚信物联，PCAN 固件）+ PCAN-View
* ST-Link V2、USB-TTL（CH340）

软件：

* STM32CubeIDE 2.2.0、PCAN-View、SSCOM

工程：`firmware/master_node`

当前阶段：第三阶段 CAN 通信系统

接线：

```
F407 PA12(CAN1_TX) → TJA1050 TXD
F407 PA11(CAN1_RX) → TJA1050 RXD
F407 5V → VCC，GND → GND
TJA1050 CANH → 分析仪 CAN_H
TJA1050 CANL → 分析仪 CAN_L
GND 共地（分析仪 GND ↔ F407 GND）
终端电阻：分析仪内置 120Ω + F407 端外接 120Ω（短距离可先只用一个）
```

---

## 4. 功能 1 实现：CubeMX CAN1 配置

在 CubeIDE 打开 `.ioc`：

1. Connectivity → CAN1 → **Activated**（引脚自动分配 PA11/PA12）
2. Parameter Settings：
   * Prescaler = **6**
   * Time Quanta in Bit Segment 1 = **11**
   * Time Quanta in Bit Segment 2 = **2**
   * ReSynchronization Jump Width = **1**
3. NVIC：使能 **CAN1 RX0 interrupt**，抢占优先级 **5**（FreeRTOS 中断安全阈值）
4. 生成代码，得到 `can.c/can.h`，main.c 自动调用 `MX_CAN1_Init()`

生成的核心配置（can.c）：

```c
hcan1.Init.Prescaler     = 6;
hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
hcan1.Init.TimeSeg1      = CAN_BS1_11TQ;
hcan1.Init.TimeSeg2      = CAN_BS2_2TQ;
```

---

## 5. 功能 2 实现：应用层 CAN 驱动与架构集成

### 5.1 app_can.c / app_can.h

`CanStart()`：配置过滤器（掩码模式全接收）+ 启动外设 + 开启接收/错误中断通知。

```c
filter.FilterMode       = CAN_FILTERMODE_IDMASK;
filter.FilterScale      = CAN_FILTERSCALE_32BIT;
filter.FilterActivation = CAN_FILTER_ENABLE;   /* 注意：新 HAL 是 ENABLE 不是 ACTIVATE */
HAL_CAN_ConfigFilter(&hcan1, &filter);
HAL_CAN_Start(&hcan1);
HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR ...);
```

`CanSendHeartbeat()`：发送标准帧 ID=0x100，8 字节 `[seq_lo, seq_hi, 0x55, 0xAA, 0...]`。

接收中断回调：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx;
    CanFrame_t frame;
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, frame.data);
    frame.node_id = rx.StdId;
    frame.dlc     = rx.DLC;
    frame.tick    = xTaskGetTickCountFromISR();
    ArchPostCanFrame(&frame);   /* ISR 安全投递 */
}
```

### 5.2 app_arch.c 集成

* `ArchPostCanFrame()`：`xQueueSendFromISR` + `xEventGroupSetBitsFromISR` + `portYIELD_FROM_ISR`
* `CanTxTask`：每秒调用 `CanSendHeartbeat(seq++)` 真实发送
* `CanRxTask`：从队列取帧打印 `[CAN_RX]`
* `CanStart()` 在 ArchDemoTask 中调用
* 错误诊断：发送后读 `ESR` 寄存器（LEC/TEC/REC），不依赖错误中断也能定位总线故障

---

## 6. 实验过程（Experiment）

### 实验 1：F407 → PCAN-View（发送验证）

现象：

```
串口：[CAN_TX] id=0x100 seq=0 / seq=1 / seq=2 ...
PCAN-View Trace：CAN-ID 100h，Length 8，数据 2F 55 AA 00 00 00 00 00 ...
```

验证：PCAN-View 每秒收到一帧 ID=0x100，首字节序号递增、`55 AA` 魔数正确，**发送方向端到端打通**。

### 实验 2：PCAN-View → F407（接收验证）

现象：

```
PCAN-View Transmit：ID=200h，DLC=8，数据 11 01 20 00 ...
串口：[CAN_RX] node=512 dlc=8 data=11 01 20 00
```

验证：512 = 0x200，正是 PCAN-View 发送的帧 ID，**接收方向打通**。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：烧录后 LED 不亮、串口无输出（耗时最长）

问题：Day15 固件烧录成功，但程序完全没跑起来。

原因：**CubeMX 重新生成时把时钟源从 HSE（外部 8M 晶振）悄悄改成了 HSI（内部 16M），但 PLLM 仍为 8**。HSI 16M÷8=2M 输入 × PLLN=336 = **672MHz VCO，远超 F407 上限 432MHz**，PLL 锁不住 → `HAL_RCC_OscConfig` 超时 → `Error_Handler()` 死循环 → 在 printf 之前卡死。

解决：恢复 HSE 时钟（PLLM=8、PLLN=336、PLLP=2、PLLQ=7 → 168MHz），并修正 `.ioc` 中 16 个时钟值。

教训：**每次 CubeMX 重新生成后，检查 Clock Configuration 页是否为 HSE + 168MHz**；关键配置放 USER CODE 保护区防丢失。

### Problem 2：CAN 分析仪把总线拉乱

问题：分析仪连上后，F407 出现 `LEC=5`（想发隐性位却看到显性位）、`REC=248` 接近 bus-off。

原因：分析仪处于卡死/持续发送状态（换线过程中状态异常或 PCAN-View 周期发送干扰）。

解决：断电重启分析仪、停掉 PCAN-View 周期发送后恢复正常。

### Problem 3：无应答现象的典型特征

问题：单节点总线（无第二节点）发送时出现 `LEC=3 TEC=0 REC=8,16,24...`。

原因：CAN 规范中 ACK 错误应计入 TEC，但 STM32 bxCAN 将 ACK 错误计入 **REC（接收错误计数）**，这是已知特性；`LEC=3` 表示帧发出去了但没人应答。

验证：该现象恰好证明发送链路正常，问题只缺一个应答节点。

### Problem 4：编译与生成问题

* `CAN_FILTER_ACTIVATE` → 新版 HAL 是 `CAN_FILTER_ENABLE`
* `HAL_CAN_RX_FIFO0_MSG_PENDING` → 新版 HAL 是 `CAN_IT_RX_FIFO0_MSG_PENDING`
* CubeMX 重新生成把 main.c 结尾多生成一层 `}` → 在 USER CODE 保护区补回 `while(1){`
* CubeMX 重新生成丢失 `configUSE_TIMERS` 与 `vApplicationGetTimerTaskMemory` → 全部放回 USER CODE 保护区并显式定义 `configTIMER_*`

### Problem 5：串口打印交错

问题：`[DEBUG]` 与 `[CAN_TX]` 打印串行。

原因：`app_can.c` 的打印直接 `printf`，没走 UART 互斥锁。

解决：统一使用 `ArchPrint()`（互斥锁保护）；ISR 回调内不能用锁，错误打印保留直接 `printf`。

---

## 8. 今日成果（Result）

完成：

* [x] CAN 理论：帧结构、仲裁、差分信号、波特率、终端电阻
* [x] F407 CAN1 500 kbit/s 配置（PA11/PA12、RX0 中断优先级 5）
* [x] F407 → PCAN-View 发送验证（ID=0x100 心跳帧）
* [x] PCAN-View → F407 接收验证（ID=0x200）
* [x] CAN 错误诊断：ESR 寄存器读取（LEC/TEC/REC）

当前架构：

```
CanTxTask(1s) --HAL_CAN_AddTxMessage--> CAN1 --> TJA1050 --> 总线 --> PCAN-View
PCAN-View --CAN帧--> TJA1050 --> CAN1_RX0中断 --> 队列+事件 --> CanRxTask
```

---

## 9. 工程总结（Engineering Summary）

## ISR 安全投递

中断里只能用 `*FromISR` 结尾的 FreeRTOS API，并通过 `portYIELD_FROM_ISR` 触发任务切换；接收帧的解析、打印等重活放在任务中。

## 共享资源互斥

多任务打印共用 UART 必须加互斥锁（`ArchPrint`），否则输出交错；ISR 上下文禁用互斥锁。

## 错误诊断三板斧

* LEC=3：ACK 错误（没人应答）
* LEC=5：位错误（总线被拉成显性/接线异常）
* REC 接近 256：即将 bus-off，需要排查总线硬件

---

## 10. 面试问答（Interview Prep）

### Q1：CAN 总线上两个节点同时发送怎么办？

答题要点：

* 逐位仲裁：ID 小的节点在仲裁段先出现显性位获胜
* 输的节点停止发送，等总线空闲自动重发
* 全程硬件完成，无需主机协调，所以 CAN 支持多主

### Q2：为什么 CAN 用差分信号？

答题要点：

* CANH/CANL 传输互补信号，接收端只关心电压差
* 干扰同时作用在两根线上，差值基本不变（共模抑制）
* 适合工业现场长距离、强干扰；UART 单端信号抗干扰弱

### Q3：波特率怎么算？

答题要点：

* 位时间 = (1+BS1+BS2) × Prescaler / APB1
* 例：APB1=42MHz，Prescaler=6、BS1=11、BS2=2 → 42M/(6×14)=500k
* 采样点 (1+BS1)/(1+BS1+BS2) 控制在 75%~87.5%

### Q4：120Ω 终端电阻的作用？

答题要点：

* 总线两端各一个，等效 60Ω，与线缆特征阻抗匹配
* 吸收信号反射，防止波形畸变
* 短距离低速时单个也能工作，长距离/高速必须两端都有

### Q5：CAN 发送没有节点应答会怎样？

答题要点：

* 帧发完收不到 ACK → 触发 ACK 错误（LEC=3）
* 错误计数器累加，超过 127 进入 Error Passive，超过 255 进入 Bus-Off
* Bus-Off 后节点退出总线，需要软件恢复（如 128×11 个隐性位后复位）

### Q6：中断里如何把数据交给任务？

答题要点：

* 使用 `xQueueSendFromISR` / `xEventGroupSetBitsFromISR` 等 FromISR API
* 通过参数传出"是否需要切换"，返回后调 `portYIELD_FROM_ISR`
* ISR 不做解析/打印等耗时操作

---

## 11. Git 提交

```bash
git add .
git commit -m "feat: Day15 CAN communication (F407 CAN1 500kbps, PCAN-View bidirectional verification)"
```

---

## 12. 下一步计划（Next Step）

Day16：

* F103 节点 CAN 收发任务（Node1/Node2 配置 CAN + TJA1050）
* 三节点双向通信验证（Master ↔ F103）
* 引入 CAN 过滤器按 ID 精确过滤

为 Day17 应用层协议（帧 ID 规划）建立基础。
