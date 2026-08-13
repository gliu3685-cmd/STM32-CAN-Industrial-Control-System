/**
  ******************************************************************************
  * @file    app_can.h
  * @brief   Node2（F103 电机节点）CAN 应用层接口（Day 16）
  *          功能：精确过滤收 Master 命令（0x201），周期发送电机状态（0x202）
  ******************************************************************************
  */

#ifndef APP_CAN_H
#define APP_CAN_H

#include "main.h"

/**
  * @brief  创建 CAN 接收队列（调度器启动前调用，无打印）
  */
void NodeCanInit(void);

/**
  * @brief  启动 CAN1：配置过滤器（只收 0x101）并开启接收中断
  */
void CanStart(void);

/**
 * @brief  发送电机状态帧（ID=0x202，DLC=2，speed 小端）
  */
void CanSendMotorData(uint16_t speed);

/**
  * @brief  CAN 接收任务：从队列取命令帧并打印
  */
void CanRxTask(void const *pv);

/**
 * @brief  CAN 发送任务：每 1s 发送一次模拟速度
  */
void CanTxTask(void const *pv);

#endif /* APP_CAN_H */
