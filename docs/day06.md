# Day06 - FreeRTOS 任务管理与调度机制

## 1. 今日目标（Objectives）

深入理解 FreeRTOS 任务运行机制。

今日学习：

* Task 生命周期
* Task 优先级
* FreeRTOS Tick 机制
* osDelay 与 HAL_Delay 区别
* 多任务调度过程

---

## 2. 理论学习（Theory）

### 2.1 FreeRTOS 任务模型

裸机开发将所有功能集中在主循环：

```c
while (1)
{
    Task1();
    Task2();
    Task3();
}
```

随着功能增加，出现代码耦合、任务管理困难、实时性下降等问题。

引入 FreeRTOS 后，由调度器统一管理多个任务：

```
FreeRTOS Scheduler
    ├── LED Task
    ├── UART Task
    └── Monitor Task
```

### 2.2 Task 状态

FreeRTOS 任务主要状态：

| 状态 | 说明 |
|---|---|
| Ready（就绪） | 任务已创建，等待 CPU 调度 |
| Running（运行） | 当前正在执行的任务 |
| Blocked（阻塞） | 等待事件（延时结束、消息到达等） |
| Suspended（挂起） | 任务被主动暂停 |

例如：

```c
osDelay(500);   /* 任务进入 Blocked 状态 */
```

### 2.3 Task 优先级

FreeRTOS 通过优先级决定任务调度顺序。本次实验创建三个任务：

| 任务 | 优先级 | 功能 |
|---|---|---|
| Monitor Task | High | 系统状态监控 |
| Default Task（LED） | Normal | LED 周期控制 |
| UART Task | Low | 串口调试输出 |

```
High Priority   →  Monitor Task
Normal Priority →  LED Task
Low Priority    →  UART Task
```

### 2.4 Tick 机制

FreeRTOS 需要时间基准管理任务调度：

```
SysTick Timer
    ↓
FreeRTOS Kernel
    ↓
Task Scheduling
```

Tick 用于：延时管理、任务切换、时间统计。

### 2.5 osDelay 与 HAL_Delay 区别

**HAL_Delay（裸机）**：

```c
HAL_Delay(500);
```

特点：CPU 阻塞等待，无法执行其他任务。

**osDelay（RTOS）**：

```c
osDelay(500);
```

特点：当前任务进入 Blocked 状态，CPU 释放，调度其他任务运行。

结论：FreeRTOS 项目中推荐使用 `osDelay()`，避免大量使用 `HAL_Delay()`。

---

## 3. 实验实现（Implementation）

创建三个任务：

**LED Task**（GPIOF PIN9，500ms 翻转）：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
osDelay(500);
```

**UART Task**（每 1000ms 输出状态）：

```c
printf("FreeRTOS UART Task Running\r\n");
```

**Monitor Task**（GPIOF PIN10，2000ms 翻转）：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10);
```

---

## 4. 实验结果（Experiment）

运行结果：

* ✅ FreeRTOS 调度器正常运行
* ✅ 三个 Task 同时工作
* ✅ 不同优先级任务正常调度
* ✅ UART 调试信息正常输出

现象：

* LED1：500ms 周期闪烁
* LED2：2s 周期闪烁
* UART：`FreeRTOS UART Task Running`

---

## 5. 遇到的问题与解决（Problems & Solutions）

### Problem 1：StartMonitorTask 链接错误

错误：

```
undefined reference to StartMonitorTask
```

原因：创建任务时声明了 Task，但没有实现函数。

解决：增加函数实现：

```c
void StartMonitorTask(void const * argument)
{
    while (1)
    {
    }
}
```

---

## 6. 今日总结（Summary）

通过 Day06 学习：

* 掌握 FreeRTOS 任务调度基本原理
* 理解 Task 不是并行运行，而是由 Scheduler 切换
* osDelay 可以主动释放 CPU
* 不同任务可以通过 Priority 管理实时性

为后续学习 Queue、Semaphore、Mutex 和 CAN 通信任务设计打下基础。

---

## 7. 下一步计划（Next Step）

Day07：FreeRTOS Queue 任务通信（见 [day07.md](day07.md)）。
