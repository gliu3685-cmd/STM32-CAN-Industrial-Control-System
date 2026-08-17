/**
  ******************************************************************************
  * @file    app_mpu6050.h
  * @brief   Node1（F103 传感器节点）MPU6050 六轴传感器接口（Day 23）
  *          软件 I2C（避开 F1 硬件 I2C 兼容性问题）
  ******************************************************************************
  */
#ifndef APP_MPU6050_H
#define APP_MPU6050_H

#include "main.h"

/* MPU6050 原始读数结构 */
typedef struct
{
    int16_t ax, ay, az;   /* 加速度计 */
    int16_t temp;         /* 温度 */
    int16_t gx, gy, gz;   /* 陀螺仪 */
} MPU6050_t;

/**
  * @brief  MPU6050 初始化（软件 I2C：PB6=SCL，PB7=SDA）
  * @retval 1=成功（WHO_AM_I=0x68），0=失败
  */
uint8_t MPU_Init(void);

/**
  * @brief  读取全部原始数据（加速度/温度/陀螺仪）
  * @param  p: 结果结构体指针
  * @retval 无
  */
void MPU_ReadAll(MPU6050_t *p);

#endif /* APP_MPU6050_H */
