/**
  ******************************************************************************
  * @file    bl_crc.h
  * @brief   Bootloader CRC32 接口（IEEE 802.3，与 Python zlib.crc32 一致）
  ******************************************************************************
  */
#ifndef BL_CRC_H
#define BL_CRC_H

#include "main.h"

/**
  * @brief  计算 CRC32（IEEE 802.3，多项式 0xEDB88320，初值 0xFFFFFFFF，结果异或 0xFFFFFFFF）
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval CRC32 值
  */
uint32_t BlCrc32(const uint8_t *data, uint32_t len);

#endif /* BL_CRC_H */
