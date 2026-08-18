#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""upgrade_tool.py — F407 UART IAP 升级工具。

用法：python upgrade_tool.py COM3 app.bin
流程：等待 BOOT 横幅 → 发 'u' 进入升级模式 → ERASE →
      分包 WRITE(<=256B) → FINISH(总长 4B LE + CRC32 4B LE) → REBOOT。
每帧超时 500ms、重试 3 次；任一帧失败退出码 1。
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
RETRIES = 3
TIMEOUT = 0.5


def frame(cmd, addr, payload):
    data = bytes([cmd]) + addr.to_bytes(4, "little") + \
        len(payload).to_bytes(2, "little") + payload
    return b"\xA5\x5A" + data + \
        (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "little")


def wait_ack(ser, timeout=TIMEOUT):
    """返回 True=ACK, False=NACK, None=超时。"""
    end = time.time() + timeout
    while time.time() < end:
        b = ser.read(1)
        if b == bytes([ACK]):
            return True
        if b == bytes([NACK]):
            return False
    return None


def send(ser, buf):
    """发一帧并等待 ACK，超时重试 RETRIES 次。"""
    for _ in range(RETRIES):
        ser.write(buf)
        r = wait_ack(ser)
        if r is True:
            return True
        if r is False:
            return False
    return False


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: python upgrade_tool.py COMx app.bin")
    port, path = sys.argv[1], sys.argv[2]

    try:
        image = open(path, "rb").read()
    except OSError as exc:
        sys.exit("cannot read %s: %s" % (path, exc))

    ser = serial.Serial(port, 115200, timeout=1)
    ser.reset_input_buffer()

    ser.timeout = 5
    if not ser.read_until(b"BOOT"):
        sys.exit("no BOOT banner on %s" % port)
    ser.timeout = 1

    ser.write(b"u")                        # 进入升级模式
    time.sleep(0.3)
    ser.reset_input_buffer()               # 丢弃 "UPGRADE MODE" 横幅

    if not send(ser, frame(CMD["ERASE"], APP_START,
                           len(image).to_bytes(4, "little"))):
        sys.exit("ERASE failed")
    print("ERASE OK")

    for off in range(0, len(image), 256):
        chunk = image[off:off + 256]
        if not send(ser, frame(CMD["WRITE"], APP_START + off, chunk)):
            sys.exit("WRITE failed at 0x%08X" % (APP_START + off))
        print("WRITE 0x%08X +%d" % (APP_START + off, len(chunk)))

    crc = (zlib.crc32(image) & 0xFFFFFFFF).to_bytes(4, "little")
    if not send(ser, frame(CMD["FINISH"], 0,
                           len(image).to_bytes(4, "little") + crc)):
        sys.exit("FINISH failed")
    print("FINISH OK, rebooting...")

    ser.write(frame(CMD["REBOOT"], 0, b""))
    sys.exit(0)


if __name__ == "__main__":
    main()
