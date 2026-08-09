# STM32-CAN-Industrial-Control-System

基于 STM32 + FreeRTOS + CAN 总线的三节点工业控制系统原型，模拟工业现场"传感器采集 → 主控决策 → 执行器响应"的分布式控制链路。

系统以 STM32F407 为主控节点，两个 STM32F103 分别作为传感节点和执行节点，覆盖实时任务调度、CAN 总线通信、电机 PID 闭环控制、故障监测等工业嵌入式核心环节，并配套 STM32 IAP 固件升级系统。

---

## 1. 系统架构

```
                        CAN Bus (500 kbps)

            STM32F407 Master（主控调度）
                         │
            ┌────────────┴────────────┐
            │                         │
    STM32F103 Sensor Node    STM32F103 Motor Node
    （MPU6050 数据采集）      （PWM + 编码器 + PID）
```

**功能链路**：

```
传感器采集 → CAN 通信 → 主控决策 → CAN 指令 → 电机执行 → 编码器反馈 → PID 闭环
```

**节点职责**：

| 节点 | 硬件 | 职责 |
|---|---|---|
| Master | STM32F407VET6 | 任务调度、CAN 通信管理、数据处理、故障监控 |
| Sensor Node | STM32F103C8T6 | MPU6050 数据采集、CAN 上报 |
| Motor Node | STM32F103C8T6 | PWM 电机驱动、编码器反馈、PID 速度闭环 |

---

## 2. 软件架构

```
Application Layer（应用逻辑）
        │
FreeRTOS Task Layer（任务调度）
        │
  HAL Driver Layer（外设驱动）
        │
   STM32 Hardware
```

**FreeRTOS 任务规划**：

```
FreeRTOS Scheduler
      │
      ├── CAN Receive Task    — 接收 CAN 报文，按 ID 分发
      ├── CAN Transmit Task   — 发送控制指令
      ├── Control Logic Task  — 传感器融合 + 决策
      ├── Fault Monitor Task  — 心跳检测 + 离线告警
      └── UART Debug Task     — 状态输出
```

任务间通过 **Queue** 传递数据，中断通过 **Semaphore** 通知任务（中断不处理业务），共享资源（如 UART）通过 **Mutex** 保护。

---

## 3. 技术栈

| 分类 | 内容 |
|---|---|
| MCU | STM32F407VET6、STM32F103C8T6 |
| RTOS | FreeRTOS（CMSIS_V1） |
| 通信 | CAN 总线（500 kbps），自定义应用层协议 |
| 控制 | PWM 输出、编码器反馈、PID 速度闭环 |
| 传感器 | MPU6050 六轴姿态（I2C） |
| 工具 | STM32CubeMX、STM32CubeIDE、ST-Link V2、Git |

---

## 4. 开发进度

### Phase 1：STM32 基础工程能力 ✅ 已完成

- STM32CubeMX 工程创建与 HAL 库开发流程
- GPIO / UART / Timer / Interrupt 基础
- 三块板裸机点灯与串口通信验证

### Phase 2：FreeRTOS 实时系统 🔄 进行中

- ✅ FreeRTOS 中间件集成与多任务调度
- ✅ 双任务/三任务验证（LED 翻转 + 串口状态输出）
- ✅ 任务通信（Queue，Producer-Consumer 模型）
- ✅ 同步与资源保护（Semaphore + Mutex）
- ✅ 软件定时器、中断安全 API、临界区保护
- ✅ 内存管理（heap_1 ~ heap_5 对比 + heap_4 合并实验）
- ✅ 任务优先级与优先级反转（信号量 vs 互斥锁对照实验）
- ✅ Event Group 事件组（AND/OR 多条件等待）+ 任务通知
- ⏳ 异常检测调试 + 综合架构设计
- ⏳ CAN 协议理论 + bxCAN 初始化（Loopback 自测）

### Phase 3：CAN 通信系统 ⏳ 规划中

- CAN 初始化、报文收发、中断处理
- 自定义 CAN 应用层协议（帧 ID 分配）
- 心跳检测与节点离线识别
- 故障管理（超时检测、自动重连、总线错误计数）

### Phase 4：电机控制闭环 ⏳ 规划中

- PWM 电机驱动（L298N）
- 编码器反馈采集（TIM 编码器模式）
- PID 速度闭环控制

### Phase 5：IAP Bootloader ⏳ 规划中

- Flash 分区管理与向量表偏移
- CRC32 校验与固件升级协议
- Python 上位机升级工具

### Phase 6：项目整理 ⏳ 规划中

- 代码重构与文档完善
- 系统联调与演示视频

---

## 5. 自定义 CAN 协议（规划）

```c
#define ID_CMD_NODE1     0x101   // Master → Node1 命令
#define ID_CMD_NODE2     0x201   // Master → Node2 命令
#define ID_DATA_NODE1    0x102   // Node1 → Master 传感器数据
#define ID_DATA_NODE2    0x202   // Node2 → Master 电机状态
#define ID_HEARTBEAT     0x301   // 心跳帧（1s 周期，5s 无心跳判离线）
```

---

## 6. 项目结构

```
STM32-CAN-Industrial-Control-System
├── firmware/
│   ├── master_node/        # F407 主控工程（FreeRTOS + HAL）
│   ├── sensor_node/        # F103 传感器节点工程
│   └── motor_node/         # F103 电机节点工程
├── docs/                   # 学习记录与协议文档
├── hardware/               # 硬件接线说明
├── 学习进度.md             # 每日学习进度跟踪
└── README.md
```

---

## 7. 学习记录

每日学习记录见 [docs/](docs/)，按 [Day 记录模板](docs/day_template.md) 编写：

- [Day 1：STM32 开发环境搭建](docs/day01.md)
- [Day 2：GPIO + UART 通信](docs/day02.md)
- [Day 3：UART 调试系统](docs/day03.md)
- [Day 4：Timer 定时器与中断](docs/day04.md)
- [Day 5：FreeRTOS 基础移植](docs/day05.md)
- [Day 6：FreeRTOS 任务管理与调度](docs/day06.md)
- [Day 7：FreeRTOS Queue 任务通信](docs/day07.md)
- [Day 8：FreeRTOS 同步机制（Semaphore & Mutex）](docs/day08.md)
- [Day 9：FreeRTOS 软件定时器与临界区保护](docs/day09.md)
- [Day 10：FreeRTOS 内存管理（heap_1 ~ heap_5）](docs/day10.md)
- [Day 11：FreeRTOS 任务设计与优先级管理](docs/day11.md)
- [Day 12：FreeRTOS 事件组与任务通知](docs/day12.md)
- [Day 13：FreeRTOS 异常检测与调试](docs/day13.md)
- [Day 14：FreeRTOS 综合架构设计（Phase 2 收官）](docs/day14.md)

---

## 8. 硬件清单

核心硬件：STM32F407VET6 最小系统板 ×1、STM32F103C8T6 最小系统板 ×2、TJA1050 CAN 收发器 ×3、ST-Link V2、USB-TTL（CH340G）。

传感器与执行器：MPU6050、直流减速电机 + 霍尔编码器、L298N 电机驱动、12V 电源。

调试工具：USB-CAN 分析仪。
