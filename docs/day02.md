# Day2 STM32工程结构学习


## 今日目标

- 理解STM32启动流程
- 理解CubeMX工程结构
- 理解HAL库架构
- 完成GPIO LED控制实验
- 优化工程文件结构


## 学习内容


### 1. STM32启动流程

STM32启动流程：

Reset

↓

startup.s

↓

SystemInit()

↓

HAL_Init()

↓

main()


### 2. CubeMX工程结构

工程主要组成：

Core

Drivers

.ioc配置文件


### 3. HAL库

应用代码通过HAL接口访问硬件：

Application

↓

HAL

↓

Register

↓

Hardware


## 实践内容

完成：

STM32F407 GPIO LED闪烁实验


GPIO:

PF9


代码：

HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);

HAL_Delay(500);


## 工程优化

完成CubeMX模块化生成：

main.c

gpio.c

gpio.h

usart.c

usart.h


## 实验结果

LED闪烁成功。


## 总结

完成STM32基础工程环境搭建，
为后续FreeRTOS、CAN通信开发建立基础。
