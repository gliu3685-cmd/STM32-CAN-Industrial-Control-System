/**
  ******************************************************************************
  * @file    can.c
  * @brief   CAN1 初始化（F103 Node1，Day 16）
  *          波特率：500 kbit/s（APB1=36MHz，Prescaler=4，BS1=15，BS2=2，SJW=1）
  *          采样点 = (1+15)/18 ≈ 88.9%
  *          引脚：PA11=CAN1_RX，PA12=CAN1_TX
  ******************************************************************************
  */

#include "can.h"

CAN_HandleTypeDef hcan1;

/**
  * @brief CAN1 初始化：500kbps 正常模式
  */
void MX_CAN1_Init(void)
{
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 4;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN MSP：时钟、GPIO（PA11/PA12）、NVIC（RX0 优先级 5）
  */
void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (canHandle->Instance == CAN1)
  {
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    /* F103 CAN1 默认映射就是 PA11/PA12，无需重映射
       （切勿调用 __HAL_AFIO_REMAP_CAN1_1()，那会把 CAN 挪到 PB8/PB9） */

    /* PA11=CAN1_RX：F103 的 CAN_RX 必须配成输入模式（CubeMX 标准写法），
       配成 AF_PP（复用推挽输出）会导致 CAN 内核收不到 RXD 电平、退不出初始化 */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA12=CAN1_TX：推挽输出，无需上下拉 */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init（F1 的 RX0 中断与 USB 低优先级共享向量） */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_TX_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
  }
}

/**
  * @brief CAN MSP DeInit
  */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{
  if (canHandle->Instance == CAN1)
  {
    __HAL_RCC_CAN1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
  }
}
