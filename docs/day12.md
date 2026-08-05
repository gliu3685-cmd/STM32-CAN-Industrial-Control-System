# Day12 - FreeRTOS 事件组（Event Group）与任务通知

## 1. 今日目标（Objectives）

本日进入 FreeRTOS 多条件同步机制的学习，重点掌握"多个事件条件组合等待"。

今日完成：

* 理解事件组（Event Group）原理与位（Bit）管理
* 掌握 `xEventGroupCreate` / `xEventGroupSetBits` / `xEventGroupWaitBits`
* 区分 AND（全部满足）与 OR（任一满足）两种等待模式
* 理解事件组与信号量/队列的区别与适用场景
* 了解任务通知（Task Notification）这一轻量同步机制

通过本日实验，实现：

```
SensorTask 每 300ms → 置位 EVT_DATA（模拟 ADC）
AlarmTask  每 1000ms → 置位 EVT_ALARM（模拟告警）
ControlTask：阶段1 OR 等待 → 阶段2 AND 等待 → 任务通知收尾
```

---

## 2. 理论学习（Theory）

### 2.1 事件组是什么

事件组是一个 32 位的整数，每一位（Bit）代表一个"事件是否发生"：

```
bit31 ... bit2  bit1  bit0
                 │     │
                 │     └─ EVT_DATA   （0=未发生，1=已发生）
                 └─────── EVT_ALARM  （0=未发生，1=已发生）
```

核心价值：**一个任务可以同时等待多个事件，并指定"全部满足（AND）"或
"任一满足（OR）"**。这是信号量做不到的——信号量一次只能等一个事件。

### 2.2 三个核心 API

```c
xEventGroupCreate();                        // 创建事件组（返回句柄）
xEventGroupSetBits(evt, EVT_DATA);          // 置位：标记事件发生
xEventGroupWaitBits(evt, EVT_DATA|EVT_ALARM,// 等待：关心的位
                    pdTRUE,                 // 唤醒后是否自动清除这些位
                    pdFALSE,                // AND(全部)/OR(任一)
                    portMAX_DELAY);          // 等待时间
```

`xEventGroupWaitBits` 第三个参数 `xWaitForAllBits`：

* `pdFALSE` = **OR**：任一关心位被置位即返回；
* `pdTRUE` = **AND**：所有关心位都被置位才返回。

### 2.3 事件组 vs 信号量 vs 队列

| 机制 | 传递什么 | 能等几个条件 | 典型场景 |
|---|---|---|---|
| 队列 | 数据 | 1 个（有数据没数据） | 任务间传数据 |
| 信号量 | 计数/通知 | 1 个 | 中断通知任务、资源计数 |
| 事件组 | 32 个事件位 | **多个，可 AND/OR** | 多条件联动、多故障告警 |

要点：事件组**不传递数据**，只传递"发生了什么事"。

### 2.4 任务通知（Task Notification）——补充知识点

任务通知是 FreeRTOS V8.2+ 提供的轻量同步机制：

```c
xTaskNotifyGive(handle);     // 发送通知（给指定任务）
ulTaskNotifyTake(pdTRUE, timeout);  // 等待通知
```

特点：

* **更快更省内存**：每个任务自带一个 32 位通知值，无需创建内核对象；
* 只能"点对点"（明确指定目标任务），不能广播；
* 可模拟二值信号量、计数信号量、事件组；
* 面试常问：任务通知和信号量区别——**通知更轻量，但没有队列/信号量的
  通用性（如多任务等待同一事件、获取时阻塞多个任务）**。

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

## 4. 实验实现（app_event.c）

### 4.1 启用事件组（FreeRTOSConfig.h，USER CODE 保护区）

```c
#define configUSE_EVENT_GROUPS             1
#define INCLUDE_xTaskGetCurrentTaskHandle  1
```

### 4.2 事件位与三个任务

```c
#define EVT_DATA     (1 << 0)   // bit0：模拟 ADC 采集完成
#define EVT_ALARM    (1 << 1)   // bit1：模拟告警触发

SensorTask: 每 300ms  置位 EVT_DATA，打印 ADC 读数
AlarmTask:  每 1000ms 置位 EVT_ALARM，打印告警
ControlTask: 先 OR 等待 5 次，再 AND 等待 3 次
```

### 4.3 核心等待代码

```c
/* OR 模式：任一事件到位即唤醒 */
bits = xEventGroupWaitBits(xEventGroup,
        EVT_DATA | EVT_ALARM,
        pdTRUE, pdFALSE, portMAX_DELAY);

/* AND 模式：两个事件都到位才唤醒 */
bits = xEventGroupWaitBits(xEventGroup,
        EVT_DATA | EVT_ALARM,
        pdTRUE, pdTRUE, portMAX_DELAY);
```

### 4.4 任务通知收尾

```c
/* ControlTask 完成后通知组织者 */
xTaskNotifyGive(hDemoOwner);

/* EventDemoTask 中等待通知 */
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

---

## 5. 实验过程（Experiment）

### 实验 1：OR 模式（任一事件触发）

预期现象（数字以实际为准）：

```
== Stage 1: OR (any bit wakes task) ==
Sensor: ADC=17, set DATA
OR wake: DATA
Sensor: ADC=34, set DATA
OR wake: DATA
Alarm: set ALARM
OR wake: ALARM
...
```

验证：

* 每 300ms 的 DATA 或每 1000ms 的 ALARM 都能唤醒 ControlTask；
* 触发频率约 300ms 一次（谁先置位谁触发）。

### 实验 2：AND 模式（全部事件满足）

预期现象：

```
== Stage 2: AND (ALL bits must be set) ==
Sensor: ADC=..., set DATA
...
Alarm: set ALARM
AND wake: both ready (bits=0x03)
```

验证：

* ControlTask 必须等 DATA 和 ALARM **同时置位**才唤醒；
* 由于 ALARM 每 1000ms 一次，AND 唤醒周期约 1000ms；
* `bits=0x03` 表示两个位同时为 1。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：`xEventGroupWaitBits` 参数多、容易记混（尤其 AND/OR 那个参数）。

原因：5 个参数分别控制"关心哪些位、是否清除、AND/OR、超时"。

解决：用宏封装语义化调用（如 `WaitAll(evt, bits)` / `WaitAny(evt, bits)`），
并记住口诀：**第四个参数 `pdTRUE`=全部（AND），`pdFALSE`=任一（OR）**。

### Problem 2

问题：演示任务完成后，传感器/告警任务仍在周期打印，串口无法收场。

原因：任务各自独立运行，没有统一的生命周期管理。

解决：用**任务通知**做收尾——ControlTask 完成后 `xTaskNotifyGive` 通知
组织者，组织者 `ulTaskNotifyTake` 收到后统一删除全部演示任务。
顺带实践了任务通知的用法。

---

## 7. 今日成果（Result）

完成：

* [x] 事件组原理与位管理（32 位事件位）
* [x] `xEventGroupCreate` / `xEventGroupSetBits` / `xEventGroupWaitBits`
* [x] AND / OR 多条件等待对比实验
* [x] 事件组 vs 信号量 vs 队列对比理解
* [x] 任务通知（xTaskNotifyGive / ulTaskNotifyTake）实践
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
    ├── Prio Demo Task（优先级反转对比，一次性）
    └── Event Demo Task（事件组 AND/OR + 任务通知，一次性）
```

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 事件组解决的是"多条件等待"

当一个任务需要"多个事件都满足"或"任一事件发生"才动作时，队列和信号量
都无能为力，事件组是最直接的答案。工业场景典型应用：故障监控任务等待
"温度过高 + 转速异常"两个条件同时成立才告警。

## 通知比信号量轻，但有代价

任务通知省内存、速度快，但只能点对点。面试中被问"为什么不全用任务通知"
时，要能答出：信号量/队列支持多任务等待同一事件、支持广播，通用性更强。

---

## 9. 面试问答（Interview Prep）

### Q1：事件组和信号量有什么区别？什么时候用事件组？

答题要点：

* 信号量只表达"一个事件"（或计数），事件组用 32 个位表达多个事件；
* 事件组支持 AND（全部满足）/ OR（任一满足）等待，信号量不支持；
* 典型场景：多故障告警联动、多条件任务启动、等待"就绪信号"组合。

### Q2：`xEventGroupWaitBits` 的 AND 和 OR 怎么区分？

答题要点：

* 第四个参数 `xWaitForAllBits`：`pdTRUE` = AND（全部置位才返回），
  `pdFALSE` = OR（任一置位即返回）；
* 第三个参数控制返回后是否自动清除事件位（`pdTRUE` 清除）。

### Q3：任务通知和信号量/队列有什么区别？

答题要点：

* 任务通知更快、更省内存（无需创建内核对象），但只能点对点；
* 信号量/队列可以"多任务等待同一对象"、可广播，通用性更强；
* FreeRTOS 官方建议：简单通知用任务通知，复杂同步用信号量/队列。

### Q4：事件组能传数据吗？

答题要点：

* 不能，事件组只传"事件是否发生"的状态位；
* 要传数据用队列；要传"发生了什么"用事件组；两者可组合使用
  （事件置位后任务再去队列取数据）。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day12 event group AND/OR wait demo with task notification"
```

---

## 11. 下一步计划（Next Step）

Day13：

* 异常检测与调试（栈溢出检测、malloc 失败处理、HardFault 定位、Assert）
* 系统稳定性保障

为后续：

```
CAN 通信任务
电机控制任务
故障监控任务
```

建立可定位、可恢复的稳健运行基础。
