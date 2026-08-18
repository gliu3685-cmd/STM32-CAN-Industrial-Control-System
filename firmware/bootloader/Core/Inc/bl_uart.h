/**
  ******************************************************************************
  * @file    bl_uart.h
  * @brief   Bootloader UART 基础接口（USART1 115200）
  ******************************************************************************
  */
#ifndef BL_UART_H
#define BL_UART_H

#include "main.h"

/**
  * @brief  发送一帧数据（阻塞，超时 100ms）
  * @param  buf: 数据指针
  * @param  len: 长度
  * @retval 无
  */
void UART_Send(const uint8_t *buf, uint16_t len);

/**
  * @brief  非阻塞读取一个字节（有数据立即返回）
  * @param  b: 输出字节
  * @retval 1=读到数据，0=无数据
  */
int UART_GetByte(uint8_t *b);

#endif /* BL_UART_H */
