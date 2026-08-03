# Day11 - FreeRTOS 任务设计与优先级管理

## 1. 今日目标（Objectives）

本日进入 FreeRTOS 任务调度机制的核心学习，重点掌握任务状态机、优先级与
工业实时系统中常见的"优先级反转"问题。

今日完成：

* 理解任务四种状态（Running / Ready / Blocked / Suspended）与状态转换
* 掌握抢占式调度与优先级的关系（数字越大优先级越高）
* 理解时间片轮转（Time Slicing）的适用场景
* 理解优先级反转（Priority Inversion）的产生与危害
* 通过实验对比二进制信号量与互斥锁，观察优先级继承的效果

通过本日实验，实现：

```
PrioDemoTask
    ├── 场景 A：二进制信号量（无继承）→ 观察反转，High 等待时间很长
    └── 场景 B：互斥锁（有继承）→ High 等待时间明显缩短
```

---

## 2. 理论学习（Theory）

### 2.1 任务状态机（Task State）

FreeRTOS 中任务在任何时刻处于以下四种状态之一：

```
                创建
                  ↓
               Ready ──────→ Running
                ↑  ↑            │
                │  │            │ 阻塞（等待事件/延时）
                │  │            ↓
                │  └────────── Blocked
                │
                └── vTaskResume → Suspended（vTaskSuspend 挂起）
```

| 状态 | 含义 | 常见触发 |
|---|---|---|
| Running | 正在占用 CPU | 调度器选中该任务 |
| Ready | 就绪，等待 CPU | 等待比自己优先级高的任务让出 |
| Blocked | 阻塞，等待事件 | `vTaskDelay`、等队列/信号量/互斥锁 |
| Suspended | 挂起，不参与调度 | `vTaskSuspend`，需 `vTaskResume` 恢复 |

要点：

* Blocked 状态的任务**不占用 CPU**，是 RTOS 省电和调度的基础；
* 只有 Running/Ready 会消耗 CPU；
* 任务等待队列、信号量、互斥锁、延时都属于 Blocked。

### 2.2 任务优先级（Task Priority）

FreeRTOS 规则：

* 数字**越大**优先级越高（与直觉相反，注意与 CMSIS 的 osPriority 映射一致）；
* `configMAX_PRIORITIES` 决定最大优先级个数（本工程为 7，有效 0~6）；
* 抢占式调度（`configUSE_PREEMPTION=1`）：**最高优先级的 Ready 任务立即运行**，
  低优先级任务被抢占；
* 高优先级任务若一直 Ready 且不阻塞，低优先级任务会饿死。

### 2.3 时间片轮转（Time Slicing）

当**多个相同优先级**的任务同时 Ready 时：

* 开启 `configUSE_TIME_SLICING`（默认开）后，同优先级任务按 1 个 tick
  时间片轮流运行；
* 用途：让多个平级任务公平共享 CPU；
* 注意：**不同优先级之间不存在时间片轮转**，高优先级抢占低优先级。

### 2.4 优先级反转（Priority Inversion）

经典问题场景：

```
High（高优先级）    等待资源
  ┌──────────────────────────────┐
  │  资源被 Low 持有              │
  │  Low 被 Mid 反复抢占 → 迟迟做不完 │
  │  High 饿死等待                │
  └──────────────────────────────┘
Mid（中优先级）    与资源无关，但频繁抢占 CPU
Low（低优先级）    持有资源，正在工作
```

结果：**高优先级任务的实际等待时间被中优先级任务拉长**，即优先级发生了
"反转"——High 反倒要等 Low + Mid 跑完。这是实时系统的严重隐患
（如飞行控制中高优先级控制任务被饿死可能导致事故）。

### 2.5 解决方案：互斥锁的优先级继承（Priority Inheritance）

FreeRTOS 的**互斥锁（Mutex）**自带优先级继承：

```
High 等待互斥锁
    ↓
系统临时把持有锁的 Low 提升到 High 的优先级
    ↓
Mid 无法再抢占 Low
    ↓
Low 快速完成并释放锁
    ↓
High 立即获得锁
```

关键区别：

* **二进制信号量（Binary Semaphore）**：无优先级继承，可能发生反转；
* **互斥锁（Mutex）**：有优先级继承，专门用于保护共享资源。

因此：**保护共享资源用互斥锁，不用信号量**；信号量用于事件通知。

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

## 4. 实验设计（app_prio.c）

### 4.1 三个角色任务

```c
LowWorker (prio 1)  持有资源 → 忙循环 500 万次 → 释放资源
MidPrio   (prio 2)  忙循环约 30-40ms → osDelay(5)，循环 15 次（持续抢占）
HighWaiter(prio 3)  延时 100ms 后尝试获取资源 → 打印实际等待时间
```

### 4.2 两个场景对比

```c
场景 A：二进制信号量（xSemaphoreCreateBinary + Give 置为可用）
    → 无优先级继承，观察 High 的等待时间（应较长）

场景 B：互斥锁（xSemaphoreCreateMutex）
    → 有优先级继承，观察 High 的等待时间（应明显缩短）
```

### 4.3 测量方法

```c
tStart = xTaskGetTickCount();
xSemaphoreTake(...);            // 阻塞直到拿到资源
tEnd = xTaskGetTickCount();
waitMs = tEnd - tStart;         // High 实际等待时间
```

---

## 5. 实验过程（Experiment）

### 实验 1：优先级反转（场景 A，二进制信号量）

预期现象（实际数值以板子为准，A 明显大于 B）：

```
[PRIO] ==== Scenario A: Binary Semaphore (NO inheritance) ====
[PRIO] Low: got resource, working...
[PRIO] Low: work done, releasing
[PRIO] High: got sem after XXX ms      ← 等待时间较长（被 Mid 抢占拖慢）
```

验证：

* Low 持有资源期间，Mid 不断抢占 CPU，Low 的工作被切碎；
* High 只能在 Low 全部完成后拿到资源，等待时间明显拉长。

### 实验 2：优先级继承修复（场景 B，互斥锁）

预期现象：

```
[PRIO] ==== Scenario B: Mutex (priority inheritance) ====
[PRIO] Low: got resource, working...
[PRIO] Low: work done, releasing
[PRIO] High: got mutex after YYY ms   ← 等待时间明显缩短
[PRIO] Demo done
```

验证：

* High 等待互斥锁时，Low 被临时提升到 High 的优先级；
* Mid 无法抢占 Low，Low 快速完成工作，High 快速获得锁。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：打印等待时间时，若分两次 `printf`（前半句和数字），可能被其他任务
插队导致输出错乱。

原因：两个 `printf` 各自加锁，锁之间有调度间隙。

解决：`PrioPrint` 改为可变参数（`vprintf`），一条语句在单次锁内完整输出。

### Problem 2

问题：忙循环计数器若不做处理可能被编译器优化掉，导致任务"秒完成"。

原因：编译器认为循环结果无用，直接删除循环。

解决：使用 `volatile` 变量（`gWork`）防止优化，确保忙循环真实占用 CPU。

---

## 7. 今日成果（Result）

完成：

* [x] 任务四状态机（Running/Ready/Blocked/Suspended）
* [x] 抢占式调度与优先级规则
* [x] 时间片轮转概念
* [x] 优先级反转问题与危害
* [x] 互斥锁优先级继承机制
* [x] 实验对比验证（信号量 vs 互斥锁）
* [x] 编译通过（0 errors, 0 warnings）

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    ├── UART Task（Mutex 保护打印）
    ├── Monitor Task（Mutex 保护打印）
    ├── Sensor Task → Queue → Control Task
    ├── Semaphore Task（TIM3 ISR 信号量通知）
    ├── Timer Service Task（5s 周期打印 uptime）
    ├── Memory Task（heap 监控，每 5s）
    └── Prio Demo Task（反转 vs 继承对比，运行一次后自删）
```

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 高优先级不代表"先执行"，而是"抢占权"

优先级决定的是"谁先抢到 CPU"，但任务拿不到资源时只能阻塞等待。
资源分配不当时，高优先级反而会被拖垮——这就是优先级反转。

## 保护资源用 Mutex，通知事件用 Semaphore

互斥锁的优先级继承是 FreeRTOS 内建机制，选择正确的同步原语
比"加锁"本身更重要。面试中"信号量 vs 互斥锁"是必考题。

---

## 9. 面试问答（Interview Prep）

### Q1：FreeRTOS 任务有哪几种状态？各自有什么特点？

答题要点：

* 四种状态：Running（运行）、Ready（就绪）、Blocked（阻塞）、Suspended（挂起）；
* Running 是唯一占用 CPU 的状态；Ready 排队等 CPU；
* Blocked 等待事件（延时/队列/信号量），不耗 CPU；
* Suspended 需显式 `vTaskSuspend` / `vTaskResume` 操作。

### Q2：什么是优先级反转？怎么解决？

答题要点：

* 低优先级任务持有资源，中优先级任务不断抢占 CPU，导致高优先级任务饿死；
* 危害：实时系统的高优先级控制任务被无关任务拖垮；
* 解决：互斥锁的优先级继承——持锁的低优先级任务被临时提升到等锁任务的
  优先级，中优先级无法再抢占；
* 注意：二进制信号量没有该机制，所以资源保护用互斥锁。

### Q3：信号量和互斥锁有什么区别？

答题要点：

* 互斥锁有**所有权**（谁加锁谁释放）和**优先级继承**，用于保护共享资源；
* 信号量无所有权，用于**事件通知/计数**（如中断通知任务）；
* 面试加分：互斥锁还可能用于**递归锁**和**删除保护**场景。

### Q4：FreeRTOS 中优先级数字越大还是越小越高？

答题要点：

* 数字**越大**优先级越高（FreeRTOS 与 UCOS 相反，UCOS 数字越小越高）；
* `configMAX_PRIORITIES` 限制最大个数；
* 同优先级靠时间片轮转共享 CPU。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day11 task priority and priority inversion demo"
```

---

## 11. 下一步计划（Next Step）

Day12：

* Event Group 事件组（多条件等待，AND/OR 位操作）
* 模拟 ADC 采集链路（Sensor → 事件置位 → Control 处理）

为后续：

```
CAN 接收任务（中断置事件 → 任务等待并处理）
故障监控（多条件同时满足才告警）
```

建立多事件协同调度基础。
