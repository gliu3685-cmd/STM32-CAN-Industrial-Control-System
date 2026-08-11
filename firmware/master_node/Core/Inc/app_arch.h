/**
  ******************************************************************************
  * @file    app_arch.h
  * @brief   FreeRTOS 综合架构设计实验（Day 14）
  *          头文件：系统架构任务接口声明
  ******************************************************************************
  */
#ifndef APP_ARCH_H
#define APP_ARCH_H

#include "cmsis_os.h"

/**
  * @brief  CAN 帧结构（Day15 起由真实 CAN 外设填充）
  *         node_id 为标准帧 ID，data/dlc 为负载与长度
  */
typedef struct
{
    uint32_t node_id;      /* CAN 标准帧 ID（11 位） */
    uint8_t  data[8];      /* 数据负载 */
    uint8_t  dlc;          /* 数据长度 */
    uint32_t tick;         /* 收帧时刻 */
} CanFrame_t;

/**
  * @brief  综合架构演示任务（Phase 2 收官）
  *         创建同步对象（Queue/EventGroup/Mutex/Timer）与全部系统任务：
  *         CAN Rx / CAN Tx / Control / Motor / Fault Monitor / Debug
  * @param  argument: 未使用
  * @retval 无
  */
void ArchDemoTask(void const * argument);

/**
  * @brief  加锁打印（UART 互斥锁保护，防止多任务输出交错）
  * @param  fmt: 格式串（支持 printf 格式参数）
  * @retval 无
  */
void ArchPrint(const char *fmt, ...);

/**
  * @brief  将收到的 CAN 帧投递到系统接收队列（可从 ISR 调用）
  * @param  pFrame: 指向待投递帧
  * @retval 无
  */
void ArchPostCanFrame(CanFrame_t *pFrame);

#endif /* APP_ARCH_H */
