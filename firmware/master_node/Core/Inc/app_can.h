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
  * @brief  发送 0x101：请求 Node1 上报温度
  * @retval 无
  */
void CanSendReportReq(void);

/**
  * @brief  发送 0x201：设置 Node2 目标速度
  * @param  speed: 目标速度（0~1000）
  * @retval 无
  */
void CanSendSpeed(uint16_t speed);

#endif /* APP_CAN_H */
