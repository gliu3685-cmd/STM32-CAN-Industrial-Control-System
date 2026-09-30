/**
  ******************************************************************************
  * @file    app_fault.h
  * @brief   FreeRTOS 异常检测与调试实验（Day 13）
  *          头文件：故障演示任务接口声明
  ******************************************************************************
  */
#ifndef APP_FAULT_H
#define APP_FAULT_H

#include "cmsis_os.h"

/**
  * @brief  异常检测与调试演示任务
  *         依次演示：栈溢出检测 → HardFault 定位 → Assert 机制 → 栈余量监控
  * @param  argument: 未使用
  * @retval 无
  */
void FaultDemoTask(void const * argument);

#endif /* APP_FAULT_H */
