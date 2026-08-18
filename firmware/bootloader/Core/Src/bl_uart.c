/**
  ******************************************************************************
  * @file    bl_uart.c
  * @brief   Bootloader UART 基础接口（USART1 115200）
  ******************************************************************************
  */

#include "bl_uart.h"
#include "usart.h"

/**
  * @brief  发送一帧数据（阻塞，超时 100ms）
  */
void UART_Send(const uint8_t *buf, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
}

/**
  * @brief  非阻塞读取一个字节（RXNE 置位才调用 HAL 读取）
  */
int UART_GetByte(uint8_t *b)
{
    return (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) ?
           (HAL_UART_Receive(&huart1, b, 1, 0) == HAL_OK) : 0;
}
