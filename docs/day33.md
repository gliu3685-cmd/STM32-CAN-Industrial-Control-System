# Day33 - UART 固件传输（帧协议 + 分包写入 + 应答重传）

## 1. 今日目标（Objectives）

本日打通"电脑发 bin → Bootloader 写 Flash"的传输链路，重点掌握：

* 帧格式：A5 5A | CMD | ADDR | LEN | DATA | CRC32
* 命令语义：ERASE / WRITE / FINISH / REBOOT
* ACK/NACK 应答与超时重传

---

## 2. 理论学习（Theory）

### 2.1 帧格式

```text
A5 5A | CMD(1B) | ADDR(4B LE) | LEN(2B LE) | DATA(<=256B) | CRC32(4B LE, 仅 DATA)
```

* 帧头 A5 5A：同步识别，噪声字节自动丢弃
* ADDR 小端：本次操作的目标 Flash 地址
* LEN：DATA 长度，上限 256（缓冲小、Flash 编程粒度友好）
* CRC32：只覆盖 DATA，校验单帧数据是否被串口噪声破坏

### 2.2 命令

| 命令 | 含义 |
|---|---|
| ERASE | 按镜像总长度擦除 APP 扇区 |
| WRITE | 写一帧数据到指定地址 |
| FINISH | 传镜像总长度 + 全图 CRC32，Bootloader 重算比对 |
| REBOOT | 复位，走启动路径 |

### 2.3 可靠传输

每帧收到后回 `0x06`(ACK) 或 `0x15`(NACK)；上位机超时 500ms、重试 3 次。帧级 CRC + 应答 + 重试，保证传输可靠。

---

## 3. 实验环境（Environment）

硬件：STM32F407ZGTx + USB-TTL（待烧录验证）

软件：Python 3 + pyserial、STM32CubeIDE 2.2.0

工程：`firmware/bootloader`、`tools/upgrade_tool.py`、`tools/proto_smoke.py`

当前阶段：第五阶段 IAP Bootloader

---

## 4. 代码实现

### 4.1 固件侧：命令分发（`bl_protocol.c`）

```c
case CMD_ERASE:  ok = (BlFlashEraseRange(APP_START, LE32(&b[7])) == 0); break;
case CMD_WRITE:  ok = (addr >= APP_START && addr + plen <= APP_START + APP_LEN_MAX)
                     && (BlFlashWriteRange(addr, &b[7], plen) == 0); break;
case CMD_FINISH: ok = BlFinishImage(LE32(&b[7]), LE32(&b[11])); break;
case CMD_REBOOT: NVIC_SystemReset(); break;
```

关键点：WRITE 校验目标地址必须落在 APP 区，防止把 Bootloader 区覆盖掉。

### 4.2 上位机（`tools/upgrade_tool.py`）

```python
for off in range(0, len(image), 256):
    chunk = image[off:off+256]
    if not send(ser, frame(CMD["WRITE"], APP_START+off, chunk)):
        sys.exit("WRITE failed at ...")
```

流程：等 BOOT → 发 `u` → ERASE → 分包 WRITE → FINISH → REBOOT。

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译 + 脚本语法（已完成）

现象：Bootloader 0 errors；`upgrade_tool.py`/`proto_smoke.py` py_compile 通过。

### 实验 2：端到端升级（待烧录）

预期：工具全程 ACK，FINISH 校验通过，重启后跑新 APP。

状态：⏳ 随 Bootloader 烧录。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：2 字节长度字段放不下大镜像

问题：LEN(2B) 最大 65535，而 APP 区 960KB。

解决：ERASE/FINISH 用 DATA 携带 4 字节镜像总长；WRITE 的 LEN 只是单帧块长（≤256）。

### Problem 2：升级中途断线会不会半砖

问题：擦除后写入一半断电，APP 区不完整。

解决：FINISH 全镜像 CRC 不通过就不写 magic → 下次启动 `NO APP`，可重新升级恢复。

---

## 7. 今日成果（Result）

完成：

* [x] 帧协议（A5 5A + 命令 + 地址 + CRC）
* [x] ERASE/WRITE/FINISH/REBOOT 分发 + 地址范围校验
* [x] Python 上位机分包/应答/重试
* [x] CLI 编译 + 脚本语法通过
* [ ] 端到端升级实测（待 Bootloader 烧录）

---

## 8. 工程总结（Engineering Summary）

## 协议是双方约定，不是单方实现

固件解析与上位机组帧必须逐字节一致（帧头、小端、CRC 覆盖范围）——改任何一边都要同步改另一边。

---

## 9. 面试问答（Interview Prep）

### Q1：串口传输怎么保证不丢数据？

答题要点：

* 帧头同步 + 长度 + 单帧 CRC
* 每帧 ACK/NACK，超时重试

### Q2：为什么 WRITE 前要先 ERASE？

答题要点：

* Flash 只能 1→0 编程，不能直接改写
* 按扇区擦除后才能写入

### Q3：为什么要校验 WRITE 地址范围？

答题要点：

* 防止脏帧把 Bootloader 区覆盖
* 只允许写 APP 区（0x08010000 ~ 0x08100000）

---

## 10. Git 提交

```bash
git add firmware/bootloader tools docs/day33.md 学习进度.md
git commit -m "docs(day33): UART transfer protocol - frame format, commands, ACK/retry"
```

---

## 11. 下一步计划（Next Step）

Day34：CRC32 完整性校验

* 两端 CRC32 一致性（固件位运算 vs Python zlib）
* FINISH 全镜像校验，损坏固件拒绝升级
