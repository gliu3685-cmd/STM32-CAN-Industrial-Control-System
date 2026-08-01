# Day02 - GPIO 控制与 UART 通信

## 1. 今日目标（Objectives）

学习 STM32 HAL 库基础外设开发。

今日完成：

* GPIO 控制
* UART 通信
* printf 重定向

---

## 2. GPIO 开发

使用 HAL 库控制 LED 闪烁：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
```

实现：LED 周期闪烁。

---

## 3. UART 配置

USART1 参数：

| 参数 | 值 |
|---|---|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |

---

## 4. printf 重定向

实现串口打印：

```c
int __io_putchar(int ch)
{
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)&ch,
        1,
        HAL_MAX_DELAY
    );
    return ch;
}
```

测试输出：

```c
printf("STM32 Start\r\n");
```

---

## 5. HAL 库知识

HAL 提供：

* GPIO API：`HAL_GPIO_WritePin()`、`HAL_GPIO_TogglePin()`
* UART API：`HAL_UART_Transmit()`
* Timer API（后续使用）

---

## 6. 今日成果（Result）

* [x] GPIO 控制 LED 闪烁
* [x] USART1 串口通信（115200, 8N1）
* [x] printf 重定向到串口

---

## 7. 工程总结（Engineering Summary）

掌握 HAL 库 GPIO 与 UART 基础调用方式，建立串口调试手段，为后续调试通信和 FreeRTOS 任务输出打下基础。

---

## 8. 下一步计划（Next Step）

Day03：UART 调试系统（见 [day03.md](day03.md)）。
