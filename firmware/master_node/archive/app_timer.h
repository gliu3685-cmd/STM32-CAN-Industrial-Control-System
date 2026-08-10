/**
  ******************************************************************************
  * @file    app_timer.h
  * @brief   FreeRTOS 软件定时器实验（Day 9）
  *          头文件：软件定时器接口声明
  ******************************************************************************
  */
#ifndef APP_TIMER_H
#define APP_TIMER_H

#include "FreeRTOS.h"
#include "timers.h"

/* 软件定时器句柄（周期 5000ms，自动重载） */
extern TimerHandle_t xPeriodicTimer;

/**
  * @brief  软件定时器回调函数：每 5s 打印一次系统运行时间
  * @param  xTimer: 触发本次回调的定时器句柄
  * @retval 无
  */
void TimerCallback(TimerHandle_t xTimer);

/**
  * @brief  创建并启动周期性软件定时器
  * @param  无
  * @retval 无
  */
void Timer_Init(void);

#endif /* APP_TIMER_H */
