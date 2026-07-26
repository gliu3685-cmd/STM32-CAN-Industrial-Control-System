docs/Day1_STM32_Setup.md
# Day1 - STM32 Development Environment Setup


## Objective

建立STM32开发环境，完成第一次程序下载运行。

目标：

- 配置STM32CubeMX
- 配置STM32CubeIDE
- 完成ST-Link下载调试
- 创建第一个STM32工程


---

# Hardware

MCU:

- STM32F407VET6


Development Tools:

- STM32CubeMX
- STM32CubeIDE
- ST-Link V2


---

# Development Process


## 1. STM32CubeMX Configuration


完成：

- MCU选择 STM32F407VET6
- Clock Configuration
- GPIO配置
- 工程生成


System Clock:


168 MHz



---

## 2. STM32CubeIDE


完成：

- 导入CubeMX工程
- 编译工程
- 下载程序


---

# First Test


实现：

LED Blink


测试结果：

- STM32成功运行
- ST-Link下载正常


---

# Knowledge Learned


## STM32开发流程



CubeMX

↓

Generate Code

↓

CubeIDE Compile

↓

ST-Link Download

↓

Hardware Test



---

# Problems


## ST-Link无法识别


Solution:

- 检查USB驱动
- 更新ST-Link Firmware
- 重新连接调试器


---

# Summary


完成STM32基础开发环境搭建。

建立后续HAL库和RTOS开发基础。
