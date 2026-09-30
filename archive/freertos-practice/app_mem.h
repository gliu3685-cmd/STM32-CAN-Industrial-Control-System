/**
  ******************************************************************************
  * @file    app_mem.h
  * @brief   FreeRTOS 内存管理实验（Day 10）
  *          头文件：堆内存演示任务接口声明
  ******************************************************************************
  */
#ifndef APP_MEM_H
#define APP_MEM_H

#include "cmsis_os.h"

/**
  * @brief  堆内存演示任务：观察 heap_4 分配/释放/合并/失败行为
  *         实验完成后转为每 5s 周期打印堆状态（监控模式）
  * @param  argument: 未使用
  * @retval 无
  */
void MemoryTask(void const * argument);

#endif /* APP_MEM_H */
