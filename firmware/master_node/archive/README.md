# 历史实验代码归档（Day 3 ~ Day 13）

本目录存放 FreeRTOS 学习阶段（Phase 2 之前）的演示实验代码。

## 归档原因

Day14 完成综合架构设计后，主工程 `Core/Src`、`Core/Inc` 只保留正式系统
代码（`app_arch.c/h`）与故障防线（钩子、configASSERT、HardFault 解析）。
学习期的演示任务已从 `freertos.c` 移除注册，为保持主工程干净，
对应代码文件统一归档到本目录（不再参与编译）。

## 归档文件与对应实验

| 文件 | 对应实验 |
|---|---|
| app_task.c/h | Day 7：队列任务通信（SensorTask → Queue → ControlTask） |
| app_queue.c/h | Day 7：传感器数据队列创建 |
| app_sync.c/h | Day 8：二值信号量 + TIM3 中断同步（原神牛逼） |
| app_timer.c/h | Day 9：软件定时器 5s 周期打印 uptime |
| app_mem.c/h | Day 10：heap_4 内存分配/释放/合并/失败钩子 |
| app_prio.c/h | Day 11：优先级反转（信号量 vs 互斥锁） |
| app_event.c/h | Day 12：事件组 AND/OR 等待 + 任务通知 |
| app_fault.c/h | Day 13：栈溢出 / HardFault / configASSERT 演示 |

## 重新启用方法

1. 把需要的 `.c` 复制回 `firmware/master_node/Core/Src/`、`.h` 复制回
   `Core/Inc/`；
2. 在 `freertos.c` 的 `USER CODE RTOS_THREADS` 区恢复对应任务的
   `osThreadDef` / `osThreadCreate` 创建块（可参考对应 day 文档）；
3. 重新编译。

注意：`app_fault.c` 的 Day13 故障演示依赖 BKP 后备寄存器进度控制，
重用时需确认备份域访问已使能（`EnableBackupDomain`）。

## 故障防线说明

以下故障防护**不依赖归档文件**，始终保留在主工程中：

* `configCHECK_FOR_STACK_OVERFLOW = 2` + `vApplicationStackOverflowHook`
* `configUSE_MALLOC_FAILED_HOOK` + `vApplicationMallocFailedHook`
* `configASSERT`（打印文件行号后复位）
* `HardFault_Handler` / `HardFault_Handler_C`（现场寄存器解析）
