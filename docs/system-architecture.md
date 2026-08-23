# 系统架构与任务设计

## 1. 总体拓扑

```text
                  CAN Bus (500 kbps)

     STM32F407 Master          STM32F103 Sensor      STM32F103 Motor
      （主控调度）               （温度 + MPU6050）     （PWM + 编码器 + PID）
```

数据流：`传感器采集 → CAN 上报 → 主控决策 → CAN 下发 → 电机执行 → 编码器反馈 → PID 闭环`

## 2. 分层

```text
Application Layer（app_arch.c / app_can.c / app_motor.c ...）
        │
FreeRTOS Task Layer（任务调度、队列、信号量、互斥量）
        │
  HAL Driver Layer（STM32 HAL：GPIO/UART/CAN/TIM/I2C）
        │
   STM32 Hardware
```

## 3. 主控（STM32F407ZGT6）

任务（ArchDemoTask 创建，优先级 Low）：

| 任务 | 职责 |
|---|---|
| CanRxTask | 从 xCanRxQueue 收帧，按 ID 分发/更新节点状态表 |
| CanTxTask | 周期发心跳 0x100、命令 0x101/0x201（500/1000 交替，约 10s 一轮） |
| ControlTask | 温度越限检测 + 速度指令计算（输出到 xMotorQueue） |
| FaultMonitorTask | 心跳超时离线检测、告警与恢复 |
| DebugTask | 串口状态输出（加 uartMutex） |

任务间通信：

* `xCanRxQueue`（8，CanFrame_t）：CAN 接收中断 → 队列 → CanRxTask
* `xMotorQueue`（4，uint16_t）：ControlTask → CanTxTask 待发速度
* `uartMutex`：串口打印互斥，防止多任务输出交错

## 4. 传感器节点（STM32F103C8T6）

| 任务 | 职责 |
|---|---|
| CanRxTask | 收 0x101，请求时上报温度（0x102） |
| CanTxTask | 周期上报温度 0x102 + MPU6050 加速度 0x103；CAN 初始化与故障恢复 |
| StartDefaultTask | 状态 LED |

通信：`xCanRxQueue`（8）+ `uartMutex`。

## 5. 电机节点（STM32F103C8T6）

| 任务 | 职责 |
|---|---|
| CanRxTask | 收 0x201 设置速度，更新 g_speed |
| CanTxTask | 20ms 周期：编码器测速 → EMA 平滑 → PID 计算占空比 → 输出 PWM；1s 上报 0x202（RPM） |
| StartDefaultTask | 状态 LED |

通信：`xCanRxQueue`（8）+ `uartMutex`；PID 状态在 app_motor.c 内（增量式 + 积分分离 + 限幅）。

## 6. 中断与安全规则

* CAN RX0 中断只做“读帧 → 投队列”，业务在任务中处理（FromISR 安全接口）
* ISR 内禁止 printf；CAN 错误回调只恢复 State=READY，详情由任务读取 ESR
* 执行器默认安全态：无命令停转（IN1/IN2 双低）
* 编码器差值法测速，规避 16 位计数器回绕

## 7. 相关文档

* [CAN 协议](can-protocol.md)
* [Bootloader 实施计划](bootloader-plan.md)
* [接线说明](../hardware/wiring-1breadboard.md)
