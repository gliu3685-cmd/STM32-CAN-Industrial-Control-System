Day07 - FreeRTOS Queue任务通信实验

## 1. 今日目标

本日进入 FreeRTOS 实时系统阶段的任务通信学习。

在 Day06 中已经完成：

- FreeRTOS 基础移植
- 多任务创建
- UART Task
- Monitor Task
- Default Task

但是目前各个 Task 之间相互独立，没有数据交互。

工业控制系统中，不同任务之间需要进行可靠的数据传递，例如：

- 传感器采集任务读取数据
- 控制任务根据数据进行决策
- 通信任务发送状态信息


因此本日学习并实现：

- FreeRTOS Queue（消息队列）
- Producer-Consumer模型
- Task间数据传递
- Sensor Task → Queue → Control Task架构


最终实现：


SensorTask
|
|
FreeRTOS Queue
|
|
ControlTask


---

# 2. 理论学习

## 2.1 为什么需要Task通信

在裸机程序中通常采用：

```c
while(1)
{
    read_sensor();

    calculate();

    control_motor();
}

所有功能集中在一个循环中。

问题：

模块耦合严重
实时性差
功能扩展困难

FreeRTOS采用多任务架构：

        +--------------+
        | Sensor Task  |
        +--------------+
                |
                |
              Queue
                |
                |
        +--------------+
        | Control Task |
        +--------------+

每个任务负责独立功能，通过通信机制交换数据。

2.2 FreeRTOS Queue介绍

Queue（消息队列）是一种任务间通信机制。

特点：

FIFO（先进先出）
支持任务阻塞
支持数据复制
多任务安全

例如：

SensorTask发送：

Temperature = 25
Speed = 1000

Queue保存：

+----------------+
| SensorData     |
+----------------+

ControlTask读取：

Temperature
Speed

进行控制。

2.3 Producer-Consumer模型

本实验采用经典生产者-消费者模型。

Producer:

SensorTask

负责产生数据。

Consumer:

ControlTask

负责消费数据。

结构：

Producer

 SensorTask
      |
      |
      v

 +----------+
 | Queue    |
 +----------+

      |
      |

      v

Consumer

 ControlTask


这种结构也是工业控制系统常见设计方式。

未来CAN项目：

CAN Receive Task

       |
       |
       v

     Queue

       |
       |
       v

Control Task

       |
       |
       v

Motor Control

3. 工程实现
3.1 工程目录调整

新增Application层：

Core

├── Inc

│   ├── app_task.h

│   └── app_queue.h


└── Src

    ├── app_task.c

    └── app_queue.c


功能划分：

文件	功能
app_task.c	Task业务逻辑
app_task.h	Task接口声明
app_queue.c	Queue创建
app_queue.h	Queue接口声明
3.2 SensorData数据结构

定义传感器数据：

typedef struct
{
    uint16_t temperature;

    uint16_t speed;

}SensorData;

模拟：

temperature
speed

两个工业控制常见参数。

3.3 创建Queue

app_queue.c:

QueueHandle_t sensorQueue;


void Queue_Init(void)
{

    sensorQueue = xQueueCreate(
            10,
            sizeof(SensorData)
    );

}


创建：

Queue长度:

10

每个元素:

SensorData

3.4 SensorTask实现

功能：

模拟采集传感器数据
发送到Queue

代码：

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


流程：

产生数据

↓

xQueueSend()

↓

Queue

3.5 ControlTask实现

功能：

从Queue读取数据
输出控制信息

代码：

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


流程：

Queue

↓

xQueueReceive()

↓

处理数据

3.6 在FreeRTOS中创建任务

freertos.c:

初始化Queue：

Queue_Init();

创建SensorTask:

osThreadDef(sensorTask,
            SensorTask,
            osPriorityNormal,
            0,
            128);

创建ControlTask:

osThreadDef(controlTask,
            ControlTask,
            osPriorityHigh,
            0,
            128);

任务优先级：

ControlTask

High


SensorTask

Normal


体现实时控制思想。

4. 编译问题与解决
问题1：找不到app_task.h

错误：

fatal error:
app_task.h:
No such file or directory

原因：

CubeIDE没有正确添加include路径。

解决：

添加：

Core/Inc

到：

C/C++ Build

-> Settings

-> Include paths
问题2：undefined reference

错误：

undefined reference to SensorTask

undefined reference to ControlTask

undefined reference to Queue_Init

原因：

头文件存在，但是.c文件没有参与编译。

检查：

Core/Src

app_task.c

app_queue.c


重新加入工程后解决。

问题3：工程路径缓存

CubeIDE保存旧build配置。

解决：

执行：

Project

-> Clean

-> Build Project


最终恢复正常。

5. 实验结果
编译结果
Build Finished.

0 errors

0 warnings


说明：

FreeRTOS任务创建成功
Queue模块成功链接
Application层结构正常
运行结果

预期串口输出：

FreeRTOS UART Task Running

System Monitor Running

Temp:1 Speed:1000

Temp:2 Speed:1000

Temp:3 Speed:1000


说明：

SensorTask

      |

      v

Queue

      |

      v

ControlTask


数据通信成功。

6. 今日收获

完成FreeRTOS任务通信基础。

掌握：

Queue概念
FIFO机制
Producer-Consumer模型
xQueueCreate()
xQueueSend()
xQueueReceive()

工程能力提升：

从：

单循环裸机程序

升级为：

FreeRTOS多任务实时系统
7. 项目关联

本实验为后续工业控制系统通信架构做准备。

最终CAN项目架构：

              CAN Bus


Sensor Node

     |

 CAN Receive Task

     |

    Queue

     |

Control Task

     |

 PID Motor Control

     |

 Motor Driver


Day07实现的Queue机制将直接应用于：

CAN数据接收
电机控制
故障监控
