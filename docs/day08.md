# Day08 - FreeRTOS同步机制学习（Semaphore & Mutex）

## 1. 今日目标（Objectives）

本阶段进入 FreeRTOS 实时系统核心机制学习，重点掌握任务之间的同步与资源保护。

今日完成：

* 学习 FreeRTOS Binary Semaphore（二值信号量）
* 理解 ISR 与 Task 的同步模型
* 使用 TIM3 中断触发 Semaphore，实现事件通知
* 学习 Mutex（互斥锁）机制
* 使用 Mutex 保护 UART 共享资源
* 验证多任务环境下资源竞争问题

通过本日实验，实现从：

```
中断直接控制硬件
```

向：

```
中断通知任务
任务处理业务
```

的实时系统设计思想转变。

---

# 2. 理论学习（Theory）

## 2.1 Semaphore（二值信号量）

### 什么是 Semaphore？

Semaphore 是 FreeRTOS 中用于任务同步的机制。

主要作用：

> 一个任务等待某个事件发生，另一个任务负责通知。

典型模型：

```
        Event

          |
          |
          v

      Semaphore

          |
          |
          v

        Task
```

例如：

CAN接收中断：

```
CAN RX Interrupt

        |

        v

Give Semaphore

        |

        v

CAN Task处理数据

```

---

## 2.2 Binary Semaphore 工作流程

本实验：

```
TIM3 Interrupt

        |

        v

xSemaphoreGiveFromISR()

        |

        v

SemaphoreTask解除阻塞

        |

        v

xSemaphoreTake()

        |

        v

执行任务代码

```

其中：

### 创建信号量

```c
xSemaphoreCreateBinary();
```

### Task等待信号量

```c
xSemaphoreTake();
```

### 中断释放信号量

```c
xSemaphoreGiveFromISR();
```

---

# 2.3 Mutex（互斥锁）

## 为什么需要 Mutex？

多个任务可能同时访问同一个资源。

例如：

```
          UART1

            |

 ----------------------

 |          |          |

Task1     Task2     Task3

printf   printf   printf

```

如果没有保护：

可能出现：

```
Temp:1Speed:UART
```

数据交错。

---

## Mutex作用

Mutex保证：

> 同一时间只有一个任务访问共享资源。

工作流程：

```
Task

 |
 |
Take Mutex

 |
 |
访问资源

 |
 |
Release Mutex

```

其他任务：

```
Take Mutex

       |
       |
       v

等待

```

---

# 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2
* USB-TTL

软件：

* STM32CubeIDE
* FreeRTOS
* HAL Library

工程：

```
STM32-CAN-Industrial-Control-System
```

当前阶段：

```
Phase 2
FreeRTOS实时系统
```

---

# 4. Binary Semaphore 实现

## 4.1 创建 Semaphore

位置：

```
freertos.c
MX_FREERTOS_Init()
```

代码：

```c
xBinarySemaphore = xSemaphoreCreateBinary();
```

---

## 4.2 创建 Semaphore Task

创建任务：

```c
osThreadDef(semaphoreTask,
            SemaphoreTask,
            osPriorityHigh,
            0,
            128);


sensorTaskHandle =
        osThreadCreate(
            osThread(semaphoreTask),
            NULL
        );
```

---

## 4.3 SemaphoreTask实现

```c
void SemaphoreTask(void const * argument)
{

    while(1)
    {

        if(xSemaphoreTake(
             xBinarySemaphore,
             portMAX_DELAY
        ) == pdTRUE)
        {

            printf("Semaphore received!\r\n");


            HAL_GPIO_TogglePin(
                GPIOF,
                GPIO_PIN_10
            );

        }

    }

}
```

---

## 4.4 TIM3中断释放Semaphore

修改：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM3)
    {

        xSemaphoreGiveFromISR(
            xBinarySemaphore,
            NULL
        );

    }
}
```

实现：

```
TIM3

 |

 |

Semaphore

 |

 |

SemaphoreTask

 |

 |

LED10 Toggle

```

---

# 5. Mutex 实现

## 5.1 创建 UART Mutex

freertos.c:

```c
osMutexDef(uartMutex);


uartMutexHandle =
        osMutexCreate(
            osMutex(uartMutex)
        );
```

---

## 5.2 UART资源保护

修改任务：

```c
osMutexWait(
    uartMutexHandle,
    osWaitForever
);


printf("UART message\r\n");


osMutexRelease(
    uartMutexHandle
);
```

保护：

* UART Task
* Monitor Task
* Semaphore Task

---

# 6. 实验过程（Experiment）

## 实验1：Semaphore同步测试

现象：

LED9:

```
500ms翻转
```

LED10:

```
TIM3触发后翻转
```

串口：

```
Semaphore received!
Semaphore received!
Semaphore received!
```

验证：

```
TIM3 ISR

↓

Semaphore

↓

Task执行

```

---

## 实验2：Mutex UART保护

加入Mutex前：

可能出现：

```
Temp:8 Speed:1000
FreeRTOS UART Task Running
原神牛逼!
```

偶尔产生异常字符：

```

```

说明：

多个任务竞争UART资源。

---

加入Mutex后：

输出：

```
System Monitor Running
Temp:13 Speed:1000
FreeRTOS UART Task Running
原神牛逼!
```

所有消息保持完整。

---

## 实验3：制造竞争验证Mutex

增加：

```c
UART START

delay

UART END
```

测试结果：

```
MONITOR START
MONITOR END

UART START
UART END
```

没有出现：

```
MONITOR START
UART START
MONITOR END
UART END
```

证明：

Mutex成功保护临界资源。

---

# 7. 遇到的问题与解决（Problems & Solutions）

## Problem 1

### 问题：

编译错误：

```
implicit declaration of function 'printf'
```

### 原因：

app_sync.c缺少stdio声明。

### 解决：

添加：

```c
#include <stdio.h>
```

---

## Problem 2

### 问题：

```
osMutexWait undeclared
uartMutexHandle undeclared
```

### 原因：

多文件之间变量和API不可见。

### 解决：

添加：

```c
#include "cmsis_os.h"
```

并声明：

```c
extern osMutexId uartMutexHandle;
```

---

## Problem 3

### 问题：

LED10闪烁但Semaphore未验证。

### 原因：

TIM3中断直接控制GPIO：

```c
HAL_GPIO_TogglePin()
```

没有经过Semaphore。

### 解决：

修改为：

```c
xSemaphoreGiveFromISR()
```

实现：

```
ISR通知Task
```

---

# 8. 今日成果（Result）

完成：

* [x] FreeRTOS Binary Semaphore实验
* [x] TIM3中断同步Task
* [x] ISR-to-Task事件通知
* [x] FreeRTOS Mutex实验
* [x] UART共享资源保护
* [x] 多任务调试验证

当前FreeRTOS架构：

```
                 FreeRTOS


          +----------------+

          |                |


      SensorTask      UARTTask

          |                |

          |                |

          +------ UART ----+

                 |

              Mutex



          SemaphoreTask

                 |

              Semaphore

                 |

              TIM3 ISR

```

---

# 9. 工程总结（Engineering Summary）

本日学习重点不是API调用，而是理解实时系统设计思想：

## 中断不要处理业务

错误：

```
Interrupt

 |

直接控制外设

```

正确：

```
Interrupt

 |

通知Task

 |

Task处理业务

```

---

## 共享资源必须保护

例如：

* UART
* SPI
* I2C
* Flash

需要：

```
Mutex

```

---

# 10. Git提交

建议提交：

```bash
git add .

git commit -m "feat: implement FreeRTOS semaphore and mutex synchronization"
```

---

# 11. 下一步计划（Next Step）

Day09:

FreeRTOS高级机制：

* Software Timer
* Event Group
* Task状态管理
* 系统周期任务设计

为后续：

```
CAN通信任务
电机控制任务
故障监控任务

```

建立实时调度基础。
