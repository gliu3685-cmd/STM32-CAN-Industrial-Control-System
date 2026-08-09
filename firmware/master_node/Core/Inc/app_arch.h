/**
  ******************************************************************************
  * @file    app_arch.h
  * @brief   FreeRTOS 综合架构设计实验（Day 14）
  *          头文件：系统架构任务接口声明
  ******************************************************************************
  */
#ifndef APP_ARCH_H
#define APP_ARCH_H

#include "cmsis_os.h"

/**
  * @brief  综合架构演示任务（Phase 2 收官）
  *         创建同步对象（Queue/EventGroup/Mutex/Timer）与全部系统任务：
  *         CAN Rx / CAN Tx / Control / Motor / Fault Monitor / Debug
  * @param  argument: 未使用
  * @retval 无
  */
void ArchDemoTask(void const * argument);

#endif /* APP_ARCH_H */
