STM32-CAN-Industrial-Control-System

基于 STM32 + FreeRTOS + CAN 总线的三节点工业控制系统原型。

模拟工业现场分布式控制架构，以 STM32F407 为主控节点、两个 STM32F103 分别为传感节点和执行节点，覆盖实时任务调度、CAN 总线通信、电机闭环控制、故障监测等工业嵌入式核心环节。

1. 项目架构
                    CAN Bus (500kbps)
                         │
            ┌────────────┴────────────┐
            │                         │
    STM32F407 Master          STM32F103 Sensor Node
    (主控调度)                 (MPU6050 数据采集)
            │                         │
            │                         │
    STM32F103 Motor Node
    (PWM 电机控制 + 编码器反馈 + PID)

系统功能链路：

传感器采集 → CAN 通信 → 主控决策 → CAN 指令 → 电机执行 → 编码器反馈 → PID 闭环

节点职责：

节点	MCU	职责
Master	STM32F407VET6	任务调度、CAN 通信管理、数据处理、故障监控
Sensor Node	STM32F103C8T6	MPU6050 数据采集、CAN 上报
Motor Node	STM32F103C8T6	PWM 电机驱动、编码器反馈、PID 速度闭环
2. 软件架构
Application Layer
      │
FreeRTOS Task Layer
      │
  HAL Driver Layer
      │
STM32 Hardware
FreeRTOS 任务规划
FreeRTOS Scheduler
      │
      ├── CAN Receive Task    — 接收 CAN 报文，按 ID 分发
      ├── CAN Transmit Task   — 发送控制指令
      ├── Control Logic Task  — 传感器融合 + 决策
      ├── Fault Monitor Task  — 心跳检测 + 离线告警
      └── UART Debug Task     — 状态输出
3. 技术栈
类别	内容
MCU	STM32F407VET6, STM32F103C8T6
RTOS	FreeRTOS (CMSIS_V1)
通信	CAN 总线 (500kbps)，自定义应用层协议
控制	PWM 输出，编码器反馈，PID 速度闭环
工具	STM32CubeMX, STM32CubeIDE, ST-Link V2, Git
4. 开发进度
Phase 1：STM32 基础工程能力 ✅
STM32CubeMX 工程创建与 HAL 库开发流程
GPIO / UART / Timer / Interrupt 基础
三块板裸机点灯与串口通信验证
Phase 2：FreeRTOS 实时系统 🚧
FreeRTOS 中间件集成
双任务验证（LED 翻转 + 串口状态输出）
多任务调度正常运行
Phase 3-6（规划中）
Phase	内容
3	CAN 通信系统（初始化、收发中断、自定义协议）
4	电机控制（PWM、编码器、PID 速度闭环）
5	STM32 IAP Bootloader（Flash 分区、CRC 校验、固件升级）
6	项目整理（代码重构、文档、测试）
5. 项目结构
STM32-CAN-Industrial-Control-System
├── firmware/
│   ├── master_node/        # F407 主控工程
│   ├── sensor_node/        # F103 传感器节点工程
│   └── motor_node/         # F103 电机节点工程
├── docs/                   # 协议文档、架构图
├── hardware/               # 硬件接线说明、原理图
└── README.md
6. 学习记录

已完成：STM32 HAL 开发、FreeRTOS 基础移植、多任务编程 进行中：FreeRTOS 进阶（队列/信号量/互斥量）、CAN 通信 规划中：电机控制、IAP Bootloader
