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
                if (plen > 256) { idx = 0; stage = 0; continue; }
                stage = 3;
            }
            continue;
        }

        if (stage == 3)                    /* DATA + CRC32(4B，仅 DATA) */
        {
            buf[7 + idx] = rx;
            idx++;
            if (idx == plen + 4)
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

/**
  * @brief  命令分发（Task 5 阶段：仅确认帧合法，不执行命令；Task 6 替换）
  */
void BlDispatchFrame(const uint8_t *hdr, uint16_t plen)
{
    (void)hdr;
    (void)plen;
    uint8_t ack = ACK;
    UART_Send(&ack, 1);
}
