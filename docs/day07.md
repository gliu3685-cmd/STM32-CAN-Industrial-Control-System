# Day07 - FreeRTOS Queue 任务通信实验

## 1. 今日目标（Objectives）

本日进入 FreeRTOS 实时系统阶段的任务通信学习。

在 Day05~06 中已完成 FreeRTOS 基础移植、多任务创建
（LED / UART / Monitor Task），但各个任务之间相互独立，没有数据交互。

工业控制系统中，不同任务之间需要进行可靠的数据传递，例如：

* 传感器采集任务读取数据
* 控制任务根据数据进行决策
* 通信任务发送状态信息

因此本日学习并实现：

* FreeRTOS Queue（消息队列）
* Producer-Consumer 模型
* Task 间数据传递
* Sensor Task → Queue → Control Task 架构

最终实现：

```
SensorTask
    ↓
FreeRTOS Queue
    ↓
ControlTask
```

---

## 2. 理论学习（Theory）

### 2.1 为什么需要 Task 通信

裸机程序中通常采用：

```c
while (1)
{
    read_sensor();
    calculate();
    control_motor();
}
```

所有功能集中在一个循环中，问题：

* 模块耦合严重
* 实时性差
* 功能扩展困难

FreeRTOS 采用多任务架构，每个任务负责独立功能，通过通信机制交换数据：

```
SensorTask ──→ Queue ──→ ControlTask
```

### 2.2 FreeRTOS Queue 介绍

Queue（消息队列）是一种任务间通信机制，特点：

* FIFO（先进先出）
* 支持任务阻塞（发送满/接收空时可阻塞等待）
* 支持数据复制（发送方数据复制进队列，安全）
* 多任务安全（内部用临界区保护）

例如 SensorTask 发送 `Temperature = 25`、`Speed = 1000`，Queue 保存后
由 ControlTask 读取并进行控制。

### 2.3 Producer-Consumer 模型

本实验采用经典生产者-消费者模型：

* Producer：SensorTask（产生数据）
* Consumer：ControlTask（消费数据）

这种结构也是工业控制系统常见设计方式。未来 CAN 项目：

```
CAN Receive Task
    ↓
  Queue
    ↓
Control Task
    ↓
Motor Control
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

## 4. 工程实现（Implementation）

### 4.1 工程目录调整

新增 Application 层：

```
Core
├── Inc
│   ├── app_task.h
│   └── app_queue.h
└── Src
    ├── app_task.c
    └── app_queue.c
```

| 文件 | 功能 |
|---|---|
| app_task.c | Task 业务逻辑 |
| app_task.h | Task 接口声明 |
| app_queue.c | Queue 创建 |
| app_queue.h | Queue 接口声明 |

### 4.2 SensorData 数据结构

```c
typedef struct
{
    uint16_t temperature;
    uint16_t speed;
} SensorData;
```

模拟 temperature、speed 两个工业控制常见参数。

### 4.3 创建 Queue

```c
QueueHandle_t sensorQueue;

void Queue_Init(void)
{
    sensorQueue = xQueueCreate(10, sizeof(SensorData));
}
```

创建参数：Queue 长度 10，每个元素为 SensorData。

### 4.4 SensorTask 实现

功能：模拟采集传感器数据并发送到 Queue。

```c
void SensorTask(void const * argument)
{
    SensorData data;
    data.temperature = 0;
    data.speed = 1000;

    while (1)
    {
        data.temperature++;
        xQueueSend(sensorQueue, &data, portMAX_DELAY);
        osDelay(1000);
    }
}
```

流程：产生数据 → `xQueueSend()` → Queue。

### 4.5 ControlTask 实现

功能：从 Queue 读取数据并输出控制信息。

```c
void ControlTask(void const * argument)
{
    SensorData recv;

    while (1)
    {
        if (xQueueReceive(sensorQueue, &recv, portMAX_DELAY) == pdPASS)
        {
            printf("Temp:%d Speed:%d\r\n", recv.temperature, recv.speed);
        }
    }
}
```

流程：Queue → `xQueueReceive()` → 处理数据。

### 4.6 在 FreeRTOS 中创建任务

```c
/* 初始化 Queue */
Queue_Init();

/* 创建 SensorTask */
osThreadDef(sensorTask, SensorTask, osPriorityNormal, 0, 128);
sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

/* 创建 ControlTask */
osThreadDef(controlTask, ControlTask, osPriorityHigh, 0, 128);
controlTaskHandle = osThreadCreate(osThread(controlTask), NULL);
```

任务优先级：ControlTask（High）高于 SensorTask（Normal），体现实时控制思想。

---

## 5. 实验结果（Experiment）

编译结果：

```
Build Finished.
0 errors
0 warnings
```

说明：FreeRTOS 任务创建成功，Queue 模块成功链接，Application 层结构正常。

预期串口输出：

```
FreeRTOS UART Task Running
System Monitor Running
Temp:1 Speed:1000
Temp:2 Speed:1000
Temp:3 Speed:1000
```

说明：SensorTask → Queue → ControlTask 数据通信成功。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：找不到 app_task.h

问题：编译报 `fatal error: app_task.h: No such file or directory`。

原因：CubeIDE 没有正确添加 include 路径。

解决：将 `Core/Inc` 添加到 `C/C++ Build → Settings → Include paths`。

### Problem 2：undefined reference

问题：编译报 `undefined reference to SensorTask / ControlTask / Queue_Init`。

原因：头文件存在，但 `.c` 文件没有参与编译。

解决：检查 `Core/Src` 下的 `app_task.c`、`app_queue.c`，重新加入工程后解决。

### Problem 3：工程路径缓存

问题：改动后编译仍用旧配置。

原因：CubeIDE 保存旧 build 配置。

解决：执行 `Project → Clean → Build Project`，最终恢复正常。

---

## 7. 今日成果（Result）

完成：

* [x] 理解 Queue 概念与 FIFO 机制
* [x] Producer-Consumer 模型
* [x] `xQueueCreate()` / `xQueueSend()` / `xQueueReceive()`
* [x] Sensor Task → Queue → Control Task 数据通信

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    ├── UART Task（1s 打印）
    ├── Monitor Task（2s 翻转）
    ├── SensorTask ──→ Queue ──→ ControlTask
```

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 队列解决"任务间传数据"

任务之间不共享全局变量直接读写，而是通过队列复制传递，天然安全解耦。
生产者和消费者速率不一致时，队列还能起缓冲作用。

## 架构演进

从单循环裸机程序升级为 FreeRTOS 多任务实时系统，队列把"采集"和
"控制"解耦：

```
Sensor Node → CAN Receive Task → Queue → Control Task → PID Motor Control
```

Day07 实现的 Queue 机制将直接应用于：CAN 数据接收、电机控制、故障监控。

---

## 9. 面试问答（Interview Prep）

### Q1：队列和全局变量传数据有什么区别？

答题要点：

* 全局变量需要互斥锁保护，容易数据竞争；
* 队列内部线程安全，发送/接收是拷贝操作，天然隔离；
* 队列还能阻塞等待（满/空），实现生产消费节奏匹配。

### Q2：`xQueueSend` 和 `xQueueSendFromISR` 区别？

答题要点：

* 中断里只能用带 FromISR 后缀的 API；
* FromISR 版本会通过参数返回"是否有更高优先级任务被唤醒"，
  由调用方决定是否切换（portYIELD_FROM_ISR）。

### Q3：队列深度怎么定？

答题要点：

* 按生产速率与消费速率的峰值差估算；
* 太浅丢数据，太深浪费 RAM（每个元素占 sizeof(类型) 字节）；
* 本实验深度 10，元素 sizeof(SensorData)。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day07 FreeRTOS queue task communication"
```

---

## 11. 下一步计划（Next Step）

Day08：FreeRTOS 同步机制（Semaphore & Mutex，见 [day08.md](day08.md)）。
