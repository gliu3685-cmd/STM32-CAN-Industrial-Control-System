建议你的根目录 README.md 更新成下面这样：

# STM32-CAN-Industrial-Control-System

基于 STM32 + FreeRTOS + CAN 总线的三节点工业控制系统。

本项目旨在模拟工业现场中的分布式控制系统架构，
通过 STM32 微控制器搭建主控节点、传感节点和执行节点，
实现实时任务调度、CAN通信、电机闭环控制以及故障监测等功能。

项目按照工业嵌入式开发流程进行设计，包括：
- 硬件抽象层开发
- RTOS实时任务管理
- 通信协议设计
- 控制算法实现
- 系统调试与工程文档管理


---

# 1. 项目目标

构建一个具有工程实践价值的三节点 CAN 工业控制系统：

             CAN Bus

    +----------------+
    |                |
    |                |

STM32F407 Master Node

    |
    |

+------+------+

| |

Sensor Node Motor Node

STM32F103 STM32F103


系统功能：

- 主控节点进行任务调度和系统管理
- 传感节点采集现场数据
- 电机节点执行控制任务
- CAN总线完成节点间通信
- FreeRTOS负责实时任务管理


---

# 2. Hardware Architecture

## Master Node

MCU:

- STM32F407VET6

Responsibilities:

- System scheduling
- CAN communication
- Data processing
- Fault monitoring


## Sensor Node

MCU:

- STM32F103C8T6

Responsibilities:

- Sensor data acquisition
- Data packaging
- CAN transmission


## Motor Node

MCU:

- STM32F103C8T6

Responsibilities:

- Motor control
- PWM generation
- Encoder feedback


---

# 3. Software Architecture

当前软件架构：


Application Layer

|
|

FreeRTOS Task Layer

|
|

HAL Driver Layer

|
|

STM32 Hardware



## FreeRTOS Task Architecture

规划任务：


FreeRTOS Scheduler

    |

+--------------------+

CAN Receive Task

Sensor Task

Motor Control Task

Fault Monitor Task

UART Debug Task

+--------------------+



---

# 4. Technical Stack


## MCU

- STM32F407VET6
- STM32F103C8T6


## Development Tools

- STM32CubeMX
- STM32CubeIDE
- ST-Link V2
- Git / GitHub


## Firmware Framework

### STM32 HAL Library

学习内容：

- HAL库架构
- GPIO初始化
- UART通信
- Timer配置
- Interrupt机制
- Peripheral Driver调用流程


### FreeRTOS

学习内容：

- RTOS基本概念
- Scheduler任务调度
- Task创建与管理
- Task Priority
- Tick机制
- Delay机制
- 多任务系统设计


### Communication

计划：

- CAN Bus
- CAN Application Protocol
- Heartbeat Detection
- Node Offline Detection


### Control Algorithm

计划：

- PWM Motor Control
- Encoder Feedback
- PID Speed Closed-loop Control


---

# 5. Development Progress


## Phase 1: STM32基础工程能力

Status: Completed ✅


Completed:

- STM32CubeMX工程创建
- STM32 HAL库开发流程
- GPIO控制
- UART调试
- Timer配置
- Interrupt基础


Hardware Test:

- STM32F407 LED Blink
- USART1 communication


---

# Phase 2: FreeRTOS实时系统

Status: In Progress 🚧


## Day5: FreeRTOS Environment Setup


Completed:

### FreeRTOS Integration

- Added FreeRTOS middleware
- Generated RTOS project structure
- Started FreeRTOS scheduler


### Task Development

Created:


Task1:

LED Task

Function:
GPIO periodic control

Task2:

UART Debug Task

Function:
RTOS running status output



### Verification


LED:


500ms toggle



UART:


FreeRTOS UART Task Running



Result:

FreeRTOS multitasking successfully verified.


---

# 6. Engineering Notes


## HAL Library Understanding


HAL (Hardware Abstraction Layer)

provides hardware-independent APIs.

Example:

GPIO:

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);

UART:

HAL_UART_Transmit();

Advantages:

Better portability
Easier peripheral management
Faster development
RTOS Design Philosophy

Without RTOS:

while(1)
{
    Task1();

    Task2();

    Task3();
}


With RTOS:

Scheduler

 |
 |
 +-- Task1

 +-- Task2

 +-- Task3


Advantages:

Better task isolation
Clear architecture
Real-time scheduling
Easier system expansion
7. Future Roadmap
Phase 2

FreeRTOS Advanced

Task priority
Queue
Semaphore
Mutex
Memory management
Phase 3

CAN Communication System

CAN initialization
CAN message transmission
CAN receive interrupt
Custom CAN protocol
Phase 4

Motor Control

PWM
Encoder
PID controller
Speed closed-loop control
Phase 5

STM32 IAP Bootloader

Bootloader design
Flash management
CRC verification
Firmware upgrade
Phase 6

Project Optimization

Code refactoring
Documentation
System testing
Resume presentation
8. Project Structure
STM32-CAN-Industrial-Control-System

├── firmware
│
│   ├── master_node
│   │
│   ├── sensor_node
│   │
│   └── motor_node
│
├── docs
│
├── hardware
│
└── README.md

9. Learning Record
Completed
STM32 HAL Development
STM32 Peripheral Configuration
FreeRTOS Basic Integration
Multi-task Programming
Currently Learning
FreeRTOS Advanced Features
CAN Communication
Industrial Embedded System Design
Author

Embedded System Learning Project

STM32 + FreeRTOS + CAN Industrial Control System
