/**
  ******************************************************************************
  * @file    bl_crc.c
  * @brief   CRC32（IEEE 802.3，多项式 0xEDB88320）
  *          实现为逐位反射算法，与 Python zlib.crc32 结果一致；
  *          标准校验向量：BlCrc32("123456789", 9) == 0xCBF43926
  ******************************************************************************
  */

#include "bl_crc.h"

/**
  * @brief  计算 CRC32
  */
uint32_t BlCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t b = 0; b < 8; b++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }

    return crc ^ 0xFFFFFFFFU;
}
