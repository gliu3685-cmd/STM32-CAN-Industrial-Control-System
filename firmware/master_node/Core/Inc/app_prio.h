/**
  ******************************************************************************
  * @file    app_prio.h
  * @brief   FreeRTOS 任务优先级实验（Day 11）
  *          头文件：优先级反转演示任务接口声明
  ******************************************************************************
  */
#ifndef APP_PRIO_H
#define APP_PRIO_H

#include "cmsis_os.h"

/**
  * @brief  优先级反转演示任务：信号量（反转）vs 互斥锁（继承）对比
  * @param  argument: 未使用
  * @retval 无
  */
void PrioDemoTask(void const * argument);

#endif /* APP_PRIO_H */
