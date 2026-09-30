/**
  ******************************************************************************
  * @file    app_task.h
  * @brief   任务通信实验（Day 7）头文件：任务与数据结构接口声明
  ******************************************************************************
  */
#ifndef APP_TASK_H
#define APP_TASK_H

#include "cmsis_os.h"
#include "queue.h"

/**
  * @brief  传感器数据结构（模拟温度与转速）
  */
typedef struct
{
    uint16_t temperature;
    uint16_t speed;
} SensorData;

extern QueueHandle_t sensorQueue;

void SensorTask(void const * argument);
void ControlTask(void const * argument);

#endif /* APP_TASK_H */
