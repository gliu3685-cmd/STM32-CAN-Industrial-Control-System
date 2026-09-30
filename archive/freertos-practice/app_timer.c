/**
  ******************************************************************************
  * @file    app_timer.c
  * @brief   FreeRTOS 软件定时器实验（Day 9）
  *          功能：软件定时器每 5s 打印一次系统运行时间
  *          学习点：
  *            1. xTimerCreate / xTimerStart 创建并启动软件定时器
  *            2. 软件定时器回调运行在定时器服务任务上下文
  *            3. 临界区保护（taskENTER_CRITICAL / taskEXIT_CRITICAL）
  ******************************************************************************
  */

#include "app_timer.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"

/* 软件定时器句柄 */
TimerHandle_t xPeriodicTimer;

/**
  * @brief  软件定时器回调函数：每 5s 打印一次系统运行时间
  * @param  xTimer: 触发本次回调的定时器句柄
  * @retval 无
  *
  * 说明：回调运行在"定时器服务任务"（Timer Service Task）上下文中，
  *       不是中断上下文，因此可以使用普通 FreeRTOS API；
  *       回调内不要调用阻塞 API（如 vTaskDelay）。
  */
void TimerCallback(TimerHandle_t xTimer)
{
    /* 获取系统运行时间（单位：tick，1 tick = 1ms） */
    TickType_t now = xTaskGetTickCount();

    /* 临界区保护：防止打印过程中被更高优先级任务抢占导致输出交错 */
    taskENTER_CRITICAL();
    printf("Timer fired, uptime: %lu ms\r\n", (unsigned long)now);
    taskEXIT_CRITICAL();
}

/**
  * @brief  创建并启动周期性软件定时器
  * @param  无
  * @retval 无
  *
  * 创建参数：
  *   - 名称     "periodic"（仅用于调试）
  *   - 周期     5000ms（pdMS_TO_TICKS 将毫秒转换为 tick）
  *   - 自动重载 pdTRUE：定时器到期后自动重新计时
  *   - 回调     TimerCallback
  */
void Timer_Init(void)
{
    xPeriodicTimer = xTimerCreate(
            "periodic",                 /* 定时器名称 */
            pdMS_TO_TICKS(5000),        /* 周期：5000ms */
            pdTRUE,                     /* 自动重载模式 */
            (void *)0,                  /* 定时器 ID（暂不使用） */
            TimerCallback               /* 回调函数 */
    );

    if (xPeriodicTimer != NULL)
    {
        /* 启动定时器；xTimerStart 会向定时器命令队列发送命令 */
        xTimerStart(xPeriodicTimer, 0);
    }
}
