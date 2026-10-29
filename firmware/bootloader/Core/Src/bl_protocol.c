/**
  ******************************************************************************
  * @file    bl_protocol.c
  * @brief   升级协议帧状态机
  *          帧格式：A5 5A | CMD(1B) | ADDR(4B LE) | LEN(2B LE) | DATA(<=256B)
  *                  | CRC32(4B LE，仅 DATA)
  ******************************************************************************
  */

#include "bl_protocol.h"
#include "bl_uart.h"
#include "bl_crc.h"
#include "bl_flash.h"

/**
  * @brief  协议主循环（阻塞）
  *         CRC 失败回 NACK；CRC 通过交给 BlDispatchFrame 统一回执
  */
void BlProtocolRun(void)
{
    uint8_t rx = 0, buf[7 + 256 + 4];   /* CMD+ADDR4+LEN2 + DATA(<=256) + CRC4 */
    uint16_t plen = 0;
    uint32_t idx = 0;
    int stage = 0;

    for (;;)
    {
        if (!UART_GetByte(&rx)) { HAL_Delay(1); continue; }

        if (stage == 0) { if (rx == 0xA5) stage = 1; continue; }
        if (stage == 1) { stage = (rx == 0x5A) ? 2 : 0; continue; }

        if (stage == 2)                    /* 帧头：CMD+ADDR4+LEN2 */
        {
            buf[idx++] = rx;
            if (idx == 7)
            {
                plen = (uint16_t)(buf[5] | (buf[6] << 8));
                idx = 0;                   /* stage 3 从 DATA 第 0 字节重新计数 */
                if (plen > 256) { stage = 0; continue; }
                stage = 3;
            }
            continue;
        }

        if (stage == 3)                    /* DATA + CRC32(4B，仅 DATA) */
        {
            buf[7 + idx] = rx;
            idx++;
            if (idx == (uint32_t)plen + 4U)
            {
                uint32_t crc_rx = (uint32_t)buf[7 + plen] |
                    ((uint32_t)buf[8 + plen] << 8) |
                    ((uint32_t)buf[9 + plen] << 16) |
                    ((uint32_t)buf[10 + plen] << 24);
                uint32_t crc_calc = BlCrc32(&buf[7], plen);

                if (crc_calc != crc_rx)
                {
                    uint8_t nack = NACK;
                    UART_Send(&nack, 1);
                }
                else
                {
                    BlDispatchFrame(buf, plen);
                }
                idx = 0;
                stage = 0;
            }
            continue;
        }
    }
}

/* 镜像长度合法：非 0 且不超出 APP 区（防止越界读 Flash 触发 HardFault） */
#define IMAGE_LEN_OK(len)  ((len) > 0U && (len) <= APP_LEN_MAX)

/**
  * @brief  ERASE：先擦元数据使旧 APP 失效，再擦 APP 区
  *         顺序保证：擦到一半掉电时 Bootloader 不会跳进残缺镜像
  * @retval 1=成功，0=失败
  */
static int BlEraseImage(uint32_t total_len)
{
    if (!IMAGE_LEN_OK(total_len)) return 0;
    if (BlFlashClearMeta() != 0) return 0;
    return BlFlashEraseRange(APP_START, total_len) == 0;
}

/**
  * @brief  FINISH：对 Flash 中已写入的镜像重算 CRC32，通过则写元数据（含 magic）
  * @retval 1=通过，0=失败
  */
static int BlFinishImage(uint32_t total_len, uint32_t crc_expect)
{
    if (!IMAGE_LEN_OK(total_len)) return 0;
    uint32_t crc_calc = BlCrc32((const uint8_t *)APP_START, total_len);
    if (crc_calc != crc_expect) return 0;
    return BlFlashWriteMeta(total_len, crc_calc) == 0;
}

/**
  * @brief  命令分发：ERASE / WRITE / FINISH / REBOOT
  */
void BlDispatchFrame(const uint8_t *hdr, uint16_t plen)
{
    uint32_t addr = (uint32_t)hdr[1] | ((uint32_t)hdr[2] << 8) |
                    ((uint32_t)hdr[3] << 16) | ((uint32_t)hdr[4] << 24);
    int ok = 0;

    switch (hdr[0])
    {
    case CMD_ERASE:  /* DATA = 镜像总长度(4B LE)，按长度擦对应 sector */
        ok = (plen >= 4U) && BlEraseImage(LE32(&hdr[7]));
        break;
    case CMD_WRITE:  /* ADDR = 目标地址（4 字节对齐），DATA = 分块(<=256B) */
        ok = (plen > 0U) && ((addr & 0x3U) == 0U)
             && (addr >= APP_START && addr + plen <= APP_START + APP_LEN_MAX)
             && (BlFlashWriteRange(addr, &hdr[7], plen) == 0);
        break;
    case CMD_FINISH: /* DATA = 镜像总长度(4B LE) + CRC32(4B LE) */
        ok = (plen >= 8U) && BlFinishImage(LE32(&hdr[7]), LE32(&hdr[11]));
        break;
    case CMD_REBOOT:
        NVIC_SystemReset();
        break;
    default:
        ok = 0;
        break;
    }

    if (hdr[0] != CMD_REBOOT)
    {
        uint8_t resp = ok ? ACK : NACK;
        UART_Send(&resp, 1);
    }
}
