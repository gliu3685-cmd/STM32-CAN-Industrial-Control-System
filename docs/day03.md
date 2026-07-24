# Day3 UART Debug System


## 今日目标

建立 STM32F407 主控节点 UART 调试系统。


## 完成功能

- USART1 配置
- PA9 TX / PA10 RX
- printf 重定向
- USB-TTL串口通信
- PC端日志输出


## UART通信链路


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

CH340

↓

PC串口助手



## 调试问题记录


### 问题1：串口无输出

现象：

- LED正常闪烁
- COM4可以打开
- 串口没有数据


原因：

USART GPIO复用配置检查。


解决：

确认：

GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;

GPIO_InitStruct.Alternate = GPIO_AF7_USART1;


重新生成代码后恢复通信。


## 测试结果


串口输出：

F407 Master Start!

LED Toggle

LED Toggle


## 总结

UART Debug系统为后续CAN通信、
FreeRTOS任务调试提供日志接口。
