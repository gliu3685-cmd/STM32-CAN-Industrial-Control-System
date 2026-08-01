# Day09 - FreeRTOS 软件定时器与临界区保护

## 1. 今日目标（Objectives）

本阶段进入 FreeRTOS 实时系统高级机制学习，重点掌握定时触发与共享资源保护。

今日完成：

* 理解 FreeRTOS Software Timer（软件定时器）工作原理
* 使用 `xTimerCreate` / `xTimerStart` 创建周期定时器
* 理解临界区保护（`taskENTER_CRITICAL` / `taskEXIT_CRITICAL`）
* 理解 FromISR 中断安全 API 的适用场景
* 验证静态内存分配模式下定时器服务任务的内存供给

通过本日实验，实现：

```
Software Timer（周期 5s）
        ↓
TimerCallback()
        ↓
打印系统运行时间（uptime）
```

---

## 2. 理论学习（Theory）

### 2.1 软件定时器（Software Timer）

软件定时器由 FreeRTOS 内核管理，不占用硬件定时器外设。所有软件定时器由同一个"定时器服务任务"（Timer Service Task）统一调度。

工作流程：

```
xTimerCreate() 创建定时器
        ↓
xTimerStart()  发送启动命令到定时器命令队列
        ↓
Timer Service Task 处理命令并登记
        ↓
到期 → 调用回调函数
        ↓
自动重载 → 重新计时
```

要点：

* 回调运行在定时器服务任务上下文中，**不是中断上下文**；
* 回调内**禁止调用阻塞 API**（如 `vTaskDelay`），否则会卡住整个定时器服务；
* `pdMS_TO_TICKS(ms)` 将毫秒转换为 tick 周期。

### 2.2 临界区保护（Critical Section）

多个任务访问同一共享资源（如 UART、I2C）时可能互相打断，导致数据交错。

临界区通过关中断实现原子访问：

```c
taskENTER_CRITICAL();   // 关中断
// 受保护代码
taskEXIT_CRITICAL();    // 开中断
```

要点：

* 关中断期间系统无法响应任何中断，临界区**必须短小**；
* 打印这类耗时操作不应用临界区保护，应使用 Mutex；
* 临界区适合保护"读改写"型短操作。

### 2.3 FromISR 中断安全 API

在中断服务函数中，只能调用带 `FromISR` 后缀的 API：

| 任务上下文 | 中断上下文 | 功能 |
|---|---|---|
| `xSemaphoreGive` | `xSemaphoreGiveFromISR` | 释放信号量 |
| `xQueueSend` | `xQueueSendFromISR` | 发送消息 |
| `xTimerStart` | `xTimerStartFromISR` | 启动定时器 |

调用前提：中断优先级数值 ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`（本工程为 5）。

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

## 4. 软件定时器实现

### 4.1 启用软件定时器（FreeRTOSConfig.h）

```c
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                ( 2 )
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             ( configMINIMAL_STACK_SIZE )
```

### 4.2 创建并启动定时器（app_timer.c）

```c
TimerHandle_t xPeriodicTimer;

void Timer_Init(void)
{
    xPeriodicTimer = xTimerCreate(
            "periodic",
            pdMS_TO_TICKS(5000),   /* 周期 5000ms */
            pdTRUE,                /* 自动重载 */
            (void *)0,
            TimerCallback
    );
    if (xPeriodicTimer != NULL)
        xTimerStart(xPeriodicTimer, 0);
}
```

### 4.3 回调函数

```c
void TimerCallback(TimerHandle_t xTimer)
{
    TickType_t now = xTaskGetTickCount();
    printf("Timer fired, uptime: %lu ms\r\n", (unsigned long)now);
}
```

---

## 5. 实验过程（Experiment）

### 实验 1：软件定时器周期触发

现象：

```
Timer fired, uptime: 5001 ms
Timer fired, uptime: 10002 ms
```

验证：

* 每 5 秒触发一次，uptime 递增约 5000ms；
* 定时器服务任务正常工作，回调成功执行。

### 实验 2：UART 互斥锁完整性验证

修复前（`osDelay` 在锁内）：

```
MONITOR START
原!            ← 输出被截断（任务饿死/交错）
Timer fired, uptime: 5001 ms
```

修复后（每个打印独立加锁、`osDelay` 在锁外）：

```
MONITOR START
MONITOR END
UART START
UART END
原神牛逼!
Temp:6 Speed:1000
Timer fired, uptime: 5001 ms
```

验证：

* 每条消息完整、无残句；
* START/END 成对出现；
* SemaphoreTask 输出恢复正常。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：启用软件定时器后链接报错：

```
undefined reference to `vApplicationGetTimerTaskMemory'
```

原因：工程开启了静态内存分配（`configSUPPORT_STATIC_ALLOCATION=1`），定时器服务任务的 TCB 和栈需要由用户提供静态缓冲区。

解决：在 `freertos.c` 中实现 `vApplicationGetTimerTaskMemory`，分配 `xTimerTaskTCBBuffer` 与 `xTimerStack[configTIMER_TASK_STACK_DEPTH]`。

### Problem 2

问题：修复 UART 保护后，`SemaphoreTask` 的"原神牛逼!"不再输出。

原因：`osDelay(100)` 被放在了互斥区内，高优先级任务长时间持锁并阻塞，导致其他任务饿死。

解决：每个打印各自独立加锁，`osDelay` 移到锁外。持锁期间禁止调用阻塞 API。

---

## 7. 今日成果（Result）

完成：

* [x] FreeRTOS 软件定时器创建与周期触发
* [x] 系统运行时间读取（`xTaskGetTickCount`）
* [x] 临界区概念理解与应用
* [x] FromISR API 使用规则掌握
* [x] 定时器服务任务静态内存供给
* [x] UART 互斥锁饿死问题修复

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    ├── UART Task（Mutex 保护打印）
    ├── Monitor Task（Mutex 保护打印）
    ├── Sensor Task → Queue → Control Task
    ├── Semaphore Task（TIM3 ISR 信号量通知）
    └── Timer Service Task（5s 周期打印 uptime）
```

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 软件定时器 ≠ 硬件定时器

软件定时器由内核任务管理，回调在任务上下文运行，回调内不能阻塞；硬件定时器（如 TIM3）产生真实中断，适合精确计时。

## 持锁期间禁止阻塞

互斥区只包裹"访问共享资源"这一小段代码，做完立即释放。持锁调用 `osDelay` 会让其他任务饿死。

---

## 9. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day09 software timer with critical section protection"
```

---

## 10. 下一步计划（Next Step）

Day10：

* FreeRTOS 内存管理（heap_1 ~ heap_5）
* 各堆实现策略与适用场景
* 定时采集触发任务设计

为后续：

```
CAN 通信任务
电机控制任务
故障监控任务
```

建立实时调度基础。
