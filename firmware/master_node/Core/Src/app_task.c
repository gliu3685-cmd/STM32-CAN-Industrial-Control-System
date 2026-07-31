#include "app_task.h"
#include "stdio.h"


void SensorTask(void const * argument)
{

    SensorData data;

    data.temperature = 0;
    data.speed = 1000;


    while(1)
    {

        data.temperature++;


        xQueueSend(
                sensorQueue,
                &data,
                portMAX_DELAY
        );


        osDelay(1000);

    }

}



void ControlTask(void const * argument)
{

    SensorData recv;


    while(1)
    {

        if(xQueueReceive(
                sensorQueue,
                &recv,
                portMAX_DELAY)==pdPASS)
        {


            printf(
                "Temp:%d Speed:%d\r\n",
                recv.temperature,
                recv.speed
            );


        }

    }

}
