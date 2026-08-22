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
/* 速度上限（协议仍收 0~1000，但实际驱动钳到 80%，防止电机过快） */
#define MOTOR_DUTY_LIMIT (800U)

/* Day25 实测：输出轴每转编码器计数（数圈法 10 圈 = 10788 计数） */
#define ENC_COUNTS_PER_REV  (1079U)

/* Day27 PID 常量（增量式，20ms 控制周期；初值需在硬件上微调） */
#define PID_KP             (5)
#define PID_KI             (1)
#define PID_KD             (0)
#define PID_INT_SEP_ERR    (30)   /* |误差| 超过该值进入积分分离，防饱和 */

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

/**
  * @brief  增量式 PID 速度控制（积分分离 + 输出限幅）
  * @param  target_rpm: 目标转速（RPM）
  * @param  rpm_fb: 实测转速（RPM，建议先平滑）
  * @retval 占空比输出（0 ~ MOTOR_DUTY_LIMIT）
  */
uint16_t MotorPidCompute(uint16_t target_rpm, uint16_t rpm_fb);

#endif /* APP_MOTOR_H */
