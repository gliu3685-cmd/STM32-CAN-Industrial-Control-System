/**
  ******************************************************************************
  * @file    app_mem.c
  * @brief   FreeRTOS 内存管理实验（Day 10）
  *          功能：演示 heap_4 动态内存的分配、释放、合并与失败处理
  *          学习点：
  *            1. pvPortMalloc / vPortFree 动态内存接口
  *            2. xPortGetFreeHeapSize / xPortGetMinimumEverFreeHeapSize
  *            3. heap_4 的空闲块合并机制（防碎片）
  *            4. configUSE_MALLOC_FAILED_HOOK 分配失败钩子
  ******************************************************************************
  */

#include "app_mem.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/**
  * @brief  打印一次堆状态（受互斥锁保护）
  * @param  tag: 步骤说明
  * @retval 无
  */
static void MemPrintState(const char *tag)
{
    osMutexWait(uartMutexHandle, osWaitForever);
    printf("[MEM] %-24s free=%5u min=%5u\r\n",
           tag,
           (unsigned)xPortGetFreeHeapSize(),
           (unsigned)xPortGetMinimumEverFreeHeapSize());
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  堆内存演示任务（Day 10 实验主体）
  * @param  argument: 未使用
  * @retval 无
  *
  * 实验流程：
  *   1. 打印初始堆状态
  *   2. 依次分配 100/200/50 字节并填充数据验证
  *   3. 释放中间块，观察空闲块合并
  *   4. 重新分配 80 字节，观察空间复用
  *   5. 全部释放，验证堆恢复到初始水位
  *   6. 故意申请超大内存，触发 MallocFailedHook
  */
void MemoryTask(void const * argument)
{
    uint8_t *pA = NULL;
    uint8_t *pB = NULL;
    uint8_t *pC = NULL;
    uint8_t *pD = NULL;
    uint8_t *pBig = NULL;
    int      i;

    /* 步骤 1：初始堆状态 */
    MemPrintState("initial");

    /* 步骤 2：分配三块不同大小的内存 */
    pA = pvPortMalloc(100);
    pB = pvPortMalloc(200);
    pC = pvPortMalloc(50);

    /* 写入数据验证内存确实可用 */
    for (i = 0; i < 100; i++) pA[i] = (uint8_t)i;
    for (i = 0; i < 200; i++) pB[i] = (uint8_t)(i & 0xFF);
    for (i = 0; i < 50;  i++) pC[i] = 0xAB;
    MemPrintState("malloc A100 B200 C50");

    /* 步骤 3：释放中间块 B，heap_4 会把相邻空闲块合并成大块 */
    vPortFree(pB);
    pB = NULL;
    MemPrintState("free B(200)");

    /* 步骤 4：重新分配 80 字节，应复用 B 释放的空间（而非从尾部切割） */
    pD = pvPortMalloc(80);
    memset(pD, 0, 80);
    MemPrintState("malloc D80 reuse");

    /* 步骤 5：全部释放，空闲块合并后应接近初始水位 */
    vPortFree(pA);
    vPortFree(pC);
    vPortFree(pD);
    pA = pC = pD = NULL;
    MemPrintState("free all");

    /* 步骤 6：故意申请超大内存，触发 MallocFailedHook 并返回 NULL */
    pBig = pvPortMalloc(0x7FFFFFFFUL);
    MemPrintState("malloc 2GB -> NULL");
    if (pBig == NULL)
    {
        osMutexWait(uartMutexHandle, osWaitForever);
        printf("[MEM] Hook fired, allocation rejected\r\n");
        osMutexRelease(uartMutexHandle);
    }

    /* 演示结束，删除自身任务释放其 TCB 与栈 */
    osMutexWait(uartMutexHandle, osWaitForever);
    printf("[MEM] MemoryTask done\r\n");
    osMutexRelease(uartMutexHandle);

    vTaskDelete(NULL);
}
