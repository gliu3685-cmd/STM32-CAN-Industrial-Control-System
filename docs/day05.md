# Day5 - FreeRTOS Basic Integration


## Objective


从裸机开发进入实时操作系统开发。


目标：

- 集成FreeRTOS
- 创建Task
- 理解Scheduler
- 实现多任务运行


---

# Why RTOS?


Without RTOS:


```c
while(1)
{

Task1();

Task2();

Task3();

}


缺点：

代码耦合
扩展困难
实时性不足
FreeRTOS Architecture
Application


 |

FreeRTOS Scheduler


 |

HAL Driver


 |

STM32 Hardware

Task Design

Created Tasks:

LED Task

Function:

GPIO periodic control

周期:

500ms

UART Debug Task

Function:

Output RTOS status

Output:

FreeRTOS UART Task Running

FreeRTOS Concepts Learned
Scheduler

负责：

Task切换
CPU分配
Task

每个Task：

拥有：

Stack
Priority
State
Implementation

Create Task:

osThreadDef();

osThreadCreate();


Delay:

osDelay();

Debug Experience

Problem:

UART没有输出。

Cause:

USB-TTL TX/RX接反。

Solution:

PA9  -> RX

PA10 -> TX

Result

Verified:

✅ FreeRTOS Kernel Running

✅ Scheduler Running

✅ Multiple Tasks Running
```
