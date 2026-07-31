#include "app_sync.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "main.h"


extern SemaphoreHandle_t xBinarySemaphore;
extern osMutexId uartMutexHandle;


void SemaphoreTask(void const * argument)
{

    while(1)
    {

        if(xSemaphoreTake(
            xBinarySemaphore,
            portMAX_DELAY
        ) == pdTRUE)
        {
        	osMutexWait(
        	                uartMutexHandle,
        	                osWaitForever
        	            );


        	printf("原神牛逼!\r\n");
        	osMutexRelease(
        	                uartMutexHandle
        	            );
            HAL_GPIO_TogglePin(
                GPIOF,
                GPIO_PIN_10
            );

        }

    }

}
