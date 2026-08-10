/**
  ******************************************************************************
  * @file    app_event.c
  * @brief   FreeRTOS 事件组实验（Day 12）
  *          功能：演示事件组的多条件等待（AND / OR），并用任务通知收尾
  *          学习点：
  *            1. xEventGroupCreate / xEventGroupSetBits / xEventGroupWaitBits
  *            2. 事件位（Bit）管理与多条件等待（AND / OR）
  *            3. 事件组 vs 信号量/队列的区别
  *            4. 任务通知（xTaskNotifyGive / ulTaskNotifyTake）轻量同步
  *
  * 实验流程：
  *   SensorTask 每 300ms 置位 EVT_DATA（模拟 ADC 采集完成）
  *   AlarmTask  每 1000ms 置位 EVT_ALARM（模拟告警触发）
  *   ControlTask：
  *     阶段 1（OR） ：任一事件到位即唤醒，观察触发频率（约 300ms 一次）
  *     阶段 2（AND）：两个事件都到位才唤醒，观察触发频率（约 1000ms 一次）
  *   完成后用任务通知告知演示组织者，统一清理任务。
  ******************************************************************************
  */

#include "app_event.h"
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/* 事件位定义 */
#define EVT_DATA     (1 << 0)   /* bit0：模拟 ADC 采集完成 */
#define EVT_ALARM    (1 << 1)   /* bit1：模拟告警触发 */

/* 事件组句柄 */
static EventGroupHandle_t xEventGroup;

/* 任务句柄（用于演示结束后的清理） */
static TaskHandle_t hSensorTask;
static TaskHandle_t hAlarmTask;
static TaskHandle_t hCtrlTask;
static TaskHandle_t hDemoOwner;

/**
  * @brief  加锁打印（支持 printf 格式参数，防止输出交错）
  * @param  fmt: 格式串
  * @retval 无
  */
static void EvtPrint(const char *fmt, ...)
{
    va_list args;

    osMutexWait(uartMutexHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  传感器任务：每 300ms 模拟 ADC 采集一次，置位 EVT_DATA
  * @param  pv: 未使用
  * @retval 无
  */
static void SensorTask(void *pv)
{
    uint32_t adc = 0;

    for (;;)
    {
        adc += 17;                          /* 模拟 ADC 读数递增 */
        xEventGroupSetBits(xEventGroup, EVT_DATA);
        EvtPrint("Sensor: ADC=%lu, set DATA\r\n", (unsigned long)adc);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

/**
  * @brief  告警任务：每 1000ms 模拟一次告警触发，置位 EVT_ALARM
  * @param  pv: 未使用
  * @retval 无
  */
static void AlarmTask(void *pv)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        xEventGroupSetBits(xEventGroup, EVT_ALARM);
        EvtPrint("Alarm: set ALARM\r\n");
    }
}

/**
  * @brief  控制任务：先演示 OR 等待，再演示 AND 等待
  * @param  pv: 未使用
  * @retval 无
  */
static void ControlTask(void *pv)
{
    EventBits_t bits;
    int i;

    /* 阶段 1：OR 模式——任一事件到位即唤醒 */
    EvtPrint("== Stage 1: OR (any bit wakes task) ==\r\n");
    for (i = 0; i < 5; i++)
    {
        bits = xEventGroupWaitBits(
                xEventGroup,
                EVT_DATA | EVT_ALARM,   /* 关心的位 */
                pdTRUE,                 /* 唤醒后自动清除这些位 */
                pdFALSE,                /* OR：任一置位即可 */
                portMAX_DELAY);
        EvtPrint("OR wake: %s\r\n", (bits & EVT_DATA) ? "DATA" : "ALARM");
    }

    /* 阶段 2：AND 模式——所有事件都到位才唤醒 */
    EvtPrint("== Stage 2: AND (ALL bits must be set) ==\r\n");
    for (i = 0; i < 3; i++)
    {
        bits = xEventGroupWaitBits(
                xEventGroup,
                EVT_DATA | EVT_ALARM,
                pdTRUE,                 /* 唤醒后自动清除 */
                pdTRUE,                 /* AND：全部置位才返回 */
                portMAX_DELAY);
        EvtPrint("AND wake: both ready (bits=0x%02X)\r\n", (unsigned)bits);
    }

    EvtPrint("Event demo done\r\n");

    /* 用任务通知告知组织者：演示结束（顺带演示任务通知用法） */
    xTaskNotifyGive(hDemoOwner);
    vTaskDelete(NULL);
}

/**
  * @brief  事件组演示任务（Day 12 实验主体）
  * @param  argument: 未使用
  * @retval 无
  */
void EventDemoTask(void const * argument)
{
    hDemoOwner = xTaskGetCurrentTaskHandle();

    xEventGroup = xEventGroupCreate();
    if (xEventGroup == NULL)
    {
        EvtPrint("Event group create failed!\r\n");
        vTaskDelete(NULL);
        return;
    }

    /* 创建三个演示任务 */
    xTaskCreate(SensorTask,  "sensor", 128, NULL, 2, &hSensorTask);
    xTaskCreate(AlarmTask,   "alarm",  128, NULL, 1, &hAlarmTask);
    xTaskCreate(ControlTask, "ctrl",   128, NULL, 3, &hCtrlTask);

    /* 阻塞等待 ControlTask 的任务通知（演示任务通知同步） */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* 演示结束，清理演示任务后删除自身 */
    vTaskDelete(hSensorTask);
    vTaskDelete(hAlarmTask);
    vTaskDelete(hCtrlTask);
    vTaskDelete(NULL);
}
