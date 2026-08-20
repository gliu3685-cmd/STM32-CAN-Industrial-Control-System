/**
  ******************************************************************************
  * @file    app_motor.h
  * @brief   Node2（F103 电机节点）电机驱动接口（Day 23）
  ******************************************************************************
  */
#ifndef APP_MOTOR_H
#define APP_MOTOR_H

#include "main.h"

/* 占空比范围：0~1000（0.1% 分辨率，ARR=999） */
#define MOTOR_DUTY_MAX   (1000U)

/* Day25 实测：输出轴每转编码器计数（数圈法 10 圈 = 10788 计数） */
#define ENC_COUNTS_PER_REV  (1079U)

/**
  * @brief  电机初始化：TIM2 PWM(PA0) + TIM3 编码器(PA6/PA7) + 方向脚(PA1)
  * @retval 无
  */
void MotorInit(void);

/**
  * @brief  设置 PWM 占空比（0~1000）
  * @param  duty: 占空比
  * @retval 无
  */
void MotorSetDuty(uint16_t duty);

/**
  * @brief  设置电机方向（0=反向，1=正向）
  * @param  dir: 方向
  * @retval 无
  */
void MotorSetDir(uint8_t dir);

/**
  * @brief  读取编码器计数（TIM3 CNT，带符号）
  * @retval 计数值
  */
int32_t MotorGetCount(void);

#endif /* APP_MOTOR_H */
