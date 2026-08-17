/**
  ******************************************************************************
  * @file    app_can.c
  * @brief   Node2（F103 电机节点）CAN 应用层（Day 16）
  *          学习点：
  *            1. 过滤器 ID 掩码模式：精确收 0x201（Mask=0x7FF，ID 左移 5 位）
  *            2. 发送：HAL_CAN_AddTxMessage（邮箱机制）
  *            3. 接收：RX0 中断 + 回调 → FreeRTOS 队列（ISR 安全投递）
  *
  *          协议：0x201 = Master→Node2 命令；0x202 = Node2→Master 数据
  ******************************************************************************
  */

#include "app_can.h"
#include "app_motor.h"
#include "can.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdarg.h>

/* 协议帧 ID */
#define CAN_CMD_ID        (0x201U)   /* Master → Node2 命令 */
#define CAN_DATA_ID       (0x202U)   /* Node2 → Master 电机状态 */

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

/* 当前命令速度（0~1000，对应占空比 0~100%） */
static uint16_t g_speed = 0;

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
    GPIO_InitTypeDef diag = {0};

    /* 临时诊断：读 PA11（CAN1_RX）当前电平，判断 TJA1050 RXD 是否输出隐性高电平 */
    diag.Pin = GPIO_PIN_11;
    diag.Mode = GPIO_MODE_INPUT;
    diag.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &diag);
    NodePrint("[NODE2] PA11 level=%u\r\n",
              (unsigned)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11));

    /* 恢复 CAN1 引脚：PA11=RX（输入上拉），PA12=TX（复用推挽） */
    diag.Mode = GPIO_MODE_INPUT;
    diag.Pull = GPIO_PULLUP;
    diag.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOA, &diag);
    diag.Mode = GPIO_MODE_AF_PP;
    diag.Speed = GPIO_SPEED_FREQ_HIGH;
    diag.Pin = GPIO_PIN_12;
    diag.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &diag);

    /* 清调试冻结位：Debug 模式下 ST-LINK 可能置 DBF=1，
       导致 CAN 内核退不出初始化模式（INAK 恒为 1，Start 超时） */
    CAN1->MCR &= ~CAN_MCR_DBF;

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

    NodePrint("[NODE2] CanStart f=%d s=%d n=%d State=%u err=0x%08lX\r\n",
              (int)r1, (int)r2, (int)r3,
              (unsigned)hcan1.State, (unsigned long)hcan1.ErrorCode);
    NodePrint("[NODE2] MSR=0x%08lX MCR=0x%08lX BTR=0x%08lX\r\n",
              (unsigned long)hcan1.Instance->MSR,
              (unsigned long)hcan1.Instance->MCR,
              (unsigned long)hcan1.Instance->BTR);
}

/**
  * @brief  CAN 错误中断回调
  *         注意：F1 HAL 在错误中断触发时会把 State 置为 HAL_CAN_STATE_ERROR，
  *         且不会自动恢复——必须在回调里恢复 READY，否则发送永远失败。
  */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }

    /* ISR 上下文禁止 printf（会与任务打印交错丢字），
       错误详情由 CanTxTask 每 5 秒读取 ESR 打印 */
    hcan->State = HAL_CAN_STATE_READY;
}

/**
  * @brief  发送电机状态帧（ID=0x202，DLC=2，speed 小端）
  */
void CanSendMotorData(uint16_t speed)
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

    data[0] = (uint8_t)(speed & 0xFF);        /* 速度低字节 */
    data[1] = (uint8_t)((speed >> 8) & 0xFF); /* 速度高字节 */

    if (HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox) != HAL_OK)
    {
        uint32_t esr = hcan1.Instance->ESR;
        NodePrint("[NODE2] add mailbox failed! State=%u LEC=%lu TEC=%lu REC=%lu TSR=0x%08lX\r\n",
                  (unsigned)hcan1.State,
                  (unsigned long)((esr >> 4U) & 0x7U),
                  (unsigned long)((esr >> 24U) & 0xFFU),
                  (unsigned long)((esr >> 16U) & 0xFFU),
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
            NodePrint("[NODE2] RX cmd=0x%03lX dlc=%u data=%02X %02X %02X %02X\r\n",
                      (unsigned long)frame.id, (unsigned)frame.dlc,
                      (unsigned)frame.data[0], (unsigned)frame.data[1],
                      (unsigned)frame.data[2], (unsigned)frame.data[3]);

            /* Day24：收到"设置速度"命令（0x01），真正驱动电机 */
            if ((frame.id == CAN_CMD_ID) && (frame.data[0] == 0x01U))
            {
                g_speed = (uint16_t)(frame.data[1] | ((uint16_t)frame.data[2] << 8));
                MotorSetDuty(g_speed);          /* 0~1000 = 0~100% 占空比 */
                MotorSetDir(1U);                /* 固定正向 */
                NodePrint("[NODE2] CMD set-speed=%u -> duty\r\n", (unsigned)g_speed);
                CanSendMotorData(g_speed);      /* 应答回传设定值 */
            }
        }
    }
}

/**
  * @brief  CAN 发送任务：每 1s 上报真实编码器速度（0x202）
  */
void CanTxTask(void const *pv)
{
    uint32_t cnt = 0;
    int32_t  last_enc = 0;
    int32_t  enc, delta;

    (void)pv;

    CanStart();

    /* Day23：电机驱动初始化；Day24 起默认停转，由 0x201 命令驱动 */
    MotorInit();
    MotorSetDir(1U);
    MotorSetDuty(0U);

    for (;;)
    {
        /* Day24：0x202 上报真实编码器速度（每秒计数值，绝对值） */
        enc   = MotorGetCount();
        delta = enc - last_enc;
        last_enc = enc;
        if (delta < 0)
        {
            delta = -delta;
        }
        if (delta > 0xFFFF)
        {
            delta = 0xFFFF;
        }
        CanSendMotorData((uint16_t)delta);
        NodePrint("[NODE2] TX 0x202 speed=%u cnt=%ld\r\n",
                  (unsigned)delta, (long)enc);

        if ((cnt % 5U) == 0U)
        {
            uint32_t esr = hcan1.Instance->ESR;
            NodePrint("[NODE2] DBG State=%u LEC=%lu TEC=%lu REC=%lu PCLK1=%lu SYSCLK=%lu\r\n",
                      (unsigned)hcan1.State,
                      (unsigned long)((esr >> 4U) & 0x7U),
                      (unsigned long)((esr >> 24U) & 0xFFU),
                      (unsigned long)((esr >> 16U) & 0xFFU),
                      (unsigned long)HAL_RCC_GetPCLK1Freq(),
                      (unsigned long)SystemCoreClock);
        }

        cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
