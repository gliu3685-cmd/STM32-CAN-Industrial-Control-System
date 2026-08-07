# Day13 - FreeRTOS 异常检测与调试

## 1. 今日目标（Objectives）

本日进入"让系统稳定运行"的主题：嵌入式系统一旦跑飞，最难的不是修代码，
而是**定位问题**。本日掌握 FreeRTOS 提供的三层故障防线：

* 栈溢出检测（`configCHECK_FOR_STACK_OVERFLOW` + 溢出钩子）
* malloc 失败处理（`configUSE_MALLOC_FAILED_HOOK`，回顾 Day10）
* HardFault 定位（解析 Cortex-M4 压栈帧，找出错的 PC/LR）
* Assert 机制（`configASSERT` 参数校验与失败定位）
* 栈余量监控（`uxTaskGetStackHighWaterMark`）

通过本日实验，实现：

```
上电 → Demo1 栈溢出 → 复位 → Demo2 HardFault → 复位
    → Demo3 configASSERT → 复位 → 正常模式（栈余量监控）
```

三个破坏性实验用 **BKP 后备寄存器**跨复位记住进度，一次上电即可依次看到
三次故障演示，最后自动恢复正常运行。

---

## 2. 理论学习（Theory）

### 2.1 栈溢出：RTOS 第一杀手

每个 FreeRTOS 任务有自己的栈（创建时指定），栈存放局部变量、函数调用帧。
栈大小给少了，任务一深调用就**越界写入相邻内存**——可能踩坏另一个任务
的 TCB、堆管理块，甚至系统静默崩溃。这种错误不报错、难复现，最危险。

FreeRTOS 提供两档检测：

| 值 | 检测方式 | 代价 |
|---|---|---|
| 0 | 关闭 | 无 |
| 1 | 任务切换时只检查**栈指针是否越界** | 小 |
| 2 | 切换时检查栈指针 + **栈顶填充值（canary）是否被覆盖** | 略大 |

方法 2 更严格：任务创建时栈顶写入一个特殊填充值，若任务用栈超过
预留量，填充值被覆盖，切换时即被识破。检测到后调用
`vApplicationStackOverflowHook`。

**工程实践**：开发阶段开启检测，并用 `uxTaskGetStackHighWaterMark(handle)`
查询每个任务的"历史最小剩余栈"（栈水位），据此调栈大小。

### 2.2 malloc 失败：动态内存的另一条防线

`pvPortMalloc` 申请不到内存时返回 `NULL`。若代码不做检查就使用返回值，
野指针直接写崩系统。开启 `configUSE_MALLOC_FAILED_HOOK` 后，申请失败还会
调用 `vApplicationMallocFailedHook`，可在其中记录日志、进入错误处理或复位。

正确姿势：**调用处先判 NULL，钩子里做系统级兜底**，两道防线都要有。

### 2.3 HardFault：Cortex-M4 的"最后一根稻草"

Cortex-M4 有三级可配置故障：MemManage（内存管理）、BusFault（总线）、
UsageFault（用法）。默认三者未使能时，**任何故障都升级为 HardFault**。

故障发生时硬件自动把 8 个寄存器压栈：

```
[0] R0  [1] R1  [2] R2  [3] R3  [4] R12  [5] LR  [6] PC  [7] xPSR
```

`PC` 就是**出错时正在执行的指令地址**——定位的钥匙。取压栈帧的方法：
查看 `LR`（EXC_RETURN）的 bit2，`0` 表示故障前用 MSP，`1` 表示用 PSP，
据此取对应栈指针作为帧基址。

拿到 PC 后两种定位方式：

* 在 map 文件（`f407_blink.map`）里搜索地址附近符号；
* 用 addr2line 直接翻译：

```bash
arm-none-eabi-addr2line -e f407_blink.elf -f -C 0x0800xxxx
```

### 2.4 Assert：把"不可能"变成"可见"

`configASSERT(x)` 是 FreeRTOS 内核的运行时参数校验宏：条件为假即触发。
内核在队列/信号量等 API 的参数检查处大量调用它。默认配置是失败后
关中断死循环——本日升级为**打印文件与行号后复位**，直接把内核断言
失败的位置暴露出来。

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

## 4. 实验实现

### 4.1 FreeRTOSConfig.h 修改（USER CODE 保护区）

```c
/* Day 13：启用栈溢出检测（方法 2） */
#define configCHECK_FOR_STACK_OVERFLOW          2
/* Day 13：启用栈余量查询 API */
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* Day 13：断言失败打印位置并复位 */
#define configASSERT( x ) if ((x) == 0) { taskDISABLE_INTERRUPTS(); \
    printf("[FAULT] ASSERT FAILED: %s line %d\r\n", __FILE__, __LINE__); \
    NVIC_SystemReset(); }
```

### 4.2 栈溢出演示（app_fault.c）

```c
static void OverflowRecurse(uint32_t depth)
{
    volatile uint8_t buf[128];        /* 每层 128 字节，快速撑爆 512B 栈 */
    buf[0] = (uint8_t)depth;
    vTaskDelay(pdMS_TO_TICKS(1));     /* 让出 CPU → 调度器切换时检测 */
    if (depth > 0)
        OverflowRecurse(depth - 1);   /* 递归 10 层，必爆 */
    (void)buf[0];
}
```

溢出触发后由 `vApplicationStackOverflowHook` 打印任务名并复位
（freertos.c USER CODE 区）。

### 4.3 HardFault 解析（stm32f4xx_it.c）

汇编入口判断 MSP/PSP，C 函数解析压栈帧并打印：

```c
void HardFault_Handler(void)
{
  __asm volatile(
    " tst lr, #4    \n"
    " ite eq        \n"
    " mrseq r0, msp \n"
    " mrsne r0, psp \n"
    " b HardFault_Handler_C \n");
}

void HardFault_Handler_C(uint32_t *stack)
{
    /* stack[0..7] = R0 R1 R2 R3 R12 LR PC xPSR */
    printf("[FAULT] PC =0x%08lX  LR =0x%08lX ...\r\n", ...);
    /* 同时打印 CFSR/BFAR 判断故障类型与出错地址 */
    NVIC_SystemReset();
}
```

演示任务故意写非法地址触发：

```c
pBad = (volatile uint32_t *)0xDEADBEEFUL;
*pBad = 0x55U;      /* 精确总线错误 → HardFault */
```

### 4.4 跨复位进度控制（BKP 后备寄存器）

```c
#define BKP_DR1_ADDR  (0x40006C04UL)   /* F407 备份数据寄存器 DR1 */

__HAL_RCC_PWR_CLK_ENABLE();
HAL_PWR_EnableBkUpAccess();            /* 解除备份域写保护 */
```

每次故障演示前先写进度，复位后按进度进入下一个演示；
三次演示完成后进入正常栈监控模式。

---

## 5. 实验过程（Experiment）

烧录后打开串口（115200, 8N1），预期依次看到：

```
== Day13 Demo 1/3: Stack Overflow Detection ==
[FAULT] StackOverflow task started (stack=128 words)
[FAULT] STACK OVERFLOW detected! task=ovf     ← 复位

== Day13 Demo 2/3: HardFault Location ==
[FAULT] HardFault task: writing 0x55 to 0xDEADBEEF...
[FAULT] ##### HARD FAULT #####
[FAULT] PC =0x0800xxxx  LR =0x0800xxxx  xPSR=0x01000000
[FAULT] CFSR=0x00008200 (DACCVIOL=1 ...)      ← 数据访问违例
[FAULT] BFAR=0xDEADBEEF                        ← 出错地址
[FAULT] Locate: arm-none-eabi-addr2line ...    ← 复位

== Day13 Demo 3/3: configASSERT Mechanism ==
[FAULT] Assert task: calling configASSERT(0)...
[FAULT] ASSERT FAILED: app_fault.c line xxx   ← 复位

== Day13 all demos done, enter normal stack monitor mode ==
[FAULT] stack monitor: self high-water=... words ...
```

验证要点：

* Demo1：串口出现 `STACK OVERFLOW detected! task=ovf`；
* Demo2：CFSR 中 `DACCVIOL=1`、BFAR=`0xDEADBEEF`，与故意写坏地址吻合；
  PC 值可粘贴给 addr2line 定位到 `HardFaultTask`；
* Demo3：断言打印的文件/行号就是 `configASSERT(0)` 调用处；
* 正常模式：每 5s 打印一次栈余量。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1

问题：三个破坏性实验都会"死机"，演示一次就结束了，无法连贯展示。

原因：栈溢出/HardFault/Assert 发生后系统状态不可信，必须复位恢复；
复位后进度丢失，又从头演示。

解决：用 **BKP 后备寄存器**（系统复位不清除）记录实验进度，每个演示
开始前先写进度再触发故障，复位后自动进入下一阶段。顺带实践了
"复位后保留状态"的工程技巧（掉电保存、看门狗复位后诊断都用它）。

### Problem 2

问题：`HardFault_Handler` 里只有 `while(1)`，故障后只知道死机，不知在哪。

原因：默认中断服务程序没有解析故障现场。

解决：用内联汇编读取 EXC_RETURN 判断 MSP/PSP，把硬件自动压栈的 8 个
寄存器帧交给 C 函数打印 PC/LR/CFSR/BFAR，再用 addr2line 定位函数。

---

## 7. 今日成果（Result）

完成：

* [x] 栈溢出检测（方法 2）与 `vApplicationStackOverflowHook`
* [x] malloc 失败钩子回顾（Day10 基础上形成完整故障处理链路）
* [x] HardFault 压栈帧解析（PC/LR/R0-R3/xPSR + CFSR/BFAR）
* [x] `configASSERT` 升级：失败打印文件行号并复位
* [x] `uxTaskGetStackHighWaterMark` 栈余量监控
* [x] BKP 后备寄存器跨复位进度控制
* [x] 编译通过（0 errors, 0 warnings）

当前 FreeRTOS 架构：

```
FreeRTOS Scheduler
    ├── LED Task（500ms 翻转）
    ├── Sensor Task → Queue → Control Task（Temp/Speed）
    ├── Semaphore Task（TIM3 ISR 信号量通知）
    ├── Timer Service Task（5s 周期打印 uptime）
    ├── Memory Task（heap 监控，每 5s）
    ├── Prio Demo Task（优先级反转，一次性）
    ├── Event Demo Task（事件组，一次性）
    ├── Fault Demo Task（栈溢出/HardFault/Assert，一次性）
    └── Stack Monitor Task（栈水位，每 5s）
```

---

## 8. 工程总结（Engineering Summary）

## 故障处理要分"检测-定位-恢复"三层

检测层：栈溢出钩子、malloc 失败钩子、`configASSERT`——把问题暴露出来；
定位层：HardFault 现场打印 PC/寄存器——把问题地址找出来；
恢复层：复位、错误处理、看门狗——让系统能回来。

## 栈大小不是拍脑袋

开发阶段开 `configCHECK_FOR_STACK_OVERFLOW=2` + 周期打印
`uxTaskGetStackHighWaterMark`，用数据决定每个任务的栈大小，比猜稳得多。

---

## 9. 面试问答（Interview Prep）

### Q1：任务栈溢出会有什么后果？怎么检测和预防？

答题要点：

* 后果：越界写入相邻内存，可能破坏其他任务 TCB/堆，系统静默崩溃，难复现；
* 检测：`configCHECK_FOR_STACK_OVERFLOW`（1 查栈指针，2 加查栈顶填充值）、
  `vApplicationStackOverflowHook` 回调；
* 预防：用 `uxTaskGetStackHighWaterMark` 实测余量，留出 20% 以上裕量，
  避免深递归和大局部数组。

### Q2：HardFault 如何定位？现场有哪些信息可用？

答题要点：

* 硬件自动压栈 R0-R3/R12/LR/PC/xPSR，PC 是出错指令地址；
* EXC_RETURN 的 bit2 判断 MSP/PSP，取对应栈指针解析帧；
* CFSR 细分故障类型（IACCVIOL/DACCVIOL/PRECISERR…），BFAR/MMFAR 给出
  出错数据地址；
* PC 用 addr2line 或 map 文件翻译成函数。

### Q3：malloc 失败怎么处理？

答题要点：

* 调用处必须判 NULL；
* 开启 `configUSE_MALLOC_FAILED_HOOK` 做系统级兜底（记日志/复位/安全模式）；
* 嵌入式里尽量避免运行时大块动态分配，用静态内存或固定池。

### Q4：`configASSERT` 在生产环境该不该开？

答题要点：

* 开发/调试阶段开启，快速暴露参数错误；
* 生产环境权衡：断言失败会停机，通常改为记录错误并进入安全状态
  （而不是裸死循环）；
* 面试加分：答出 `configASSERT` 是编译期宏、可裁剪。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day13 fault detection & debug (stack overflow, HardFault, assert)"
```

---

## 11. 下一步计划（Next Step）

Day14：

* Phase 2 总结 + 综合架构设计（任务划分、优先级分配、同步机制选型）
* 搭建最终 RTOS 框架：CAN Rx/Tx、Control、Motor、Fault Monitor、Debug

之后进入 Phase 3：

```
CAN 通信任务（F407 主控 + 双 F103 节点）
```
