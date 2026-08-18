#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""proto_smoke.py — Bootloader 升级流程冒烟测试（Task 6 联调用）。

流程：等 BOOT 横幅 → 发 'u' 进入升级模式 → ERASE(0x08010000, 4096) →
分 16 帧 WRITE 写入 0xAA → FINISH(4096, CRC32) → REBOOT。
每帧等待 ACK，任一失败即退出非零。

用法：python proto_smoke.py COM3
依赖：pyserial（pip install pyserial）
"""

import sys
import time
import zlib

import serial

ACK = 0x06
NACK = 0x15
CMD = {"ERASE": 0x01, "WRITE": 0x02, "FINISH": 0x03, "REBOOT": 0x04}
APP_START = 0x08010000


def frame(cmd, addr, payload):
    data = bytes([cmd]) + addr.to_bytes(4, "little") + \
        len(payload).to_bytes(2, "little") + payload
    return b"\xA5\x5A" + data + \
        (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "little")


def wait_ack(ser, timeout=1.0):
    end = time.time() + timeout
    while time.time() < end:
        b = ser.read(1)
        if b == bytes([ACK]):
            return True
        if b == bytes([NACK]):
            return False
    return None


def send_and_wait(ser, buf, tag):
    ser.write(buf)
    r = wait_ack(ser)
    if r is not True:
        sys.exit("%s failed (ack=%r)" % (tag, r))
    print("%s OK" % tag)


def main():
    port = sys.argv[1]
    image = b"\xAA" * 4096
    ser = serial.Serial(port, 115200, timeout=1)
    ser.reset_input_buffer()

    ser.timeout = 5
    if not ser.read_until(b"BOOT"):
        sys.exit("no BOOT banner")
    ser.timeout = 1

    ser.write(b"u")                      # 进入升级模式
    time.sleep(0.3)
    ser.reset_input_buffer()             # 丢弃 "UPGRADE MODE" 横幅

    send_and_wait(ser, frame(CMD["ERASE"], APP_START,
                             len(image).to_bytes(4, "little")), "ERASE")

    for off in range(0, len(image), 256):
        chunk = image[off:off + 256]
        send_and_wait(ser, frame(CMD["WRITE"], APP_START + off, chunk),
                      "WRITE 0x%08X" % (APP_START + off))

    crc = (zlib.crc32(image) & 0xFFFFFFFF).to_bytes(4, "little")
    send_and_wait(ser, frame(CMD["FINISH"], 0,
                             len(image).to_bytes(4, "little") + crc), "FINISH")

    ser.write(frame(CMD["REBOOT"], 0, b""))
    print("REBOOT sent")


if __name__ == "__main__":
    main()
