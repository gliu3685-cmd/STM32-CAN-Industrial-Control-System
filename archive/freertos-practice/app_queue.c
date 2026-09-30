/**
  ******************************************************************************
  * @file    app_queue.c
  * @brief   队列创建（Day 7）：SensorData 消息队列初始化
  *          注意：历史实验归档，Queue_Init 未在 freertos.c 调用（Day14 起收敛）
  ******************************************************************************
  */

#include "app_queue.h"
#include "app_task.h"
#include <stdio.h>

/* 传感器数据队列句柄 */
QueueHandle_t sensorQueue;

/**
  * @brief  创建传感器数据队列
  * @param  无
  * @retval 无
  */
void Queue_Init(void)
{
    sensorQueue = xQueueCreate(10, sizeof(SensorData));

    if (sensorQueue == NULL)
    {
        /* 创建失败：heap 不足或参数非法；工程中应在此进入错误处理 */
        printf("[QUEUE] sensorQueue create failed!\r\n");
    }
}
