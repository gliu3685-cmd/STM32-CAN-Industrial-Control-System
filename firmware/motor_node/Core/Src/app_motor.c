/**
  ******************************************************************************
  * @file    app_motor.c
  * @brief   Node2（F103 电机节点）电机驱动（Day 23）
  *          学习点：
  *            1. TIM2_CH1(PA0) PWM 输出 → L298N IN1，控制速度
  *            2. PA1 GPIO → L298N IN2，控制方向
  *            3. TIM3 编码器模式（PA6/PA7）计数，A/B 正交 4 倍频
  *
  *          PWM：72MHz / (7+1) / (999+1) = 9kHz，占空比 0~1000
  *          编码器：TIM3 CNT 16 位，正转递增、反转递减
  ******************************************************************************
  */

#include "app_motor.h"
#include "stm32f1xx_hal_tim.h"
#include <stdio.h>

/* TIM 句柄（本地静态，不依赖 CubeMX 生成） */
static TIM_HandleTypeDef htim2;
static TIM_HandleTypeDef htim3;

/**
  * @brief  电机初始化：时钟、GPIO、TIM2 PWM、TIM3 编码器
  * @param  无
  * @retval 无
  */
void MotorInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA0 = TIM2_CH1：PWM 输出（F1 复用推挽） */
    gpio.Pin  = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA1 = L298N IN2：方向控制 */
    gpio.Pin  = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &gpio);
    /* 默认低电平：IN1/IN2 双低 = 停转（安全态），避免上电即转 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

    /* PA6/PA7 = TIM3_CH1/CH2：编码器 A/B 输入 */
    gpio.Pin  = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_INPUT;
    /* 霍尔编码器多为开集输出，必须上拉；
       NOPULL 时引脚悬空，电机运行时 PWM 噪声易被误计为脉冲 */
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* TIM2：PWM，2kHz（PSC=35 → 2MHz，ARR=999 → 2kHz）。
       由 9kHz 降频，减少 PWM 开关沿耦合进编码器 A/B 线的噪声 */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 35U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim2);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

    /* TIM3：编码器模式（TI12 = 双通道 4 倍频） */
    TIM_Encoder_InitTypeDef enc = {0};
    enc.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler = TIM_ICPSC_DIV1;
    enc.IC1Filter    = 0U;
    enc.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC2Prescaler = TIM_ICPSC_DIV1;
    enc.IC2Filter    = 0U;
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFFU;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Encoder_Init(&htim3, &enc);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

}

/**
  * @brief  设置 PWM 占空比（0~1000）
  * @param  duty: 占空比
  * @retval 无
  */
void MotorSetDuty(uint16_t duty)
{
    /* 钳到 MOTOR_DUTY_LIMIT：既避免 CCR>ARR 恒高（L298N 双高刹车），
       也限制最高转速（Day25 起默认 80% 上限） */
    if (duty > MOTOR_DUTY_LIMIT)
    {
        duty = MOTOR_DUTY_LIMIT;
    }
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty);
}

/**
  * @brief  设置电机方向（0=反向，1=正向）
  * @param  dir: 方向
  * @retval 无
  */
void MotorSetDir(uint8_t dir)
{
    /* L298N：正向 = IN2 拉低，IN1 接 PWM，电机速度与 duty 成正比；
       若 IN2 恒高，duty 越大 IN1/IN2 同为高的时间越多 = 越接近刹车（速度映射反了）。
       dir=1 正向 → PA1 低；dir=0 反向 → PA1 高（协议当前未用反向） */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, dir ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
  * @brief  读取编码器计数
  * @retval TIM3 CNT（正转递增、反转递减）
  */
int32_t MotorGetCount(void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
}

/* PID 状态（增量式，20ms 周期） */
static int32_t pid_err1 = 0;
static int32_t pid_err2 = 0;
static int32_t pid_out  = 0;

/**
  * @brief  增量式 PID：Δout = Kp(e-e1) + Ki*e + Kd(e-2e1+e2)
  *         积分分离：|e| 大于阈值时 Ki 项置 0，防止启动/大阶跃积分饱和；
  *         输出限幅到 [0, MOTOR_DUTY_LIMIT]（无反向制动）
  */
uint16_t MotorPidCompute(uint16_t target_rpm, uint16_t rpm_fb)
{
    int32_t e  = (int32_t)target_rpm - (int32_t)rpm_fb;
    int32_t ea = (e < 0) ? -e : e;
    int32_t dout = PID_KP * (e - pid_err1)
                 + ((ea > PID_INT_SEP_ERR) ? 0 : PID_KI) * e
                 + PID_KD * (e - 2 * pid_err1 + pid_err2);

    pid_out += dout;
    if (pid_out > (int32_t)MOTOR_DUTY_LIMIT) pid_out = MOTOR_DUTY_LIMIT;
    if (pid_out < 0) pid_out = 0;

    pid_err2 = pid_err1;
    pid_err1 = e;
    return (uint16_t)pid_out;
}
