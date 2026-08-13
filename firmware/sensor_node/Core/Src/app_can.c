/**
  ******************************************************************************
  * @file    app_can.c
  * @brief   Node1（F103 传感器节点）CAN 应用层（Day 16）
  *          学习点：
  *            1. 过滤器 ID 掩码模式：精确收 0x101（Mask=0x7FF，ID 左移 5 位）
  *            2. 发送：HAL_CAN_AddTxMessage（邮箱机制）
  *            3. 接收：RX0 中断 + 回调 → FreeRTOS 队列（ISR 安全投递）
  *
  *          协议：0x101 = Master→Node1 命令；0x102 = Node1→Master 数据
  ******************************************************************************
  */

#include "app_can.h"
#include "can.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdarg.h>

/* 协议帧 ID */
#define CAN_CMD_ID        (0x101U)   /* Master → Node1 命令 */
#define CAN_DATA_ID       (0x102U)   /* Node1 → Master 传感器数据 */

/**
  * @brief  CAN 帧简化结构（节点内部使用）
  */
typedef struct
{
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} CanFrame_t;

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

static QueueHandle_t xCanRxQueue;

/**
  * @brief  加锁打印（UART 互斥锁保护，防止多任务输出交错）
  */
static void NodePrint(const char *fmt, ...)
{
    va_list args;

    osMutexWait(uartMutexHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  创建 CAN 接收队列（调度器启动前调用）
  */
void NodeCanInit(void)
{
    xCanRxQueue = xQueueCreate(8, sizeof(CanFrame_t));
}

/**
  * @brief  启动 CAN1：过滤器（只收 0x101）+ 启动外设 + RX0 中断通知
  */
void CanStart(void)
{
    CAN_FilterTypeDef filter = {0};

    /* 掩码模式：Mask=0x7FF（11 位全比较），Filter=0x101 → 只收命令帧 */
    filter.FilterIdHigh         = (uint16_t)(CAN_CMD_ID << 5);
    filter.FilterIdLow          = 0x0000U;
    filter.FilterMaskIdHigh     = (uint16_t)(0x7FFU << 5);
    filter.FilterMaskIdLow      = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterBank           = 0U;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation     = CAN_FILTER_ENABLE;

    HAL_StatusTypeDef r1 = HAL_CAN_ConfigFilter(&hcan1, &filter);
    HAL_StatusTypeDef r2 = HAL_CAN_Start(&hcan1);
    HAL_StatusTypeDef r3 = HAL_CAN_ActivateNotification(&hcan1,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_ERROR |
                                     CAN_IT_BUSOFF |
                                     CAN_IT_LAST_ERROR_CODE);

    NodePrint("[NODE1] CanStart f=%d s=%d n=%d State=%u err=0x%08lX\r\n",
              (int)r1, (int)r2, (int)r3,
              (unsigned)hcan1.State, (unsigned long)hcan1.ErrorCode);
    NodePrint("[NODE1] MSR=0x%08lX MCR=0x%08lX BTR=0x%08lX\r\n",
              (unsigned long)hcan1.Instance->MSR,
              (unsigned long)hcan1.Instance->MCR,
              (unsigned long)hcan1.Instance->BTR);
}

/**
  * @brief  CAN 错误中断回调
  *         注意：F1 HAL 在错误中断触发时会把 State 置为 HAL_CAN_STATE_ERROR，
  *         且不会自动恢复——必须在回调里恢复 READY，否则发送永远失败。
  *         ISR 上下文不能用互斥锁，此处直接 printf（临时诊断）。
  */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }

    printf("[NODE1] CAN_ERR code=0x%08lX\r\n", (unsigned long)hcan->ErrorCode);

    /* 恢复状态机：错误中断处理完后让外设回到可发送状态 */
    hcan->State = HAL_CAN_STATE_READY;
}

/**
  * @brief  发送传感器数据帧（ID=0x102，DLC=2，temp 低字节在前）
  */
void CanSendSensorData(uint8_t temp)
{
    CAN_TxHeaderTypeDef tx = {0};
    uint8_t  data[8] = {0};
    uint32_t mailbox = 0;

    tx.StdId = CAN_DATA_ID;
    tx.ExtId = 0U;
    tx.IDE   = CAN_ID_STD;
    tx.RTR   = CAN_RTR_DATA;
    tx.DLC   = 2U;
    tx.TransmitGlobalTime = DISABLE;

    data[0] = temp;   /* 温度低字节 */
    data[1] = 0x00;   /* 高字节（温度 < 256，恒 0） */

    HAL_StatusTypeDef st = HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox);

    if (st != HAL_OK)
    {
        uint32_t esr = hcan1.Instance->ESR;
        NodePrint("[NODE1] TX FAIL st=%d LEC=%lu TEC=%lu REC=%lu State=%u TSR=0x%08lX\r\n",
                  (int)st,
                  (unsigned long)((esr >> 4U) & 0x7U),   /* Last Error Code */
                  (unsigned long)((esr >> 24U) & 0xFFU), /* TX Error Counter */
                  (unsigned long)((esr >> 16U) & 0xFFU), /* RX Error Counter */
                  (unsigned)hcan1.State,
                  (unsigned long)hcan1.Instance->TSR);
    }
}

/**
  * @brief  CAN RX0 中断回调：读帧 → 投递到队列（ISR 安全）
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx = {0};
    CanFrame_t          frame;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hcan->Instance != CAN1)
    {
        return;
    }

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, frame.data) != HAL_OK)
    {
        return;
    }

    frame.id  = rx.StdId;
    frame.dlc = rx.DLC;

    xQueueSendFromISR(xCanRxQueue, &frame, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
  * @brief  CAN 接收任务：从队列取命令帧并打印
  */
void CanRxTask(void const *pv)
{
    CanFrame_t frame;

    (void)pv;

    for (;;)
    {
        if (xQueueReceive(xCanRxQueue, &frame, portMAX_DELAY) == pdTRUE)
        {
            NodePrint("[NODE1] RX cmd=0x%03lX dlc=%u data=%02X %02X %02X %02X\r\n",
                      (unsigned long)frame.id, (unsigned)frame.dlc,
                      (unsigned)frame.data[0], (unsigned)frame.data[1],
                      (unsigned)frame.data[2], (unsigned)frame.data[3]);
        }
    }
}

/**
  * @brief  CAN 发送任务：启动 CAN 后每 1s 发送一次模拟温度（30℃ 起递增）
  */
void CanTxTask(void const *pv)
{
    uint8_t temp = 30;
    uint32_t cnt = 0;

    (void)pv;

    CanStart();

    for (;;)
    {
        /* 每 5 秒打印一次 CAN 状态快照（便于串口抓取诊断信息） */
        if ((cnt % 5U) == 0U)
        {
            NodePrint("[NODE1] DBG State=%u MSR=0x%08lX MCR=0x%08lX BTR=0x%08lX\r\n",
                      (unsigned)hcan1.State,
                      (unsigned long)hcan1.Instance->MSR,
                      (unsigned long)hcan1.Instance->MCR,
                      (unsigned long)hcan1.Instance->BTR);
        }

        CanSendSensorData(temp);
        NodePrint("[NODE1] TX 0x102 temp=%u\r\n", (unsigned)temp);

        temp++;
        if (temp > 60)
        {
            temp = 30;
        }

        cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
