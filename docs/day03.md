# Day03 - UART 调试系统

## 1. 今日目标（Objectives）

建立 STM32F407 主控节点的 UART 调试系统，让串口成为后续所有实验的
"观察窗口"。

今日完成：

* USART1 配置（PA9 TX / PA10 RX）
* printf 重定向
* USB-TTL 串口通信
* PC 端日志输出

---

## 2. 理论学习（Theory）

### 2.1 调试系统分层

```
printf()
    ↓
__io_putchar()
    ↓
HAL_UART_Transmit()
    ↓
USART1
    ↓
PA9 TX
    ↓
CH340（USB-TTL 转换）
    ↓
PC 串口助手
```

理解这条链路，排查"串口没输出"时就能逐层定位：程序有没有跑？
printf 有没有进？引脚配置对不对？线接没接对？串口助手参数对不对？

### 2.2 GPIO 复用功能

USART1 的 TX/RX 引脚必须配置为**复用功能**（Alternate Function），
并选择正确的复用编号：

```c
GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;        /* 复用推挽 */
GPIO_InitStruct.Alternate = GPIO_AF7_USART1;        /* USART1 的复用编号 */
```

复用编号写错是串口无输出的头号原因。

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

### 4.1 USART1 配置

CubeMX 中启用 USART1（异步模式），参数 115200-8N1，引脚自动分配到
PA9（TX）/ PA10（RX）。

### 4.2 printf 重定向

```c
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

---

## 5. 实验过程（Experiment）

现象：PC 串口助手实时收到主控输出：

```
F407 Master Start!
LED Toggle
LED Toggle
```

验证：

* `F407 Master Start!` 在 main 函数开机处输出；
* `LED Toggle` 周期性输出，与 LED 闪烁同步；
* 日志实时、无乱码，调试链路完整。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：串口无输出

问题：

* LED 正常闪烁（程序在跑）
* COM4 可以打开（USB-TTL 正常）
* 串口没有数据

原因：USART GPIO 复用配置错误——引脚没有配置为 USART 的复用功能，
数据根本没从 PA9 送出去。

解决：确认引脚复用配置：

```c
GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
```

重新生成代码后恢复通信。

---

## 7. 今日成果（Result）

完成：

* [x] USART1 调试串口配置完成
* [x] printf 重定向可用
* [x] PC 端日志实时输出

---

## 8. 工程总结（Engineering Summary）

本日学习重点：

## 调试系统是项目的"眼睛"

UART 调试系统为后续 CAN 通信、FreeRTOS 任务调试提供统一的日志输出接口，
是嵌入式开发的基础调试手段。之后所有任务打印都复用这条链路。

## 排查问题要按链路分层

串口无输出时，按 "程序 → printf → 引脚复用 → 接线 → 串口参数" 逐层
排查，比瞎试快得多。

---

## 9. 面试问答（Interview Prep）

### Q1：串口没输出，你会怎么排查？

答题要点：

* 先确认程序在跑（LED/断点）；
* 再确认 printf 重定向是否生效；
* 检查 GPIO 复用配置（AF7_USART1）和引脚；
* 检查 TX/RX 是否接反、USB-TTL 共地；
* 最后核对串口助手参数（波特率 115200-8N1）。

### Q2：波特率算错了会怎样？

答题要点：

* 收发双方采样时序不一致，出现乱码或完全无数据；
* 波特率由时钟源与分频寄存器决定（CubeMX 自动计算）。

### Q3：为什么嵌入式调试首选串口而不是其他接口？

答题要点：

* 简单（2 根线）、通用（USB-TTL 便宜）、printf 直接复用；
* 缺点：速度慢、无流控，不适合大数据传输。

---

## 10. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day03 UART debug system with printf redirect"
```

---

## 11. 下一步计划（Next Step）

Day04：Timer 定时器与中断（见 [day04.md](day04.md)）。
