/**
  ******************************************************************************
  * @file    app_sync.c
  * @brief   同步机制实验（Day 8）：TIM3 中断 → 二值信号量 → 任务
  *          注意：历史实验归档，SemaphoreTask 未在 freertos.c 注册（Day14 起收敛）
  ******************************************************************************
  */

#include "app_sync.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "main.h"

extern SemaphoreHandle_t xBinarySemaphore;
extern osMutexId uartMutexHandle;

/**
  * @brief  信号量任务：等待 TIM3 中断释放的信号量后执行业务
  * @param  argument: 未使用
  * @retval 无
  */
void SemaphoreTask(void const * argument)
{
    (void)argument;

    for (;;)
    {
        if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdTRUE)
        {
            osMutexWait(uartMutexHandle, osWaitForever);
            printf("原神牛逼!\r\n");   /* 个人测试文案，正式项目建议规范日志 */
            osMutexRelease(uartMutexHandle);

            HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10);
        }
    }
}
