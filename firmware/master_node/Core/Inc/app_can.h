/**
  ******************************************************************************
  * @file    app_can.h
  * @brief   CAN 通信实验（Day 15）—— 应用层 CAN 驱动接口
  *          功能：启动 CAN1、配置过滤器、发送心跳帧、中断接收回调
  ******************************************************************************
  */
#ifndef APP_CAN_H
#define APP_CAN_H

#include "main.h"

/**
  * @brief  启动 CAN1：配置过滤器（接收全部标准帧）并开启接收中断
  * @param  无
  * @retval 无
  */
void CanStart(void);

/**
  * @brief  发送主控心跳帧（标准帧 ID=0x100，8 字节）
  * @param  seq: 发送序号
  * @retval 无
  */
void CanSendHeartbeat(uint32_t seq);

/**
  * @brief  发送应用层命令帧（0=Node1 请求上报，1=Node2 设置速度）
  * @param  node: 目标节点号（0/1）
  * @retval 无
  */
void CanSendCommand(uint8_t node);

#endif /* APP_CAN_H */
