/**
  ******************************************************************************
  * @file    bl_flash.h
  * @brief   Bootloader Flash 擦写与 APP 有效性接口
  ******************************************************************************
  */
#ifndef BL_FLASH_H
#define BL_FLASH_H

#include "main.h"

/* APP 区起始地址（Bootloader 区 = 前 64KB，Sector 0-3） */
#define APP_START   0x08010000U
/* APP 区最大长度（ZGTx 1024KB - 64KB = 960KB） */
#define APP_LEN_MAX 0xF0000U
/* 升级成功后写入 APP_START+0x100 的有效标志 */
#define APP_MAGIC   0xA5A5A5A5U

/* 4 字节小端读取 */
#define LE32(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | \
                 ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

/**
  * @brief  按扇区擦除 [addr, addr+len) 覆盖的 Flash 区间
  * @retval 0=成功，-1=失败
  */
int BlFlashEraseRange(uint32_t addr, uint32_t len);

/**
  * @brief  64 位双字编程写入 Flash（尾块自动补 0xFF）
  * @retval 0=成功，-1=失败
  */
int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len);

/**
  * @brief  在 APP_START+0x100 写入有效标志 0xA5A5A5A5
  */
void BlFlashWriteMagic(void);

/**
  * @brief  判断 APP 是否有效：栈指针在 RAM 范围且 magic 正确
  * @retval 1=有效，0=无效
  */
int BlAppValid(void);

#endif /* BL_FLASH_H */
