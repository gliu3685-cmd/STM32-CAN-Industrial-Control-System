# Day03 - UART 调试系统

## 1. 今日目标（Objectives）

建立 STM32F407 主控节点的 UART 调试系统。

今日完成：

* USART1 配置（PA9 TX / PA10 RX）
* printf 重定向
* USB-TTL 串口通信
* PC 端日志输出

---

## 2. UART 通信链路

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
CH340（USB-TTL）
    ↓
PC 串口助手
```

---

## 3. 遇到的问题与解决（Problems & Solutions）

### Problem 1：串口无输出

现象：

* LED 正常闪烁
* COM4 可以打开
* 串口没有数据

原因：USART GPIO 复用配置错误。

解决：确认引脚复用配置：

```c
GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
```

重新生成代码后恢复通信。

---

## 4. 测试结果（Result）

串口输出：

```
F407 Master Start!
LED Toggle
LED Toggle
```

---

## 5. 今日成果（Result）

* [x] USART1 调试串口配置完成
* [x] printf 重定向可用
* [x] PC 端日志实时输出

---

## 6. 工程总结（Engineering Summary）

UART 调试系统为后续 CAN 通信、FreeRTOS 任务调试提供统一的日志输出接口，是嵌入式开发的基础调试手段。

---

## 7. 下一步计划（Next Step）

Day04：Timer 定时器与中断（见 [day04.md](day04.md)）。
