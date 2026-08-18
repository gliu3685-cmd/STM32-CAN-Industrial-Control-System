/**
  ******************************************************************************
  * @file    bl_protocol.h
  * @brief   Bootloader 升级协议（帧解析与命令分发）
  ******************************************************************************
  */
#ifndef BL_PROTOCOL_H
#define BL_PROTOCOL_H

#include "main.h"

/* 升级命令 */
#define CMD_ERASE   0x01U   /* DATA = 镜像总长度(4B LE) */
#define CMD_WRITE   0x02U   /* ADDR = 目标地址，DATA = 分块(<=256B) */
#define CMD_FINISH  0x03U   /* DATA = 镜像总长度(4B LE) + CRC32(4B LE) */
#define CMD_REBOOT  0x04U

/* 应答 */
#define ACK         0x06U
#define NACK        0x15U

/**
  * @brief  协议主循环：收帧 → CRC 校验 → 分发（阻塞，不返回）
  */
void BlProtocolRun(void);

/**
  * @brief  命令分发（hdr[0]=CMD, hdr[1..4]=ADDR, hdr[5..6]=LEN, hdr[7..]=DATA）
  */
void BlDispatchFrame(const uint8_t *hdr, uint16_t plen);

#endif /* BL_PROTOCOL_H */
