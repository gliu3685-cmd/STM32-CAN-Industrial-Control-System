docs/Day2_GPIO_UART.md
# Day2 - GPIO Control and UART Communication


## Objective


学习STM32 HAL库基础外设开发。


完成：

- GPIO控制
- UART通信
- printf重定向


---

# GPIO Development


使用HAL库控制LED：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);

实现：

LED周期闪烁。

UART Configuration

USART1:

Baud Rate: 115200

Data Bits: 8

Parity: None

Stop Bits: 1

printf Redirect

实现：

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


实现：

printf("STM32 Start\r\n");

HAL Library Knowledge

HAL提供：

GPIO API
UART API
Timer API

例如：

HAL_UART_Transmit()

HAL_GPIO_WritePin()
