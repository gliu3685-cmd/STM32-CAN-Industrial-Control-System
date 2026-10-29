/**
  ******************************************************************************
  * @file    bl_flash.h
  * @brief   Bootloader Flash 擦写与 APP 有效性接口
  ******************************************************************************
  */
#ifndef BL_FLASH_H
#define BL_FLASH_H

#include "main.h"

/* APP 区起始地址（Bootloader 代码 = Sector 0-2 共 48KB，Sector 3 = 元数据） */
#define APP_START   0x08010000U
/* APP 区最大长度（ZGTx 1024KB - 64KB = 960KB） */
#define APP_LEN_MAX 0xF0000U
/* 升级成功后写入元数据区的有效标志 */
#define APP_MAGIC   0xA5A5A5A5U

/* 元数据区：Sector 3（0x0800C000，16KB）独立擦写，不与 APP 镜像重叠。
   布局：+0 magic，+4 镜像长度，+8 镜像 CRC32 */
#define META_ADDR   0x0800C000U
#define META_SECTOR 3U

/* 4 字节小端读取 */
#define LE32(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | \
                 ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

/**
  * @brief  按扇区擦除 [addr, addr+len) 覆盖的 Flash 区间
  * @retval 0=成功，-1=失败
  */
int BlFlashEraseRange(uint32_t addr, uint32_t len);

/**
  * @brief  32 位字编程写入 Flash（尾块自动补 0xFF，addr 需 4 字节对齐）
  * @retval 0=成功，-1=失败
  */
int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len);

/**
  * @brief  擦除元数据扇区（升级开始时调用，使旧标志失效）
  * @retval 0=成功，-1=失败
  */
int BlFlashClearMeta(void);

/**
  * @brief  写入元数据：magic + 镜像长度 + CRC32
  * @retval 0=成功，-1=失败
  */
int BlFlashWriteMeta(uint32_t len, uint32_t crc);

/**
  * @brief  判断 APP 是否有效：magic 正确 + 栈指针在 RAM 范围 + 复位向量在 APP 区
  * @retval 1=有效，0=无效
  */
int BlAppValid(void);

/**
  * @brief  跳转到 APP（关中断、停 SysTick、清 NVIC、复位时钟后跳转）
  */
void BlJumpToApp(void);

#endif /* BL_FLASH_H */
