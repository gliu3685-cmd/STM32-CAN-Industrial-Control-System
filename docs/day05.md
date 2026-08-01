# Day05 - FreeRTOS 基础移植

## 1. 今日目标（Objectives）

从裸机开发进入实时操作系统开发。

今日完成：

* 集成 FreeRTOS
* 创建 Task
* 理解 Scheduler
* 实现多任务运行

---

## 2. 理论学习（Theory）

### 2.1 为什么需要 RTOS？

裸机开发：

```c
while (1)
{
    Task1();
    Task2();
    Task3();
}
```

缺点：

* 代码耦合
* 扩展困难
* 实时性不足

### 2.2 FreeRTOS 架构

```
Application（应用层）
    ↓
FreeRTOS Scheduler（调度器）
    ↓
HAL Driver（驱动层）
    ↓
STM32 Hardware（硬件）
```

---

## 3. 任务设计（Task Design）

创建两个任务：

| 任务 | 功能 | 周期 |
|---|---|---|
| LED Task | GPIO 周期控制 | 500ms |
| UART Debug Task | 输出 RTOS 状态 | 1000ms |

---

## 4. 实现（Implementation）

创建任务：

```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

osThreadDef(uartTask, StartUartTask, osPriorityLow, 0, 128);
uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);
```

延时：

```c
osDelay(500);   /* 任务内延时 */
```

---

## 5. 遇到的问题与解决（Problems & Solutions）

### Problem 1：UART 没有输出

原因：USB-TTL 的 TX/RX 接反。

解决：

```
PA9  → RX
PA10 → TX
```

---

## 6. 实验结果（Result）

验证：

* ✅ FreeRTOS Kernel Running
* ✅ Scheduler Running
* ✅ Multiple Tasks Running

串口输出：

```
FreeRTOS UART Task Running
```

---

## 7. 工程总结（Engineering Summary）

完成 FreeRTOS 移植与多任务创建，掌握 `osThreadDef` / `osThreadCreate` / `osDelay` 基本用法，理解任务、优先级、栈的基本概念，为任务通信和同步学习打下基础。

---

## 8. 下一步计划（Next Step）

Day06：FreeRTOS 任务管理与调度机制（见 [day06.md](day06.md)）。
