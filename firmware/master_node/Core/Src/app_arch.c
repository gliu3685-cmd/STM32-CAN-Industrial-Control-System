/**
  ******************************************************************************
  * @file    app_arch.c
  * @brief   FreeRTOS 综合架构设计实验（Day 14）—— Phase 2 收官
  *          功能：搭建三节点 CAN 工业控制系统的最终 RTOS 框架，
  *                形成 Phase 3（CAN 通信）的软件基础。
  *          学习点：
  *            1. 任务划分与单一职责
  *            2. 优先级分配（实时性要求 → RMS 思想）
  *            3. 同步机制选型（Queue 传数据 / EventGroup 告警 /
  *               Timer 心跳 / Mutex 保护共享状态）
  *            4. 故障监控（数据新鲜度 = 节点在线检测）
  *
  * 架构（Day15 起将模拟数据源替换为真实 CAN 外设）：
  *
  *   SimNodeTask(模拟F103节点) --Queue--> CanRxTask --Mutex--> SysState
  *   CanRxTask --Event--> ControlTask --Queue--> MotorTask
  *   HeartbeatTimer --Event--> FaultMonitorTask(超时/越限检测)
  *   CanTxTask(周期发送)        DebugTask(周期打印系统状态)
  ******************************************************************************
  */

#include "app_arch.h"
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"
#include "timers.h"
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/* 系统参数 */
#define ARCH_NODE_NUM       (2U)       /* 两个 F103 从节点 */
#define ARCH_RX_TIMEOUT_MS  (1500U)    /* 超过 1.5s 未收到帧判为离线 */
#define ARCH_TEMP_LIMIT     (80)       /* 节点 0 温度告警阈值（℃） */
#define ARCH_NODE0_DROP_CNT (25U)      /* 演示：25 帧后模拟节点 0 掉线 */

/* 事件位定义 */
#define EVT_HEARTBEAT   (1 << 0)       /* 1s 心跳 */
#define EVT_NEW_FRAME   (1 << 1)       /* 收到新 CAN 帧 */
#define EVT_TEMP_FAULT  (1 << 2)       /* 温度越限 */
#define EVT_OFFLINE     (1 << 3)       /* 节点离线 */

/**
  * @brief  模拟 CAN 帧（Day15 将替换为真实 CAN 接收结构）
  */
typedef struct
{
    uint32_t node_id;      /* 节点 ID：0/1 */
    uint8_t  data[8];      /* 数据负载 */
    uint8_t  dlc;          /* 数据长度 */
    uint32_t tick;         /* 收帧时刻 */
} CanFrame_t;

/**
  * @brief  共享系统状态（由 xSysMutex 保护）
  */
typedef struct
{
    uint32_t uptime_tick;               /* 系统运行节拍（心跳递增） */
    uint8_t  node_online[ARCH_NODE_NUM];
    int32_t  node_temp[ARCH_NODE_NUM];
    uint16_t adc;
    uint16_t speed;
    uint32_t last_rx_tick[ARCH_NODE_NUM];
    uint8_t  fault_temp;                /* 温度越限标志 */
    uint8_t  fault_offline;             /* 节点离线标志 */
} SysState_t;

/* 内核对象句柄 */
static SysState_t         gSys;
static QueueHandle_t      xCanRxQueue;      /* CAN 接收队列 */
static QueueHandle_t      xMotorQueue;      /* 电机指令队列 */
static EventGroupHandle_t xSysEvents;       /* 系统事件组 */
static SemaphoreHandle_t  xSysMutex;        /* 共享状态互斥锁 */
static TimerHandle_t      xHeartbeatTimer;  /* 1s 心跳定时器 */

/**
  * @brief  加锁打印（UART 互斥锁保护，防止多任务输出交错）
  * @param  fmt: 格式串（支持 printf 格式参数）
  * @retval 无
  */
static void ArchPrint(const char *fmt, ...)
{
    va_list args;

    osMutexWait(uartMutexHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  心跳定时器回调（运行于 Timer Service Task）
  *         每 1s 递增运行节拍并置位心跳事件
  * @param  xTimer: 定时器句柄（未使用）
  * @retval 无
  */
static void HeartbeatCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    /* 回调处于任务上下文，可使用普通 API；互斥锁非阻塞取用 */
    if (xSemaphoreTake(xSysMutex, 0) == pdTRUE)
    {
        gSys.uptime_tick++;
        xSemaphoreGive(xSysMutex);
    }
    xEventGroupSetBits(xSysEvents, EVT_HEARTBEAT);
}

/**
  * @brief  模拟 F103 从节点数据源（Day15 由真实 CAN 中断替代）
  *         每 300ms 依次上报节点 0（温度）、节点 1（ADC/速度）；
  *         节点 0 在演示中段"掉线"，用于观察离线故障检测。
  * @param  pv: 未使用
  * @retval 无
  */
static void SimNodeTask(void *pv)
{
    CanFrame_t frame;
    uint32_t   cnt = 0;

    (void)pv;

    for (;;)
    {
        /* 节点 0：温度数据，每帧 +2℃，25 帧后停止发送（模拟掉线） */
        if (cnt <= ARCH_NODE0_DROP_CNT)
        {
            frame.node_id = 0;
            frame.dlc     = 2;
            frame.tick    = xTaskGetTickCount();
            frame.data[0] = (uint8_t)(30 + cnt * 2);
            frame.data[1] = 0;
            xQueueSend(xCanRxQueue, &frame, portMAX_DELAY);
            xEventGroupSetBits(xSysEvents, EVT_NEW_FRAME);
        }

        /* 节点 1：ADC/速度数据 */
        frame.node_id = 1;
        frame.dlc     = 4;
        frame.tick    = xTaskGetTickCount();
        frame.data[0] = (uint8_t)(cnt & 0xFF);
        frame.data[1] = (uint8_t)((cnt >> 8) & 0xFF);
        frame.data[2] = 0xE8;                       /* 1000 低字节 */
        frame.data[3] = 0x03;                       /* 1000 高字节 */
        xQueueSend(xCanRxQueue, &frame, portMAX_DELAY);
        xEventGroupSetBits(xSysEvents, EVT_NEW_FRAME);

        cnt++;
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

/**
  * @brief  CAN 接收任务：从队列取帧并更新共享状态（Day15 接真实 CAN）
  * @param  pv: 未使用
  * @retval 无
  */
static void CanRxTask(void *pv)
{
    CanFrame_t frame;

    (void)pv;

    for (;;)
    {
        if (xQueueReceive(xCanRxQueue, &frame, portMAX_DELAY) == pdTRUE)
        {
            if (frame.dlc >= 4)
            {
                ArchPrint("[CAN_RX] node=%lu dlc=%u data=%02X %02X %02X %02X\r\n",
                          (unsigned long)frame.node_id, (unsigned)frame.dlc,
                          (unsigned)frame.data[0], (unsigned)frame.data[1],
                          (unsigned)frame.data[2], (unsigned)frame.data[3]);
            }
            else
            {
                ArchPrint("[CAN_RX] node=%lu dlc=%u data=%02X %02X\r\n",
                          (unsigned long)frame.node_id, (unsigned)frame.dlc,
                          (unsigned)frame.data[0], (unsigned)frame.data[1]);
            }

            xSemaphoreTake(xSysMutex, portMAX_DELAY);
            if (frame.node_id < ARCH_NODE_NUM)
            {
                gSys.last_rx_tick[frame.node_id] = frame.tick;
                gSys.node_online[frame.node_id]  = 1;
                if (frame.node_id == 0)
                {
                    gSys.node_temp[0] = frame.data[0];
                }
                else
                {
                    gSys.adc   = (uint16_t)(frame.data[0] | (frame.data[1] << 8));
                    gSys.speed = (uint16_t)(frame.data[2] | (frame.data[3] << 8));
                }
            }
            xSemaphoreGive(xSysMutex);
        }
    }
}

/**
  * @brief  控制任务：响应新数据/心跳，检测温度越限，计算并下发电机指令
  * @param  pv: 未使用
  * @retval 无
  */
static void ControlTask(void *pv)
{
    uint16_t target;

    (void)pv;

    for (;;)
    {
        xEventGroupWaitBits(xSysEvents, EVT_NEW_FRAME | EVT_HEARTBEAT,
                            pdTRUE, pdFALSE, portMAX_DELAY);

        xSemaphoreTake(xSysMutex, portMAX_DELAY);
        /* 温度越限 → 置故障标志与事件（工业中此时应降速/停机） */
        if (gSys.node_temp[0] >= ARCH_TEMP_LIMIT)
        {
            gSys.fault_temp = 1;
            xEventGroupSetBits(xSysEvents, EVT_TEMP_FAULT);
            ArchPrint("[CTRL] temp over limit (%ld), reduce speed!\r\n",
                      (long)gSys.node_temp[0]);
        }
        /* 简单控制律演示：目标速度 = 1000 + 温度 × 5 */
        target = (uint16_t)(1000 + gSys.node_temp[0] * 5);
        xSemaphoreGive(xSysMutex);

        if (xQueueSend(xMotorQueue, &target, 0) != pdTRUE)
        {
            ArchPrint("[CTRL] motor queue full!\r\n");
        }
    }
}

/**
  * @brief  电机任务：接收目标速度指令（Day15 起转发给电机节点或本地驱动）
  * @param  pv: 未使用
  * @retval 无
  */
static void MotorTask(void *pv)
{
    uint16_t target = 0;

    (void)pv;

    for (;;)
    {
        if (xQueueReceive(xMotorQueue, &target, portMAX_DELAY) == pdTRUE)
        {
            ArchPrint("[MOTOR] target speed = %u\r\n", (unsigned)target);
        }
    }
}

/**
  * @brief  故障监控任务：每个心跳周期检查节点数据新鲜度，超时判离线
  * @param  pv: 未使用
  * @retval 无
  */
static void FaultMonitorTask(void *pv)
{
    uint32_t now;
    uint8_t  offline;
    uint8_t  i;

    (void)pv;

    for (;;)
    {
        xEventGroupWaitBits(xSysEvents, EVT_HEARTBEAT,
                            pdTRUE, pdFALSE, portMAX_DELAY);

        now     = xTaskGetTickCount();
        offline = 0;

        xSemaphoreTake(xSysMutex, portMAX_DELAY);
        for (i = 0; i < ARCH_NODE_NUM; i++)
        {
            if ((now - gSys.last_rx_tick[i]) > ARCH_RX_TIMEOUT_MS)
            {
                gSys.node_online[i] = 0;
                offline = 1;
            }
        }
        if (offline)
        {
            gSys.fault_offline = 1;
            xEventGroupSetBits(xSysEvents, EVT_OFFLINE);
        }
        xSemaphoreGive(xSysMutex);

        if (offline)
        {
            ArchPrint("[FAULT] node offline detected!\r\n");
        }
    }
}

/**
  * @brief  CAN 发送任务：周期发送心跳帧（Day15 替换为真实 CAN 发送）
  * @param  pv: 未使用
  * @retval 无
  */
static void CanTxTask(void *pv)
{
    uint32_t seq = 0;

    (void)pv;

    for (;;)
    {
        ArchPrint("[CAN_TX] master heartbeat seq=%lu\r\n", (unsigned long)seq++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
  * @brief  调试任务：周期打印系统运行状态与堆水位
  * @param  pv: 未使用
  * @retval 无
  */
static void DebugTask(void *pv)
{
    (void)pv;

    for (;;)
    {
        xSemaphoreTake(xSysMutex, portMAX_DELAY);
        ArchPrint("[DEBUG] up=%lu node0=%s(%ldC) node1=%s adc=%u speed=%u "
                  "fault=%s%s heap=%lu\r\n",
                  (unsigned long)gSys.uptime_tick,
                  gSys.node_online[0] ? "ON" : "OFF", (long)gSys.node_temp[0],
                  gSys.node_online[1] ? "ON" : "OFF",
                  (unsigned)gSys.adc, (unsigned)gSys.speed,
                  gSys.fault_temp    ? "TEMP " : "",
                  gSys.fault_offline ? "OFFLINE" : "",
                  (unsigned long)xPortGetFreeHeapSize());
        xSemaphoreGive(xSysMutex);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
  * @brief  综合架构演示任务（Day 14 实验主体）
  *         创建全部内核对象与系统任务后自我删除
  * @param  argument: 未使用
  * @retval 无
  */
void ArchDemoTask(void const * argument)
{
    (void)argument;

    /* 创建同步对象：队列 ×2、事件组、互斥锁、软件定时器 */
    xCanRxQueue     = xQueueCreate(8, sizeof(CanFrame_t));
    xMotorQueue     = xQueueCreate(4, sizeof(uint16_t));
    xSysEvents      = xEventGroupCreate();
    xSysMutex       = xSemaphoreCreateMutex();
    xHeartbeatTimer = xTimerCreate("hbeat", pdMS_TO_TICKS(1000),
                                   pdTRUE, NULL, HeartbeatCallback);

    if (xCanRxQueue == NULL || xMotorQueue == NULL || xSysEvents == NULL ||
        xSysMutex == NULL || xHeartbeatTimer == NULL)
    {
        ArchPrint("[ARCH] object create failed!\r\n");
        vTaskDelete(NULL);
        return;
    }
    xTimerStart(xHeartbeatTimer, 0);

    ArchPrint("== Day14 System Architecture started ==\r\n");

    /* 创建系统任务：优先级体现实时性（控制/接收/故障 > 电机 > 发送/模拟 > 调试） */
    xTaskCreate(SimNodeTask,      "sim",   256, NULL, 2, NULL);
    xTaskCreate(CanRxTask,        "canrx", 256, NULL, 4, NULL);
    xTaskCreate(ControlTask,      "ctrl",  256, NULL, 5, NULL);
    xTaskCreate(MotorTask,        "motor", 256, NULL, 3, NULL);
    xTaskCreate(FaultMonitorTask, "fault", 256, NULL, 4, NULL);
    xTaskCreate(CanTxTask,        "cantx", 256, NULL, 2, NULL);
    xTaskCreate(DebugTask,        "debug", 256, NULL, 1, NULL);

    /* 架构任务使命完成，删除自身 */
    vTaskDelete(NULL);
}
