/**
  ******************************************************************************
  * @file    app_event.h
  * @brief   FreeRTOS 事件组实验（Day 12）
  *          头文件：事件组演示任务接口声明
  ******************************************************************************
  */
#ifndef APP_EVENT_H
#define APP_EVENT_H

#include "cmsis_os.h"

/**
  * @brief  事件组演示任务：多条件等待（AND / OR）+ 任务通知收尾
  * @param  argument: 未使用
  * @retval 无
  */
void EventDemoTask(void const * argument);

#endif /* APP_EVENT_H */
