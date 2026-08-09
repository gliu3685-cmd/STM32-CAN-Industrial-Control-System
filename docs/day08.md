# Day08 - FreeRTOS 同步机制（Semaphore & Mutex）

## 1. 今日目标（Objectives）

本阶段进入 FreeRTOS 实时系统核心机制学习，重点掌握任务之间的同步与资源保护。

今日完成：

* 学习 FreeRTOS Binary Semaphore（二值信号量）
* 理解 ISR 与 Task 的同步模型
* 使用 TIM3 中断触发 Semaphore，实现事件通知
* 学习 Mutex（互斥锁）机制
* 使用 Mutex 保护 UART 共享资源
* 验证多任务环境下资源竞争问题

通过本日实验，实现从：

```
中断直接控制硬件
```

向：

```
中断通知任务
任务处理业务
```

的实时系统设计思想转变。

---

## 2. 理论学习（Theory）

### 2.1 Semaphore（二值信号量）

Semaphore 是 FreeRTOS 中用于任务同步的机制，核心作用：

> 一个任务等待某个事件发生，另一个任务（或中断）负责通知。

典型模型：

```
Event → Semaphore → Task
```

例如 CAN 接收中断：

```
CAN RX Interrupt → Give Semaphore → CAN Task 处理数据
```

### 2.2 Binary Semaphore 工作流程

本实验：

```
TIM3 Interrupt
    ↓
xSemaphoreGiveFromISR()
    ↓
Semaphore（计数 +1）
    ↓
SemaphoreTask 解除阻塞
    ↓
xSemaphoreTake()
    ↓
执行任务代码
```

三个核心 API：

| API | 作用 | 使用位置 |
|---|---|---|
| `xSemaphoreCreateBinary()` | 创建二值信号量 | 任务 |
| `xSemaphoreTake()` | 等待/获取信号量 | 任务 |
| `xSemaphoreGiveFromISR()` | 释放信号量（带 FromISR） | 中断 |

### 2.3 Mutex（互斥锁）

#### 为什么需要 Mutex？

多个任务可能同时访问同一个资源，例如三个任务同时 printf 到 UART：

```
Task1 ─┐
Task2 ─┼── UART1
Task3 ─┘
```

如果没有保护，可能出现：

```
Temp:1Speed:UART
```

数据交错、输出损坏。

#### Mutex 作用

Mutex 保证**同一时间只有一个任务访问共享资源**：

```
Task ── Take Mutex ── 访问资源 ── Release Mutex
```

其他任务：

```
Take Mutex → 资源被占 → 阻塞等待 → 获得后继续
```

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2
* USB-TTL（CH340G）

软件：

* STM32CubeIDE
* FreeRTOS Kernel V10.3.1（CMSIS_V1）
* HAL Library

工程：`f407_blink`（主工程，与仓库 master_node 同步）

当前阶段：Phase 2 FreeRTOS 实时系统

---

## 4. Binary Semaphore 实现

### 4.1 创建 Semaphore

位置：freertos.c 的 `MX_FREERTOS_Init()`。

```c
xBinarySemaphore = xSemaphoreCreateBinary();
```

### 4.2 创建 Semaphore Task

```c
osThreadDef(semaphoreTask, SemaphoreTask, osPriorityHigh, 0, 128);
osThreadCreate(osThread(semaphoreTask), NULL);
```

### 4.3 SemaphoreTask 实现

```c
void SemaphoreTask(void const * argument)
{
    while (1)
    {
        if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdTRUE)
        {
            osMutexWait(uartMutexHandle, osWaitForever);
            printf("原神牛逼!\r\n");   /* 个人测试文案，正式项目建议规范日志 */
            osMutexRelease(uartMutexHandle);

            HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10);
        }
    }
}
```

### 4.4 TIM3 中断释放 Semaphore

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        xSemaphoreGiveFromISR(xBinarySemaphore, NULL);
    }
}
```

实现链路：

```
TIM3 → Semaphore → SemaphoreTask → LED10 Toggle
```

---

## 5. Mutex 实现

### 5.1 创建 UART Mutex

freertos.c：

```c
osMutexDef(uartMutex);
uartMutexHandle = osMutexCreate(osMutex(uartMutex));
```

### 5.2 UART 资源保护

所有涉及串口打印的任务统一加锁：

```c
osMutexWait(uartMutexHandle, osWaitForever);
printf("UART message\r\n");
osMutexRelease(uartMutexHandle);
```

保护范围：

* UART Task
* Monitor Task
* Semaphore Task

---

## 6. 实验过程（Experiment）

### 实验 1：Semaphore 同步测试

现象：

* LED9：500ms 翻转（定时器任务）
* LED10：TIM3 触发后翻转
* 串口：每次 TIM3 中断输出一次通知

验证：

```
TIM3 ISR → Give Semaphore → Task 解除阻塞 → 执行业务
```

中断只负责"通知"，业务由任务完成。

### 实验 2：Mutex UART 保护

加入 Mutex 前：多个任务竞争 UART，偶尔产生异常字符（数据交错）。

加入 Mutex 后：

```
System Monitor Running
Temp:13 Speed:1000
FreeRTOS UART Task Running
原神牛逼!
```

所有消息保持完整。

### 实验 3：制造竞争验证 Mutex

增加"START / 延时 / END"打印片段制造竞争，测试结果：

```
MONITOR START
MONITOR END

UART START
UART END
```

没有出现交错：

```
MONITOR START
UART START
MONITOR END
UART END
```

证明 Mutex 成功保护临界资源。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：编译报 `implicit declaration of function 'printf'`。

原因：app_sync.c 缺少 stdio 声明。

解决：添加：

```c
#include <stdio.h>
```

### Problem 2

问题：编译报 `osMutexWait undeclared`、`uartMutexHandle undeclared`。

原因：多文件之间变量和 API 不可见。

解决：添加：

```c
#include "cmsis_os.h"
```

并声明：

```c
extern osMutexId uartMutexHandle;
```

### Problem 3

问题：LED10 闪烁但 Semaphore 未验证。

原因：TIM3 中断直接控制 GPIO（`HAL_GPIO_TogglePin`），没有经过 Semaphore。

解决：修改为中断只通知任务：

```c
xSemaphoreGiveFromISR(xBinarySemaphore, NULL);
```

实现 ISR 通知 Task 的模型。

---

## 8. 今日成果（Result）

完成：

* [x] FreeRTOS Binary Semaphore 实验
* [x] TIM3 中断同步 Task
* [x] ISR-to-Task 事件通知
* [x] FreeRTOS Mutex 实验
* [x] UART 共享资源保护
* [x] 多任务调试验证

当前 FreeRTOS 架构：

```
FreeRTOS
  ├── SensorTask ──→ Queue ──→ ControlTask
  ├── UART / Monitor / Semaphore Task ──→ UART（Mutex 保护）
  └── SemaphoreTask ←── Semaphore ←── TIM3 ISR
```

---

## 9. 工程总结（Engineering Summary）

本日学习重点不是 API 调用，而是理解实时系统设计思想：

## 中断不要处理业务

错误：

```
Interrupt → 直接控制外设
```

正确：

```
Interrupt → 通知 Task → Task 处理业务
```

中断必须"快进快出"，耗时业务放任务里。

## 共享资源必须保护

例如 UART、SPI、I2C、Flash 等外设被多任务共享时，必须用 Mutex 保证
同一时刻只有一个访问者，否则数据交错、逻辑错乱。

---

## 10. 面试问答（Interview Prep）

### Q1：信号量和互斥锁有什么区别？

答题要点：

* 信号量管"数量/通知"（无所有权，谁都能 give/take）；
* 互斥锁管"排他"（有所有权，只能持有者释放，支持优先级继承）；
* 保护共享资源用互斥锁，事件通知用信号量。

### Q2：中断里为什么必须用 FromISR 版本 API？

答题要点：

* 普通 API 可能阻塞（挂起调度器/临界区），中断上下文不允许；
* FromISR 版本不阻塞，并通过参数告知是否唤醒高优先级任务，
  由调用方决定是否立即切换。

### Q3：二值信号量和计数信号量区别？

答题要点：

* 二值信号量只有 0/1，适合"事件发生/未发生"；
* 计数信号量可累计多次事件，适合"资源池有 N 个"；
* 中断给多次、任务取多次时用计数信号量。

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day08 FreeRTOS semaphore and mutex synchronization"
```

---

## 12. 下一步计划（Next Step）

Day09（已完成）：FreeRTOS 高级机制——Software Timer（软件定时器）、
FromISR、临界区保护（见 [day09.md](day09.md)）。

后续：

```
CAN 通信任务
电机控制任务
故障监控任务
```

建立实时调度基础。
