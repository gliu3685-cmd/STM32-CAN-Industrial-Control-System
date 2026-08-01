# Day6 - FreeRTOS任务管理与调度机制


## 今日目标

深入理解FreeRTOS任务运行机制。

学习：

- Task生命周期
- Task优先级
- FreeRTOS Tick机制
- osDelay与HAL_Delay区别
- 多任务调度过程


---

# 一、FreeRTOS任务模型


在裸机开发中：

```c
while(1)
{
    Task1();

    Task2();

    Task3();
}
所有功能集中在主循环中。

随着系统功能增加：

代码耦合增加
任务管理困难
实时性下降

引入FreeRTOS后：

              FreeRTOS Scheduler


                    |

        +-----------+-----------+

        |           |           |

     LED Task   UART Task   Monitor Task


系统通过调度器管理多个任务。

二、Task状态

FreeRTOS任务主要状态：

Ready（就绪）

任务已经创建，等待CPU调度。

Running（运行）

当前正在执行的任务。

Blocked（阻塞）

任务等待某个事件，例如：

延时结束
等待消息

例如：

osDelay(500);

任务进入Blocked状态。

Suspended（挂起）

任务被主动暂停。

三、Task优先级

FreeRTOS通过优先级决定任务调度顺序。

本次实验创建三个任务：

Monitor Task

优先级：

High

功能：

系统状态监控。

Default Task

优先级：

Normal

功能：

LED周期控制。

UART Task

优先级：

Low

功能：

串口调试输出。

任务结构：

High Priority

Monitor Task


Normal Priority

LED Task


Low Priority

UART Task

四、Tick机制

FreeRTOS需要时间基准管理任务调度。

系统Tick：

SysTick Timer

        |

FreeRTOS Kernel

        |

Task Scheduling


Tick用于：

延时管理
任务切换
时间统计
五、osDelay与HAL_Delay区别
HAL_Delay

裸机中：

HAL_Delay(500);

特点：

CPU阻塞等待
无法执行其他任务
osDelay

RTOS中：

osDelay(500);

特点：

当前任务进入Blocked状态
CPU释放
调度其他任务运行

因此：

FreeRTOS项目中推荐：

osDelay()

而不是大量使用：

HAL_Delay()
六、实验实现

创建三个FreeRTOS任务：

LED Task

功能：

控制GPIOF PIN9 LED

周期：

500ms

代码：

HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);

osDelay(500);
UART Task

功能：

串口输出任务运行状态。

输出：

FreeRTOS UART Task Running

周期：

1000ms
Monitor Task

功能：

模拟系统监控任务。

控制：

GPIOF PIN10

周期：

2000ms
七、实验结果

运行结果：

✅ FreeRTOS调度器正常运行

✅ 三个Task同时工作

✅ 不同优先级任务正常调度

✅ UART调试信息正常输出

现象：

LED1:

500ms周期闪烁

LED2:

2s周期闪烁

UART:

FreeRTOS UART Task Running
八、问题记录
问题：StartMonitorTask链接错误

错误：

undefined reference to StartMonitorTask

原因：

创建任务时声明了Task，但是没有实现函数。

解决：

增加：

void StartMonitorTask(void const * argument)
{
    while(1)
    {

    }
}
九、今日总结

通过Day6学习：

掌握FreeRTOS任务调度基本原理。

理解：

Task不是并行运行，而是由Scheduler进行切换
osDelay可以主动释放CPU
不同任务可以通过Priority管理实时性

为后续学习：

Queue
Semaphore
Mutex
CAN通信任务设计

打下基础。
```
