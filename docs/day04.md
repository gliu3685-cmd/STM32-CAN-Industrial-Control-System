# Day04 - Timer 定时器与中断

## 1. 今日目标（Objectives）

学习 STM32 定时器与中断机制，理解"事件驱动"与"轮询"的本质区别。

今日完成：

* TIM3 基础定时器配置
* 定时器中断
* `HAL_TIM_PeriodElapsedCallback` 回调

---

## 2. 理论学习（Theory）

### 2.1 Polling（轮询）

`while` 循环主动查询事件：

```c
while (1)
{
    if (事件发生) { 处理; }
}
```

特点：CPU 持续占用，一直在"问"，实时性差、浪费功耗。

### 2.2 Interrupt（中断）

硬件事件触发 CPU 响应：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* 中断内处理 */
}
```

特点：事件驱动，CPU 只在需要时响应，实时性好。

### 2.3 定时器计数原理

STM32 定时器本质是一个**计数器**：时钟源每来一个脉冲计数值 +1，
计数到设定值（自动重载值 ARR）溢出，触发更新中断。溢出周期由
**预分频器（PSC）** 和 **自动重载值（ARR）** 共同决定：

```
定时时间 = (PSC + 1) × (ARR + 1) / 定时器时钟频率
```

例：定时器时钟 84MHz，PSC=8399、ARR=4999 时：

```
(8399+1) × (4999+1) / 84MHz = 8400 × 5000 / 84e6 = 0.5s
```

（具体参数以工程 CubeMX 配置为准，此处演示计算方法。）

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2
* USB-TTL（CH340G）

软件：

* STM32CubeIDE
* HAL Library
* PC 串口助手（115200, 8N1）

工程：`f407_blink`（主工程，与仓库 master_node 同步）

当前阶段：Phase 1 STM32 基础

---

## 4. 实现（Implementation）

TIM3 定时中断周期触发 LED 翻转：

```
TIM3 定时中断
    ↓
HAL_TIM_PeriodElapsedCallback
    ↓
LED 翻转
```

CubeMX 配置 TIM3（预分频/自动重载按目标周期设置）并在 main 中启动中断：

```c
HAL_TIM_Base_Start_IT(&htim3);
```

回调实现：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
    }
}
```

---

## 5. 实验过程（Experiment）

现象：

* LED 按定时器周期翻转（由硬件计时触发，不占主循环）；
* 主循环同时可以干别的事，两者互不阻塞。

验证：

* 修改 PSC/ARR 后翻转周期随之改变，证明定时器计数链路生效；
* 中断回调内不做耗时操作，保持响应及时。

---

## 6. 遇到的问题与解决（Problems & Solutions）

本日未遇到阻塞性问题。

注意点：中断回调（ISR）里不要放耗时操作（如长延时、复杂计算、
大段打印），中断应"快进快出"；需要长时间处理的逻辑应只做标志/通知，
放到主循环或任务中执行（Day08 的信号量正是为此设计）。

---

## 7. 今日成果（Result）

完成：

* [x] TIM3 基础定时器配置
* [x] 定时器中断触发
* [x] 中断回调处理 LED
* [x] 理解 Polling 与 Interrupt 的区别

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 事件驱动优于轮询

轮询是 CPU 主动"问"，中断是硬件主动"叫"。实时系统里，周期任务、
外部事件都用中断/定时器驱动，把 CPU 从空等中解放出来。

## 定时器是实时系统的"节拍器"

Timer 用于：PID 周期控制、CAN 超时检测、周期任务调度——后面每一个
实验都会用到。

---

## 9. 面试问答（Interview Prep）

### Q1：轮询和中断有什么区别？什么时候用哪个？

答题要点：

* 轮询：CPU 持续查询，简单但浪费 CPU、实时性差；
* 中断：硬件通知 CPU，响应及时、省 CPU；
* 低频/不紧急事件可轮询，高频/紧急事件必须中断。

### Q2：定时器的 PSC 和 ARR 怎么计算？

答题要点：

* 定时时间 = (PSC+1) × (ARR+1) / 定时器时钟；
* 先定 PSC（分频到合适计数频率），再定 ARR（计多少个脉冲溢出）。

### Q3：中断回调里能不能做耗时操作？

答题要点：

* 不能，中断应快进快出；
* 只做置标志/通知（如信号量 GiveFromISR），业务移到任务/主循环。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day04 TIM3 timer interrupt and callback"
```

---

## 11. 下一步计划（Next Step）

Day05：FreeRTOS 基础移植（见 [day05.md](day05.md)）。
