#include "app_queue.h"
#include "app_task.h"


QueueHandle_t sensorQueue;


void Queue_Init(void)
{

    sensorQueue = xQueueCreate(
            10,
            sizeof(SensorData)
    );


    if(sensorQueue != NULL)
    {

    }

}
