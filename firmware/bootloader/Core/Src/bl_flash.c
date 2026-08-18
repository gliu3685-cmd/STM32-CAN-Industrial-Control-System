/**
  ******************************************************************************
  * @file    bl_flash.c
  * @brief   Flash 擦写、有效标志与 APP 有效性判断（F407ZGTx）
  *          扇区布局：Sector 0-3=16KB，Sector 4=64KB，Sector 5-11=128KB
  ******************************************************************************
  */

#include "bl_flash.h"

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
    uint32_t start_sec = AddrToSector(addr), end_sec = AddrToSector(addr + len - 1);

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
  * @brief  64 位双字编程写入 Flash（尾块补 0xFF）
  */
int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint64_t word = 0xFFFFFFFFFFFFFFFFULL;

    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i++)
    {
        word &= ~(0xFFULL << ((i % 8) * 8));
        word |= (uint64_t)data[i] << ((i % 8) * 8);
        if ((i % 8) == 7 || i == len - 1)
        {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                  addr + i - (i % 8), word) != HAL_OK)
            {
                HAL_FLASH_Lock();
                return -1;
            }
            word = 0xFFFFFFFFFFFFFFFFULL;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/**
  * @brief  在 APP_START+0x100 写入有效标志（向量表保留区，不影响运行）
  */
void BlFlashWriteMagic(void)
{
    HAL_FLASH_Unlock();
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START + 0x100U,
                      (uint32_t)APP_MAGIC);
    HAL_FLASH_Lock();
}

/**
  * @brief  APP 有效性：栈指针在 128KB 主 SRAM 范围且 magic 正确
  */
int BlAppValid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_START;
    uint32_t magic = *(volatile uint32_t *)(APP_START + 0x100U);

    return (sp >= 0x20000000U && sp < 0x20020000U) && (magic == APP_MAGIC);
}
