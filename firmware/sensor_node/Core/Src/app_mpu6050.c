/**
  ******************************************************************************
  * @file    app_mpu6050.c
  * @brief   Node1（F103 传感器节点）MPU6050 软件 I2C 驱动（Day 23）
  *          学习点：
  *            1. 软件 I2C 时序：START/STOP/ACK/字节读写
  *            2. F1 硬件 I2C 有兼容性问题，软件 I2C 更稳
  *            3. MPU6050 寄存器：WHO_AM_I/PWR_MGMT_1/量程配置
  *            4. 0x3B 起突发读 14 字节：accel(6)+temp(2)+gyro(6)
  *
  *          接线：PB6=SCL，PB7=SDA，AD0=GND（地址 0x68）
  ******************************************************************************
  */

#include "app_mpu6050.h"

/* 引脚与端口 */
#define MPU_PORT     GPIOB
#define MPU_SCL_PIN  GPIO_PIN_6
#define MPU_SDA_PIN  GPIO_PIN_7

/* I2C 从机地址（AD0=GND → 0x68，左移 1 位为 8 位地址） */
#define MPU_I2C_ADDR  (0x68U << 1)

/* 关键寄存器 */
#define MPU_REG_WHO_AM_I      0x75U
#define MPU_REG_PWR_MGMT_1    0x6BU
#define MPU_REG_SMPLRT_DIV    0x19U
#define MPU_REG_CONFIG        0x1AU
#define MPU_REG_GYRO_CONFIG   0x1BU
#define MPU_REG_ACCEL_CONFIG  0x1CU
#define MPU_REG_ACCEL_XOUT_H  0x3BU

/* SCL/SDA 电平控制（开漏输出：写 0 拉低，写 1 释放由模块上拉） */
#define MPU_SCL_H()  HAL_GPIO_WritePin(MPU_PORT, MPU_SCL_PIN, GPIO_PIN_SET)
#define MPU_SCL_L()  HAL_GPIO_WritePin(MPU_PORT, MPU_SCL_PIN, GPIO_PIN_RESET)
#define MPU_SDA_H()  HAL_GPIO_WritePin(MPU_PORT, MPU_SDA_PIN, GPIO_PIN_SET)
#define MPU_SDA_L()  HAL_GPIO_WritePin(MPU_PORT, MPU_SDA_PIN, GPIO_PIN_RESET)
#define MPU_SDA_READ()  HAL_GPIO_ReadPin(MPU_PORT, MPU_SDA_PIN)

/* 半位周期延时（72MHz 下的粗略 µs 级延时，软件 I2C 容差大） */
static void mpu_delay(void)
{
    volatile uint32_t i;
    for (i = 0U; i < 40U; i++)
    {
        __NOP();
    }
}

/** START：SCL 高电平期间 SDA 拉低 */
static void mpu_i2c_start(void)
{
    MPU_SDA_H(); MPU_SCL_H(); mpu_delay();
    MPU_SDA_L(); mpu_delay();
    MPU_SCL_L(); mpu_delay();
}

/** STOP：SCL 高电平期间 SDA 拉高 */
static void mpu_i2c_stop(void)
{
    MPU_SDA_L(); MPU_SCL_H(); mpu_delay();
    MPU_SDA_H(); mpu_delay();
}

/** 等待从机 ACK：SDA 应为低 */
static uint8_t mpu_i2c_wait_ack(void)
{
    uint8_t ack;
    MPU_SDA_H(); mpu_delay();
    MPU_SCL_H(); mpu_delay();
    ack = (MPU_SDA_READ() == GPIO_PIN_RESET) ? 1U : 0U;
    MPU_SCL_L(); mpu_delay();
    return ack;
}

/** 发送 1 字节（MSB 在前） */
static void mpu_i2c_write_byte(uint8_t data)
{
    int8_t i;
    for (i = 7; i >= 0; i--)
    {
        MPU_SCL_L();
        if (data & (1U << i))
        {
            MPU_SDA_H();
        }
        else
        {
            MPU_SDA_L();
        }
        mpu_delay();
        MPU_SCL_H(); mpu_delay();
    }
    MPU_SCL_L(); mpu_delay();
}

/** 读取 1 字节（ack=1 主发 ACK，ack=0 主发 NACK） */
static uint8_t mpu_i2c_read_byte(uint8_t ack)
{
    uint8_t data = 0U;
    int8_t i;

    MPU_SDA_H();                 /* 释放 SDA，交由从机驱动 */
    for (i = 7; i >= 0; i--)
    {
        MPU_SCL_L(); mpu_delay();
        MPU_SCL_H(); mpu_delay();
        if (MPU_SDA_READ() == GPIO_PIN_SET)
        {
            data |= (uint8_t)(1U << i);
        }
    }
    MPU_SCL_L();

    if (ack)
    {
        MPU_SDA_L(); mpu_delay();   /* ACK：SDA 拉低 */
        MPU_SCL_H(); mpu_delay();
        MPU_SCL_L(); mpu_delay();
        MPU_SDA_H();
    }
    else
    {
        MPU_SDA_H(); mpu_delay();   /* NACK：SDA 保持高 */
        MPU_SCL_H(); mpu_delay();
        MPU_SCL_L(); mpu_delay();
    }
    return data;
}

/** 写寄存器 */
static void mpu_write_reg(uint8_t reg, uint8_t val)
{
    mpu_i2c_start();
    mpu_i2c_write_byte(MPU_I2C_ADDR | 0U);   /* 写方向 */
    mpu_i2c_wait_ack();
    mpu_i2c_write_byte(reg);
    mpu_i2c_wait_ack();
    mpu_i2c_write_byte(val);
    mpu_i2c_wait_ack();
    mpu_i2c_stop();
}

/** 从指定寄存器连续读 len 字节 */
static void mpu_read_bytes(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    mpu_i2c_start();
    mpu_i2c_write_byte(MPU_I2C_ADDR | 0U);   /* 写寄存器地址 */
    mpu_i2c_wait_ack();
    mpu_i2c_write_byte(reg);
    mpu_i2c_wait_ack();
    mpu_i2c_start();                          /* 重复起始，切换读方向 */
    mpu_i2c_write_byte(MPU_I2C_ADDR | 1U);
    mpu_i2c_wait_ack();
    for (i = 0U; i < len; i++)
    {
        buf[i] = mpu_i2c_read_byte(i < (len - 1U));
    }
    mpu_i2c_stop();
}

/**
  * @brief  MPU6050 初始化：GPIO 开漏 + 唤醒 + 采样率/滤波/量程 + WHO_AM_I 校验
  * @retval 1=成功，0=失败
  */
uint8_t MPU_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t id = 0U;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin  = MPU_SCL_PIN | MPU_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;   /* 开漏：配合模块上拉实现线与 */
    gpio.Pull = GPIO_PULLUP;           /* 内部上拉兜底 */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MPU_PORT, &gpio);

    MPU_SDA_H();
    MPU_SCL_H();
    mpu_delay();

    mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x00U);    /* 退出睡眠 */
    mpu_write_reg(MPU_REG_SMPLRT_DIV, 0x07U);    /* 1kHz/(7+1)=125Hz */
    mpu_write_reg(MPU_REG_CONFIG, 0x00U);        /* DLPF 260Hz */
    mpu_write_reg(MPU_REG_GYRO_CONFIG, 0x00U);   /* 陀螺仪 ±250°/s */
    mpu_write_reg(MPU_REG_ACCEL_CONFIG, 0x00U);  /* 加速度计 ±2g */

    mpu_read_bytes(MPU_REG_WHO_AM_I, &id, 1U);
    return (id == 0x68U) ? 1U : 0U;
}

/**
  * @brief  读取全部原始数据
  * @param  p: 结果结构体指针
  * @retval 无
  */
void MPU_ReadAll(MPU6050_t *p)
{
    uint8_t buf[14];

    mpu_read_bytes(MPU_REG_ACCEL_XOUT_H, buf, 14U);
    p->ax   = (int16_t)((buf[0]  << 8) | buf[1]);
    p->ay   = (int16_t)((buf[2]  << 8) | buf[3]);
    p->az   = (int16_t)((buf[4]  << 8) | buf[5]);
    p->temp = (int16_t)((buf[6]  << 8) | buf[7]);
    p->gx   = (int16_t)((buf[8]  << 8) | buf[9]);
    p->gy   = (int16_t)((buf[10] << 8) | buf[11]);
    p->gz   = (int16_t)((buf[12] << 8) | buf[13]);
}
