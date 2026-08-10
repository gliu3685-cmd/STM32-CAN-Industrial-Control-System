/**
  ******************************************************************************
  * @file    app_fault.c
  * @brief   FreeRTOS 异常检测与调试实验（Day 13）
  *          功能：栈溢出检测、HardFault 定位、Assert 机制、栈余量监控
  *          学习点：
  *            1. configCHECK_FOR_STACK_OVERFLOW（方法 2）+ 溢出钩子
  *            2. HardFault 压栈帧解析（PC/LR/R0-R3/xPSR + CFSR/BFAR）
  *            3. configASSERT 断言机制（打印文件行号后复位）
  *            4. uxTaskGetStackHighWaterMark 栈余量监控
  *
  * 演示流程（利用 BKP 后备寄存器跨复位记住进度）：
  *   上电/复位 → Demo1 栈溢出 → NVIC_SystemReset
  *            → Demo2 HardFault → NVIC_SystemReset
  *            → Demo3 configASSERT → NVIC_SystemReset
  *            → 正常模式：每 5s 打印栈余量
  *   如需重看完整演示：重新烧录（清备份域）后再次上电。
  ******************************************************************************
  */

#include "app_fault.h"
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* UART 共享资源互斥锁（定义于 freertos.c） */
extern osMutexId uartMutexHandle;

/* STM32F407 BKP 后备数据寄存器 DR1（备份域，系统复位不清除） */
#define BKP_DR1_ADDR        (0x40006C04UL)
/* 演示进度：0=栈溢出，1=HardFault，2=Assert，3=正常模式 */
#define FAULT_STAGE_MAX     (3U)

/**
  * @brief  加锁打印（防止多任务输出交错）
  * @param  fmt: 格式串（支持 printf 格式参数）
  * @retval 无
  */
static void FaultPrint(const char *fmt, ...)
{
    va_list args;

    osMutexWait(uartMutexHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osMutexRelease(uartMutexHandle);
}

/**
  * @brief  读取故障演示进度（BKP_DR1）
  * @retval 当前阶段 0~3
  */
static uint32_t FaultStageRead(void)
{
    return (*(volatile uint32_t *)BKP_DR1_ADDR) & 0xFFU;
}

/**
  * @brief  写入故障演示进度（BKP_DR1）
  * @param  stage: 阶段编号
  * @retval 无
  */
static void FaultStageWrite(uint32_t stage)
{
    *(volatile uint32_t *)BKP_DR1_ADDR = stage & 0xFFU;
}

/**
  * @brief  使能备份域访问（PWR 时钟 + DBP 写保护解除）
  * @retval 无
  */
static void EnableBackupDomain(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

/**
  * @brief  递归压栈：每层 128 字节局部数组，快速撑爆 128 字（512B）任务栈
  * @param  depth: 剩余递归层数
  * @retval 无
  */
static void OverflowRecurse(uint32_t depth)
{
    volatile uint8_t buf[128];

    buf[0] = (uint8_t)depth;          /* 写入栈上数组（触发栈使用） */
    vTaskDelay(pdMS_TO_TICKS(1));     /* 主动让出 → 调度器切换时检测栈溢出 */
    if (depth > 0)
    {
        OverflowRecurse(depth - 1);   /* 递归 10 层 × ~144B >> 512B，必爆 */
    }
    buf[0] = 0;                       /* 防止编译器优化 */
    (void)buf[0];                     /* 读取一次，消除 unused-but-set 警告 */
}

/**
  * @brief  栈溢出演示任务（Demo 1）
  *         任务栈仅 128 字，递归调用撑爆后触发 vApplicationStackOverflowHook
  * @param  pv: 未使用
  * @retval 无
  */
static void StackOverflowTask(void *pv)
{
    (void)pv;

    FaultPrint("[FAULT] StackOverflow task started (stack=128 words)\r\n");
    OverflowRecurse(10);              /* 递归 10 层，栈必然溢出 */
    vTaskDelete(NULL);
}

/**
  * @brief  HardFault 演示任务（Demo 2）
  *         故意写非法地址 0xDEADBEEF → BusFault → 升级 HardFault
  * @param  pv: 未使用
  * @retval 无
  */
static void HardFaultTask(void *pv)
{
    volatile uint32_t *pBad;

    (void)pv;

    FaultPrint("[FAULT] HardFault task: writing 0x55 to 0xDEADBEEF...\r\n");
    pBad = (volatile uint32_t *)0xDEADBEEFUL;
    *pBad = 0x55U;                    /* 写非法地址 → 精确总线错误 → HardFault */
    vTaskDelete(NULL);
}

/**
  * @brief  Assert 演示任务（Demo 3）
  *         故意调用 configASSERT(0) → 打印失败位置并复位
  * @param  pv: 未使用
  * @retval 无
  */
static void AssertTask(void *pv)
{
    (void)pv;

    FaultPrint("[FAULT] Assert task: calling configASSERT(0)...\r\n");
    configASSERT(0);                  /* 故意断言失败 */
    vTaskDelete(NULL);
}

/**
  * @brief  栈余量监控任务（正常模式）
  *         每 5s 打印本任务栈水位（uxTaskGetStackHighWaterMark）
  * @param  pv: 未使用
  * @retval 无
  */
static void StackMonitorTask(void *pv)
{
    (void)pv;

    for (;;)
    {
        FaultPrint("[FAULT] stack monitor: self high-water=%u words, BKP stage=%lu\r\n",
                   (unsigned)uxTaskGetStackHighWaterMark(NULL),
                   (unsigned long)FaultStageRead());
        osDelay(5000);
    }
}

/**
  * @brief  异常检测与调试演示任务（Day 13 实验主体）
  *         按 BKP 记录分阶段演示，每次故障触发复位后进入下一阶段
  * @param  argument: 未使用
  * @retval 无
  */
void FaultDemoTask(void const * argument)
{
    uint32_t stage;

    (void)argument;

    EnableBackupDomain();
    stage = FaultStageRead();

    if (stage == 0)
    {
        /* 阶段 1：栈溢出检测 */
        FaultStageWrite(1);
        FaultPrint("== Day13 Demo 1/3: Stack Overflow Detection ==\r\n");
        xTaskCreate(StackOverflowTask, "ovf", 128, NULL, 1, NULL);
        vTaskDelete(NULL);            /* 等待栈溢出钩子触发复位 */
    }
    else if (stage == 1)
    {
        /* 阶段 2：HardFault 定位 */
        FaultStageWrite(2);
        FaultPrint("== Day13 Demo 2/3: HardFault Location ==\r\n");
        xTaskCreate(HardFaultTask, "hdf", 128, NULL, 1, NULL);
        vTaskDelete(NULL);
    }
    else if (stage == 2)
    {
        /* 阶段 3：Assert 机制 */
        FaultStageWrite(3);
        FaultPrint("== Day13 Demo 3/3: configASSERT Mechanism ==\r\n");
        xTaskCreate(AssertTask, "ast", 128, NULL, 1, NULL);
        vTaskDelete(NULL);
    }
    else
    {
        /* 正常模式：栈余量周期监控 */
        FaultPrint("== Day13 all demos done, enter normal stack monitor mode ==\r\n");
        xTaskCreate(StackMonitorTask, "stkmon", 128, NULL, 1, NULL);
        vTaskDelete(NULL);
    }
}
