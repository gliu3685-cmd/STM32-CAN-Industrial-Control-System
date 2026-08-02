# Day10 - FreeRTOS 内存管理（heap_1 ~ heap_5）

## 1. 今日目标（Objectives）

本日进入 FreeRTOS 实时系统内存管理机制的学习，重点掌握内核对象的内存来源与堆实现策略。

今日完成：

* 理解 FreeRTOS 动态内存的分配入口（`pvPortMalloc` / `vPortFree`）
* 掌握五种堆实现 heap_1 ~ heap_5 的算法与适用场景
* 学会用 `xPortGetFreeHeapSize` / `xPortGetMinimumEverFreeHeapSize` 监控堆水位
* 通过实验观察 heap_4 的**空闲块合并**行为（防碎片机制）
* 掌握 `configUSE_MALLOC_FAILED_HOOK` 分配失败钩子的用法

通过本日实验，实现：

```
MemoryTask
    ├── 分配 A(100) / B(200) / C(50)
    ├── 释放 B → 观察空闲块合并
    ├── 重新分配 D(80) → 观察空间复用
    ├── 全部释放 → 验证堆水位恢复
    └── 申请 2GB → 触发 MallocFailedHook
```

---

## 2. 理论学习（Theory）

### 2.1 动态内存从哪来：configTOTAL_HEAP_SIZE

FreeRTOS 使用动态内存创建任务、队列、信号量、定时器等内核对象。内核对象
所需内存统一来自一块**静态数组**（heap 区），大小由
`configTOTAL_HEAP_SIZE` 决定（本工程为 15360 字节 = 15KB）。

```c
#define configTOTAL_HEAP_SIZE   ((size_t)15360)
```

实际提供内存分配实现的是 `portable/MemMang/` 下的 heap_x.c 文件，
**同一时间只能编译一个**。本工程使用 `heap_4.c`。

### 2.2 五种堆实现对比

| 实现 | 算法 | 能否释放 | 碎片处理 | 适用场景 |
|---|---|---|---|---|
| heap_1 | 简单顺序分配，只增不减 | ❌ 不能 | 无碎片问题 | 永不删除任务/对象的极简系统 |
| heap_2 | 最佳匹配（best fit） | ✅ 能 | 有碎片，不合并 | 分配大小固定、不频繁增删 |
| heap_3 | 包装 C 库 `malloc/free` | ✅ 能 | 依赖链接器堆 | 需要用到标准库 `malloc` 的场景 |
| heap_4 | 首次匹配（first fit）+ **相邻空闲块合并** | ✅ 能 | 自动合并防碎片 | **大多数工程默认选择（本工程）** |
| heap_5 | 同 heap_4，支持**多个不连续内存区** | ✅ 能 | 自动合并 | 多个 RAM 分区（如内部 SRAM + 外部 SDRAM） |

选择要点：

* 任务可能被删除 → 至少 heap_2；
* 分配大小不定且频繁增删 → heap_4（合并机制显著降低碎片）；
* 内存分布在多个物理区域 → heap_5；
* 要求确定性最高、绝无删除 → heap_1。

### 2.3 heap_4 的合并机制（本日实验核心）

heap_4 的空闲块按地址升序挂链表，释放时：

```
释放 B（200 字节）
    ↓
检查 B 的前后邻居
    ↓
若相邻块也是空闲 → 合并成一个大块
    ↓
避免"小碎片堆积导致大分配失败"
```

这正是 heap_4 相比 heap_2 的关键优势。

### 2.4 堆水位监控与失败钩子

```c
xPortGetFreeHeapSize();              // 当前剩余堆
xPortGetMinimumEverFreeHeapSize();   // 历史最低剩余（水位最低点）
```

分配失败时（堆不足），若开启钩子，内核会调用：

```c
#define configUSE_MALLOC_FAILED_HOOK   1
// 实现：
void vApplicationMallocFailedHook(void);
```

工业项目中通常在此记录错误标志或复位系统。

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

## 4. 内存演示任务实现（app_mem.c）

### 4.1 堆状态打印（互斥锁保护）

```c
static void MemPrintState(const char *tag)
{
    osMutexWait(uartMutexHandle, osWaitForever);
    printf("[MEM] %-24s free=%5u min=%5u\r\n",
           tag,
           (unsigned)xPortGetFreeHeapSize(),
           (unsigned)xPortGetMinimumEverFreeHeapSize());
    osMutexRelease(uartMutexHandle);
}
```

### 4.2 实验主流程

```c
void MemoryTask(void const * argument)
{
    /* 1. 初始状态 */
    MemPrintState("initial");

    /* 2. 分配三块不同大小并写入数据验证 */
    pA = pvPortMalloc(100);
    pB = pvPortMalloc(200);
    pC = pvPortMalloc(50);
    MemPrintState("malloc A100 B200 C50");

    /* 3. 释放中间块，观察合并 */
    vPortFree(pB);
    MemPrintState("free B(200)");

    /* 4. 重新分配 80 字节，观察空间复用 */
    pD = pvPortMalloc(80);
    MemPrintState("malloc D80 reuse");

    /* 5. 全部释放，堆水位应恢复 */
    vPortFree(pA); vPortFree(pC); vPortFree(pD);
    MemPrintState("free all");

    /* 6. 申请 2GB，触发失败钩子 */
    pBig = pvPortMalloc(0x7FFFFFFFUL);
    MemPrintState("malloc 2GB -> NULL");
    ...
}
```

### 4.3 启用失败钩子

`FreeRTOSConfig.h`（USER CODE 保护区）：

```c
#define configUSE_MALLOC_FAILED_HOOK   1
```

`freertos.c`（USER CODE Application 区）：

```c
void vApplicationMallocFailedHook(void)
{
    taskENTER_CRITICAL();
    printf("[MEM] !! MALLOC FAILED HOOK !!\r\n");
    taskEXIT_CRITICAL();
}
```

说明：钩子可能在任何上下文被调用，因此用临界区而非互斥锁。

---

## 5. 实验过程（Experiment）

### 实验 1：heap_4 分配 / 释放 / 合并观察

预期现象（数字以实际烧录为准）：

```
[MEM] initial                 free=11296 min=11296
[MEM] malloc A100 B200 C50    free=10916 min=10916
[MEM] free B(200)             free=11124 min=10916
[MEM] malloc D80 reuse        free=11036 min=10916
[MEM] free all                free=11296 min=10916
```

验证：

* `free` 随分配减少、随释放增加；
* 释放 B 后再分配 D，`free` 只减少约 88 字节，说明 D **复用了 B 的空闲块**（而非从堆尾新切一块），heap_4 合并机制生效；
* 全部释放后 `free` 恢复到初始值 11296，证明空闲块合并后无泄漏、无小碎片残留。

### 实验 2：分配失败钩子

预期现象：

```
[MEM] malloc 2GB -> NULL      free=11296 min=10916
[MEM] !! MALLOC FAILED HOOK !!
[MEM] Hook fired, allocation rejected
[MEM] MemoryTask done
```

验证：

* 申请 0x7FFFFFFF 字节必然失败，`pvPortMalloc` 返回 NULL；
* 失败瞬间内核调用 `vApplicationMallocFailedHook`，串口输出钩子信息；
* 任务检查 NULL 后安全退出，未发生内存越界。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：在 `vApplicationMallocFailedHook` 中直接使用 UART 互斥锁是否安全？

原因：钩子可能在调度器尚未启动时被调用（例如启动阶段创建对象失败），此时互斥锁 API 依赖调度器，存在死锁风险。

解决：钩子内改用 `taskENTER_CRITICAL` / `taskEXIT_CRITICAL` 临界区打印，不依赖调度器状态，任何上下文均安全。

### Problem 2

问题：为什么 heap_2 也能释放内存，工程却选择 heap_4？

原因：本工程后续 CAN 通信、电机控制任务需要动态创建/删除任务并传递大小不定的数据块，heap_2 不合并空闲块，长时间运行会产生碎片导致分配失败。

解决：选用 heap_4，利用其"首次匹配 + 相邻合并"特性降低碎片，适合长期运行的控制系统。

---

## 7. 今日成果（Result）

完成：

* [x] 理解 heap_1 ~ heap_5 的算法与适用场景
* [x] `pvPortMalloc` / `vPortFree` 动态分配/释放
* [x] 堆水位监控（free / min ever）
* [x] heap_4 空闲块合并与空间复用验证
* [x] `configUSE_MALLOC_FAILED_HOOK` 失败钩子
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
    └── Memory Task（heap 演示一次 + 每 5s 周期监控堆状态）
```

---

## 8. 工程总结（Engineering Summary）

> 更新：MemoryTask 实验部分运行一次后不删除自身，而是转入
> 周期监控模式（每 5s 打印一次 `[MEM] monitor free=.. min=..`），
> 便于随时在串口观察堆水位，也更符合实际工程的内存监控用法。

本日学习重点：

## 内存管理是"选型 + 监控"问题

heap 实现不是越高级越好，而是按场景选择：简单固定系统用 heap_1，
通用系统用 heap_4，多内存区用 heap_5。工程定型后靠
`xPortGetFreeHeapSize` 持续监控水位，避免运行中内存耗尽。

## 释放后要置 NULL

`vPortFree` 之后把指针置 NULL，防止野指针重复释放；每次
`pvPortMalloc` 后检查返回值是否为 NULL 再使用，这是嵌入式内存
使用的两条铁律。

---

## 9. 面试问答（Interview Prep）

### Q1：FreeRTOS 的 heap_1 ~ heap_5 有什么区别？实际工程怎么选？

答题要点：

* heap_1：只分配不释放，永不产生碎片，用于"任务创建后永不被删除"的简单系统；
* heap_2：支持释放（最佳匹配），但空闲块不合并，长时间运行会产生碎片；
* heap_3：直接包装 C 库 `malloc/free`，依赖链接器配置的堆大小，线程安全依赖调度器挂起；
* heap_4：支持释放 + **相邻空闲块自动合并**，是通用工程的默认选择；
* heap_5：算法同 heap_4，额外支持把多个不连续内存区（如内部 SRAM + 外部 SDRAM）合并管理。

选型口诀：**有删除用 heap_2 起，怕碎片用 heap_4，多内存区用 heap_5，追求极简确定用 heap_1**。

### Q2：heap_4 是怎么解决内存碎片问题的？

答题要点：

* 空闲块按地址升序组织成链表；
* 释放内存时检查相邻块是否空闲，是则合并成一个大块；
* 分配采用首次匹配（first fit），速度较快；
* 效果：避免"总空闲量充足、但连续空间不足"导致的分配失败。

### Q3：`pvPortMalloc` 和 C 库 `malloc` 有什么区别？

答题要点：

* `pvPortMalloc` 从 FreeRTOS 静态数组（`configTOTAL_HEAP_SIZE`）分配，不依赖链接器堆；
* 内部会自动挂起调度器/进入临界区，保证多任务安全；
* `malloc` 是否线程安全取决于 C 库实现，且内存来自链接脚本的 heap 段；
* 嵌入式 FreeRTOS 工程中内核对象统一用 `pvPortMalloc`，两者混用会各自消耗不同内存池。

### Q4：任务的内存是怎么分配的？栈大小怎么确定？

答题要点：

* 动态创建任务时，TCB 和任务栈都从 heap 分配（栈大小以"字"为单位，128 = 512 字节）；
* 栈大小由任务的局部变量、函数调用深度、printf 等决定，宁大勿小；
* 检查手段：`uxTaskGetStackHighWaterMark()` 查看栈剩余高水位；
* 若开启 `configCHECK_FOR_STACK_OVERFLOW`，栈溢出时触发钩子。

### Q5：内存分配失败怎么办？怎么预防？

答题要点：

* 开启 `configUSE_MALLOC_FAILED_HOOK`，分配失败时进入 `vApplicationMallocFailedHook`，工业中通常记录错误并复位；
* 每次 `pvPortMalloc` 后必须检查返回值是否为 NULL；
* 预防：估算峰值内存、用 `xPortGetMinimumEverFreeHeapSize` 监控历史最低水位、释放后指针置 NULL 防止重复释放。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day10 FreeRTOS heap memory management demo (heap_4)"
```

---

## 11. 下一步计划（Next Step）

Day11：

* 多任务采集 Demo（Task_ReadSensor 100ms → 滑动平均 → 串口上报）
* 任务栈高水位检查（`uxTaskGetStackHighWaterMark`）
* 定时采集触发任务设计

为后续：

```
CAN 通信任务
电机控制任务
故障监控任务
```

建立实时调度与资源管理基础。
