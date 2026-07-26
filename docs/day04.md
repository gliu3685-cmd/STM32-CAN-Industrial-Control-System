# Day4 Timer and Interrupt


## 完成功能

- TIM3基础定时器配置
- 定时器中断
- HAL_TIM_PeriodElapsedCallback


## 实验现象

TIM3中断周期触发LED翻转。


## 学习总结

理解：

Polling:

while循环主动查询


Interrupt:

硬件事件触发CPU响应


## 工程意义

Timer用于：

- PID周期控制
- CAN超时检测
- 周期任务调度
