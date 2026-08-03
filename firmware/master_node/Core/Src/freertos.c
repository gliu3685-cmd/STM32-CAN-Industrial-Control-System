/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <app_queue.h>
#include <app_task.h>
#include "app_sync.h"
#include "app_timer.h"
#include "app_mem.h"
#include "app_prio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
SemaphoreHandle_t xBinarySemaphore;
osMutexId uartMutexHandle;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId uartTaskHandle;
osThreadId monitorTaskHandle;
osThreadId sensorTaskHandle;
osThreadId controlTaskHandle;
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartUartTask(void const * argument);
void StartMonitorTask(void const * argument);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	xBinarySemaphore = xSemaphoreCreateBinary();

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
	osMutexDef(uartMutex);

	uartMutexHandle = osMutexCreate(osMutex(uartMutex));
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
	Timer_Init();
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
	Queue_Init();
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);
  osThreadDef(uartTask,
              StartUartTask,
              osPriorityLow,
              0,
              128);


  uartTaskHandle =
          osThreadCreate(osThread(uartTask), NULL);
  osThreadDef(monitorTask,
              StartMonitorTask,
              osPriorityHigh,
              0,
              128);


  monitorTaskHandle =
          osThreadCreate(osThread(monitorTask), NULL);
  /* USER CODE BEGIN RTOS_THREADS */
  osThreadDef(sensorTask,
            SensorTask,
            osPriorityNormal,
            0,
            128);


sensorTaskHandle =
        osThreadCreate(osThread(sensorTask), NULL);



osThreadDef(controlTask,
            ControlTask,
            osPriorityHigh,
            0,
            128);


controlTaskHandle =
        osThreadCreate(osThread(controlTask), NULL);

osThreadDef(semaphoreTask,
            SemaphoreTask,
            osPriorityNormal,
            0,
            128);


osThreadCreate(
            osThread(semaphoreTask),
            NULL);

osThreadDef(memoryTask,
            MemoryTask,
            osPriorityLow,
            0,
            256);


osThreadCreate(
            osThread(memoryTask),
            NULL);

osThreadDef(prioDemoTask,
            PrioDemoTask,
            osPriorityLow,
            0,
            256);


osThreadCreate(
            osThread(prioDemoTask),
            NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
	  HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);

	      osDelay(500);
  }
}
  void StartUartTask(void const * argument)
  {

      while(1)
      {osMutexWait(
              uartMutexHandle,
              osWaitForever
          );


      printf("UART START\r\n");

          osMutexRelease(
              uartMutexHandle
          );

          osDelay(100);

          osMutexWait(
              uartMutexHandle,
              osWaitForever
          );

          printf("UART END\r\n");

          osMutexRelease(
              uartMutexHandle
          );
      }
  }
      void StartMonitorTask(void const * argument)
      {

          while(1)
          {osMutexWait(
        		  uartMutexHandle,
        		  osWaitForever
        		 );


          printf("MONITOR START\r\n");


        		 osMutexRelease(
        		  uartMutexHandle
        		 );

              osDelay(100);

              osMutexWait(
            		  uartMutexHandle,
            		  osWaitForever
            		 );

              printf("MONITOR END\r\n");


        		 osMutexRelease(
        		  uartMutexHandle
        		 );
          }

      }/* USER CODE END StartDefaultTask */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  内存分配失败钩子：pvPortMalloc 申请失败时由内核调用
  * @param  无
  * @retval 无
  *
  * 说明：本工程堆空间不足时进入此函数。演示中打印提示后返回，
  *       调用方 pvPortMalloc 会返回 NULL；
  *       工业应用中通常在此进入错误处理或系统复位。
  */
void vApplicationMallocFailedHook(void)
{
    /* 用临界区而非互斥锁：hook 可能在调度器未启动时被调用 */
    taskENTER_CRITICAL();
    printf("[MEM] !! MALLOC FAILED HOOK !!\r\n");
    taskEXIT_CRITICAL();
}

/* USER CODE END Application */
