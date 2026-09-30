/**
  ******************************************************************************
  * @file    app_task.c
  * @brief   任务通信实验（Day 7）：SensorTask → Queue → ControlTask
  *          注意：历史实验归档，任务未在 freertos.c 注册（Day14 起收敛）
  ******************************************************************************
  */

#include "app_task.h"
#include <stdio.h>
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/**
  * @brief  生产者任务：每 1s 模拟采集一次数据并发送到队列
  * @param  argument: 未使用
  * @retval 无
  */
void SensorTask(void const * argument)
{
    SensorData data;

    (void)argument;

    data.temperature = 0;
    data.speed = 1000;

    for (;;)
    {
        data.temperature++;

        xQueueSend(sensorQueue, &data, portMAX_DELAY);

        osDelay(1000);
    }
}

/**
  * @brief  消费者任务：从队列接收数据并打印（UART 互斥锁保护）
  * @param  argument: 未使用
  * @retval 无
  */
void ControlTask(void const * argument)
{
    SensorData recv;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(sensorQueue, &recv, portMAX_DELAY) == pdPASS)
        {
            osMutexWait(uartMutexHandle, osWaitForever);
            printf("Temp:%d Speed:%d\r\n", recv.temperature, recv.speed);
            osMutexRelease(uartMutexHandle);
        }
    }
}
