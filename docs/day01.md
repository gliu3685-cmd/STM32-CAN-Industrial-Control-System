# Day1 STM32 Development Environment Setup


## 今日目标

完成 STM32F407 开发环境搭建，
建立后续嵌入式项目开发基础。


## 硬件环境

主控：

- STM32F407VET6 最小系统板

调试下载：

- ST-Link V2


## 软件环境

开发工具：

- STM32CubeMX
- STM32CubeIDE

代码管理：

- Git
- GitHub


## 完成内容


### 1. STM32CubeMX配置

完成：

- STM32F407VET6芯片选择
- 时钟配置
- GPIO基础配置
- 工程代码生成


### 2. STM32CubeIDE工程导入

完成：

- CubeMX生成工程
- CubeIDE编译
- 工程运行


### 3. ST-Link下载测试

完成：

- ST-Link连接
- 程序下载
- Debug运行


### 4. 第一个程序运行

实现：

- GPIO控制LED
- STM32程序成功运行


## 遇到的问题


### ST-Link识别问题

现象：

下载器连接后设备识别异常。


解决：

检查：

- ST-Link驱动
- USB连接
- 下载接口


## 项目意义

Day1建立STM32基础开发环境。

后续将在此基础上实现：

- FreeRTOS实时系统
- CAN通信
- 电机闭环控制
- IAP Bootloader
