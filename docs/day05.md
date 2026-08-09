# Day05 - FreeRTOS 基础移植

## 1. 今日目标（Objectives）

从裸机开发进入实时操作系统开发，完成 FreeRTOS 移植与第一个多任务程序。

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

* 代码耦合（改一个功能影响全部）
* 扩展困难（加功能要改主循环）
* 实时性不足（Task3 必须等 Task1、Task2 跑完）

RTOS 由调度器（Scheduler）统一管理多个任务，每个任务独立循环，
按优先级和延时自动切换，互不阻塞。

### 2.2 FreeRTOS 架构

```
Application（应用层：我们的任务代码）
    ↓
FreeRTOS Scheduler（调度器：任务切换）
    ↓
HAL Driver（驱动层）
    ↓
STM32 Hardware（硬件）
```

### 2.3 任务 = 无限循环 + 栈 + 优先级

FreeRTOS 任务就是一个永不返回的 C 函数，每个任务有自己的栈（局部变量）
和优先级。创建任务时这两个参数必须给对：栈太小会溢出（Day13 详解），
优先级决定抢占顺序（Day11 详解）。

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

当前阶段：Phase 2 FreeRTOS 实时系统

---

## 4. 任务设计（Task Design）

创建两个任务：

| 任务 | 功能 | 周期 |
|---|---|---|
| LED Task | GPIO 周期控制 | 500ms |
| UART Debug Task | 输出 RTOS 状态 | 1000ms |

---

## 5. 实现（Implementation）

创建任务（freertos.c）：

```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

osThreadDef(uartTask, StartUartTask, osPriorityLow, 0, 128);
uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);
```

任务内延时（释放 CPU 给其他任务）：

```c
osDelay(500);   /* 任务进入 Blocked，调度器切换其他任务 */
```

---

## 6. 实验过程（Experiment）

现象：

* 两个任务同时"运行"：LED 500ms 翻转，串口每秒打印一次；
* 互不阻塞，调度器自动切换。

串口输出：

```
FreeRTOS UART Task Running
FreeRTOS UART Task Running
```

验证：

* FreeRTOS Kernel Running
* Scheduler Running
* Multiple Tasks Running

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：UART 没有输出

问题：程序能运行但串口无数据。

原因：USB-TTL 的 TX/RX 接反。

解决：

```
PA9  → RX
PA10 → TX
```

（芯片 TX 接 USB-TTL 的 RX，芯片 RX 接 USB-TTL 的 TX。）

---

## 8. 今日成果（Result）

完成：

* [x] FreeRTOS 集成到工程
* [x] 多任务创建与运行
* [x] 理解 Scheduler 与 osDelay 释放 CPU

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    └── UART Debug Task（1s 打印）
```

---

## 9. 工程总结（Engineering Summary）

本日学习重点：

## 从"主循环"到"任务"

裸机是一个大循环串行执行，RTOS 是把功能拆成多个独立任务，由调度器
管理切换。任务之间通过延时/同步机制协作，互不阻塞。

## osDelay 不是"空等"

`osDelay` 让当前任务进入 Blocked 状态并释放 CPU，调度器立刻切换到
其他就绪任务——这是 RTOS 与裸机 `HAL_Delay` 的本质区别。

---

## 10. 面试问答（Interview Prep）

### Q1：RTOS 相比裸机有什么优势？

答题要点：

* 功能解耦（任务独立）、扩展性好；
* 实时性（按优先级抢占）；
* 周期/事件任务统一由调度器管理。

### Q2：FreeRTOS 任务是怎么"同时运行"的？

答题要点：

* 单核 CPU 同一时刻只跑一个任务；
* 调度器按优先级 + 时间片快速切换，宏观上像并行；
* 切换靠 SysTick 中断（tick）。

### Q3：osDelay 和 HAL_Delay 有什么区别？

答题要点：

* HAL_Delay 空转阻塞 CPU，其他任务无法运行；
* osDelay 让出 CPU（任务进入 Blocked），调度器切换其他任务；
* RTOS 工程中禁止用 HAL_Delay 做任务延时。

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day05 FreeRTOS porting and multi-task demo"
```

---

## 12. 下一步计划（Next Step）

Day06：FreeRTOS 任务管理与调度机制（见 [day06.md](day06.md)）。
