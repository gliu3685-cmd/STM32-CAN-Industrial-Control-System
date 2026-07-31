#ifndef APP_TASK_H
#define APP_TASK_H

#include "cmsis_os.h"
#include "queue.h"


typedef struct
{
    uint16_t temperature;
    uint16_t speed;

}SensorData;


extern QueueHandle_t sensorQueue;


void SensorTask(void const * argument);

void ControlTask(void const * argument);


#endif
