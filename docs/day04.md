# Day04 - Timer 定时器与中断

## 1. 今日目标（Objectives）

学习 STM32 定时器与中断机制。

今日完成：

* TIM3 基础定时器配置
* 定时器中断
* `HAL_TIM_PeriodElapsedCallback` 回调

---

## 2. 实验现象（Experiment）

TIM3 中断周期触发 LED 翻转：

```
TIM3 定时中断
    ↓
HAL_TIM_PeriodElapsedCallback
    ↓
LED 翻转
```

---

## 3. 学习总结（Theory）

### Polling（轮询）

`while` 循环主动查询事件：

```c
while (1)
{
    if (事件发生) { 处理; }
}
```

特点：CPU 持续占用，实时性差。

### Interrupt（中断）

硬件事件触发 CPU 响应：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* 中断内处理 */
}
```

特点：事件驱动，CPU 只在需要时响应。

---

## 4. 工程意义

Timer 用于：

* PID 周期控制
* CAN 超时检测
* 周期任务调度

---

## 5. 今日成果（Result）

* [x] TIM3 基础定时器配置
* [x] 定时器中断触发
* [x] 中断回调处理 LED
* [x] 理解 Polling 与 Interrupt 的区别

---

## 6. 工程总结（Engineering Summary）

掌握定时器中断机制，理解"事件驱动"与"轮询"的本质区别。定时器是后续周期控制（PID）、通信超时检测和任务调度的基础。

---

## 7. 下一步计划（Next Step）

Day05：FreeRTOS 基础移植（见 [day05.md](day05.md)）。
