/**
  ******************************************************************************
  * @file    app_prio.c
  * @brief   FreeRTOS 任务优先级实验（Day 11）
  *          功能：演示优先级反转，并对比二进制信号量与互斥锁的区别
  *          学习点：
  *            1. 任务状态（Running/Ready/Blocked/Suspended）与调度
  *            2. 抢占式调度与任务优先级
  *            3. 优先级反转问题（Priority Inversion）
  *            4. 互斥锁的优先级继承机制（Priority Inheritance）
  *
  * 实验原理：
  *   低优先级任务 LowWorker 持有共享资源并做长时间忙循环；
  *   中优先级任务 MidPrio 频繁抢占 CPU；
  *   高优先级任务 HighWaiter 等待共享资源。
  *   - 使用二进制信号量：无优先级继承，Low 被 Mid 反复抢占，High 被长时间饿死（反转）；
  *   - 使用互斥锁：FreeRTOS 自动把 Low 临时提升到 High 的优先级，Mid 无法抢占，
  *     High 快速获得资源（继承修复）。
  *   HighWaiter 打印实际等待时间，两次场景对比直观可见。
  ******************************************************************************
  */

#include "app_prio.h"
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/* 共享资源句柄与场景标志 */
static SemaphoreHandle_t xResSem;      /* 场景 A：二进制信号量（无继承） */
static SemaphoreHandle_t xResMutex;    /* 场景 B：互斥锁（有继承） */
static uint8_t           gUseMutex;    /* 当前场景选择 */

/* 忙循环计数器（volatile 防止被编译器优化掉） */
static volatile uint32_t gWork;

/**
  * @brief  加锁打印（防止多个任务输出交错）
  * @param  fmt: 格式串（支持 printf 格式参数）
  * @retval 无
  */
static void PrioPrint(const char *fmt, ...)
{
    va_list args;

    osMutexWait(uartMutexHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  低优先级任务：持有共享资源并做长时间忙循环
  * @param  pv: 未使用
  * @retval 无
  */
static void LowWorker(void *pv)
{
    uint32_t i;

    /* 获取共享资源（按场景选择信号量或互斥锁） */
    if (gUseMutex)
        xSemaphoreTake(xResMutex, portMAX_DELAY);
    else
        xSemaphoreTake(xResSem, portMAX_DELAY);

    PrioPrint("[PRIO] Low: got resource, working...\r\n");

    /* 忙循环模拟长时间工作：不主动让出 CPU */
    for (i = 0; i < 5000000UL; i++)
    {
        gWork = i;
    }

    PrioPrint("[PRIO] Low: work done, releasing\r\n");

    if (gUseMutex)
        xSemaphoreGive(xResMutex);
    else
        xSemaphoreGive(xResSem);

    vTaskDelete(NULL);
}

/**
  * @brief  中优先级任务：忙循环抢占 CPU，模拟"与资源无关但不断打断"的任务
  * @param  pv: 未使用
  * @retval 无
  */
static void MidPrio(void *pv)
{
    uint32_t k, j;

    for (k = 0; k < 15; k++)
    {
        /* 忙循环约 30-40ms，期间 CPU 被 Mid 独占（抢占 Low） */
        for (j = 0; j < 2000000UL; j++)
        {
            gWork = j;
        }
        osDelay(5);   /* 短暂让出，给 Low 一点点运行机会 */
    }

    vTaskDelete(NULL);
}

/**
  * @brief  高优先级任务：等待共享资源，统计实际等待时间
  * @param  pv: 未使用
  * @retval 无
  */
static void HighWaiter(void *pv)
{
    TickType_t tStart;
    TickType_t tEnd;
    uint32_t   waitMs;

    /* 让 LowWorker 先拿到资源（100ms 足够 Low 拿锁） */
    osDelay(100);

    tStart = xTaskGetTickCount();

    if (gUseMutex)
        xSemaphoreTake(xResMutex, portMAX_DELAY);
    else
        xSemaphoreTake(xResSem, portMAX_DELAY);

    tEnd = xTaskGetTickCount();
    waitMs = (uint32_t)(tEnd - tStart);

    if (gUseMutex)
    {
        PrioPrint("[PRIO] High: got mutex after %lu ms\r\n", (unsigned long)waitMs);
        xSemaphoreGive(xResMutex);
    }
    else
    {
        PrioPrint("[PRIO] High: got sem after %lu ms\r\n", (unsigned long)waitMs);
        xSemaphoreGive(xResSem);
    }

    vTaskDelete(NULL);
}

/**
  * @brief  运行一个场景：创建低/中/高三个任务，等待它们跑完
  * @param  useMutex: 1 = 互斥锁场景，0 = 二进制信号量场景
  * @retval 无
  */
static void RunScenario(uint8_t useMutex)
{
    gUseMutex = useMutex;

    if (useMutex)
        PrioPrint("[PRIO] ==== Scenario B: Mutex (priority inheritance) ====\r\n");
    else
        PrioPrint("[PRIO] ==== Scenario A: Binary Semaphore (NO inheritance) ====\r\n");

    /* 创建低/中/高三个任务 */
    xTaskCreate(LowWorker,  "low",  128, NULL, 1, NULL);
    xTaskCreate(MidPrio,    "mid",  128, NULL, 2, NULL);
    xTaskCreate(HighWaiter, "high", 128, NULL, 3, NULL);

    /* 等待本场景三个任务全部结束（最长约 2s） */
    osDelay(2000);
}

/**
  * @brief  优先级反转演示任务（Day 11 实验主体）
  * @param  argument: 未使用
  * @retval 无
  */
void PrioDemoTask(void const * argument)
{
    /* 场景 A：二进制信号量 → 观察优先级反转 */
    xResSem = xSemaphoreCreateBinary();
    if (xResSem != NULL)
    {
        xSemaphoreGive(xResSem);   /* 初始为可用 */
    }
    RunScenario(0);

    /* 场景 B：互斥锁 → 观察优先级继承修复 */
    xResMutex = xSemaphoreCreateMutex();
    RunScenario(1);

    PrioPrint("[PRIO] Demo done\r\n");

    vTaskDelete(NULL);
}
