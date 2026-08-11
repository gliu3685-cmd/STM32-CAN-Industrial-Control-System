/**
  ******************************************************************************
  * @file    app_can.c
  * @brief   CAN 通信实验（Day 15）—— 应用层 CAN 驱动
  *          学习点：
  *            1. CAN 过滤器（ID 掩码模式，本实验不过滤全接收）
  *            2. 发送：HAL_CAN_AddTxMessage（邮箱机制）
  *            3. 接收：RX0 中断 + 回调 → FreeRTOS 队列（ISR 安全投递）
  *
  * 波特率：500 kbit/s（APB1=42MHz，Prescaler=6，BS1=11，BS2=2，SJW=1）
  *         采样点 = (1+11)/14 ≈ 85.7%
  ******************************************************************************
  */

#include "app_can.h"
#include "app_arch.h"
#include "can.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* 主控心跳帧 ID（标准帧 11 位） */
#define CAN_HEARTBEAT_ID   (0x100U)

/**
  * @brief  启动 CAN1：过滤器 + 启动外设 + 开启 RX0 中断通知
  * @param  无
  * @retval 无
  *
  * 过滤器说明：ID 掩码模式，掩码全 0 表示"任何 ID 都匹配"，
  *           即接收总线上所有标准帧（本实验验证阶段先全收）。
  */
void CanStart(void)
{
    CAN_FilterTypeDef filter = {0};

    /* 过滤器 Bank0：掩码模式，全接收 */
    filter.FilterIdHigh         = 0x0000U;
    filter.FilterIdLow          = 0x0000U;
    filter.FilterMaskIdHigh     = 0x0000U;
    filter.FilterMaskIdLow      = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterBank           = 0U;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation     = CAN_FILTER_ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    {
        ArchPrint("[CAN] filter config failed!\r\n");
    }
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        ArchPrint("[CAN] start failed!\r\n");
    }
    if (HAL_CAN_ActivateNotification(&hcan1,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_ERROR |
                                     CAN_IT_BUSOFF |
                                     CAN_IT_LAST_ERROR_CODE) != HAL_OK)
    {
        ArchPrint("[CAN] RX notification failed!\r\n");
    }

    ArchPrint("[CAN] started, 500kbps, filter=all, waiting frames...\r\n");
}

/**
  * @brief  CAN 错误中断回调（HAL 在错误中断中调用）
  *         打印错误码，便于定位 ACK/总线关闭等故障
  * @param  hcan: CAN 句柄
  * @retval 无
  */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }

    /* 注意：ISR 上下文不能用互斥锁，此处直接用 printf（临时诊断） */
    printf("[CAN_ERR] code=0x%08lX", (unsigned long)hcan->ErrorCode);
    if (hcan->ErrorCode & HAL_CAN_ERROR_ACK)
    {
        printf(" ACK(no node responding!)");
    }
    if (hcan->ErrorCode & HAL_CAN_ERROR_BOF)
    {
        printf(" BUS-OFF");
    }
    if (hcan->ErrorCode & HAL_CAN_ERROR_EWG)
    {
        printf(" WARNING");
    }
    if (hcan->ErrorCode & HAL_CAN_ERROR_EPV)
    {
        printf(" PASSIVE");
    }
    printf("\r\n");
}

/**
  * @brief  发送主控心跳帧
  * @param  seq: 发送序号（0 递增）
  * @retval 无
  */
void CanSendHeartbeat(uint32_t seq)
{
    CAN_TxHeaderTypeDef tx = {0};
    uint8_t  data[8] = {0};
    uint32_t mailbox = 0;

    tx.StdId             = CAN_HEARTBEAT_ID;
    tx.ExtId             = 0U;
    tx.IDE               = CAN_ID_STD;
    tx.RTR               = CAN_RTR_DATA;
    tx.DLC               = 8U;
    tx.TransmitGlobalTime = DISABLE;

    /* 负载定义：seq 低/高字节 + 主控状态字（0x55AA） + 预留 */
    data[0] = (uint8_t)(seq & 0xFFU);
    data[1] = (uint8_t)((seq >> 8) & 0xFFU);
    data[2] = 0x55U;
    data[3] = 0xAAU;

    if (HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox) == HAL_OK)
    {
        ArchPrint("[CAN_TX] id=0x100 seq=%lu\r\n", (unsigned long)seq);
    }
    else
    {
        ArchPrint("[CAN_TX] add mailbox failed!\r\n");
    }

    /* 诊断：直接读错误状态寄存器（不依赖错误中断/NVIC 配置） */
    {
        uint32_t esr = hcan1.Instance->ESR;
        uint32_t lec = (esr >> 4U) & 0x7U;   /* Last Error Code */
        uint32_t rec = (esr >> 16U) & 0xFFU; /* RX 错误计数 */
        uint32_t tec = (esr >> 24U) & 0xFFU; /* TX 错误计数 */

        if (lec != 0U)
        {
            ArchPrint("[CAN_ERR] LEC=%lu TEC=%lu REC=%lu\r\n",
                      (unsigned long)lec,
                      (unsigned long)tec,
                      (unsigned long)rec);
        }
    }
}

/**
  * @brief  CAN RX0 中断回调（HAL 在中断中调用）
  *         读取报文后投递到系统接收队列，由 CanRxTask 打印与处理
  * @param  hcan: CAN 句柄
  * @retval 无
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx = {0};
    CanFrame_t          frame;

    if (hcan->Instance != CAN1)
    {
        return;
    }

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, frame.data) != HAL_OK)
    {
        return;
    }

    frame.node_id = rx.StdId;
    frame.dlc     = rx.DLC;
    frame.tick    = (uint32_t)xTaskGetTickCountFromISR();

    ArchPostCanFrame(&frame);
}
