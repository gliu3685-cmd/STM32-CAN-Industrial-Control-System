/**
  ******************************************************************************
  * @file    bl_flash.c
  * @brief   Flash 擦写、有效标志与 APP 有效性判断（F407ZGTx）
  *          扇区布局：Sector 0-3=16KB，Sector 4=64KB，Sector 5-11=128KB
  *          写入宽度：VDD=2.7~3.6V 下只能用 32 位字编程（RANGE_3）；
  *          64 位双字编程需 VPP 外加 8~9V，本板不具备
  ******************************************************************************
  */

#include "bl_flash.h"
#include "usart.h"

/**
  * @brief  地址 → 扇区号（1024KB 器件）
  */
static uint32_t AddrToSector(uint32_t addr)
{
    if (addr < 0x08010000U) return (addr - 0x08000000U) / 0x4000U; /* sector 0-3，16KB */
    if (addr < 0x08020000U) return 4U;                             /* sector 4，64KB */
    return 5U + (addr - 0x08020000U) / 0x20000U;                   /* sector 5-11，128KB */
}

/**
  * @brief  按扇区擦除 [addr, addr+len) 覆盖的 Flash 区间
  */
int BlFlashEraseRange(uint32_t addr, uint32_t len)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t fail = 0;
    uint32_t start_sec, end_sec;

    if (len == 0U) return -1;
    start_sec = AddrToSector(addr);
    end_sec   = AddrToSector(addr + len - 1);

    HAL_FLASH_Unlock();
    for (uint32_t sec = start_sec; sec <= end_sec; sec++)
    {
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Sector = sec;
        erase.NbSectors = 1;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&erase, &fail) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/**
  * @brief  32 位字编程写入 Flash（尾块补 0xFF）
  */
int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t word = 0xFFFFFFFFU;

    if ((addr & 0x3U) != 0U) return -1;   /* 字编程要求 4 字节对齐 */

    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i++)
    {
        word &= ~(0xFFU << ((i % 4) * 8));
        word |= (uint32_t)data[i] << ((i % 4) * 8);
        if ((i % 4) == 3 || i == len - 1)
        {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                  addr + i - (i % 4), word) != HAL_OK)
            {
                HAL_FLASH_Lock();
                return -1;
            }
            word = 0xFFFFFFFFU;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/**
  * @brief  擦除元数据扇区（Sector 3）
  */
int BlFlashClearMeta(void)
{
    return BlFlashEraseRange(META_ADDR, 4U);
}

/**
  * @brief  写入元数据：magic + 镜像长度 + CRC32（调用前需已擦除）
  *         magic 放在最后写：写到一半掉电时 magic 仍为 0xFF，APP 判为无效
  */
int BlFlashWriteMeta(uint32_t len, uint32_t crc)
{
    int ret = 0;

    HAL_FLASH_Unlock();
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, META_ADDR + 4U, len) != HAL_OK ||
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, META_ADDR + 8U, crc) != HAL_OK ||
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, META_ADDR, APP_MAGIC) != HAL_OK)
    {
        ret = -1;
    }
    HAL_FLASH_Lock();
    return ret;
}

/**
  * @brief  APP 有效性：magic 正确 + 栈指针在 128KB 主 SRAM + 复位向量落在 APP 区
  */
int BlAppValid(void)
{
    uint32_t magic = *(volatile uint32_t *)META_ADDR;
    uint32_t sp    = *(volatile uint32_t *)APP_START;
    uint32_t pc    = *(volatile uint32_t *)(APP_START + 4U);

    return (magic == APP_MAGIC) &&
           (sp >= 0x20000000U && sp <= 0x20020000U) &&
           (pc >= APP_START && pc < APP_START + APP_LEN_MAX);
}

/**
  * @brief  跳转到 APP：清理 Bootloader 现场后设置 MSP 并跳转 Reset_Handler，永不返回
  *         APP 侧需设置 SCB->VTOR 并在初始化后重新开中断（__enable_irq）
  */
void BlJumpToApp(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_START;
    void (*reset)(void) = (void (*)(void))(*(volatile uint32_t *)(APP_START + 4U));

    /* 外设与时钟恢复复位态，APP 按自己的配置重新初始化
       （放在关中断前：HAL_RCC_DeInit 内部超时依赖 SysTick 计数） */
    HAL_UART_DeInit(&huart1);
    HAL_RCC_DeInit();
    HAL_DeInit();

    __disable_irq();

    /* 停 SysTick，避免 APP 未接管前进入 Bootloader 的 SysTick_Handler */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    /* 关闭并清除所有外设中断的使能与挂起位 */
    for (uint32_t i = 0; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = APP_START;
    __set_MSP(sp);
    __DSB();
    __ISB();
    reset();
}
