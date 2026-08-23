# Day34 - CRC32 完整性校验（损坏固件拒绝升级）

## 1. 今日目标（Objectives）

本日给升级流程加"最后一道闸"，重点掌握：

* CRC32（IEEE 802.3）原理与两端一致性
* FINISH 全镜像校验流程
* 校验不通过 → 拒绝升级（不写 magic）

---

## 2. 理论学习（Theory）

### 2.1 CRC32 是什么

CRC32 把数据按多项式 0xEDB88320（IEEE 802.3，反射）逐位算出一个 32 位余数。与简单校验和不同，CRC 对**任意位翻转**都有很强的检出能力。

标准校验向量：`crc32("123456789") == 0xCBF43926`。

### 2.2 两层校验

* 帧级：每帧 DATA 的 CRC32 → 防传输中噪声改坏单帧
* 全镜像：FINISH 时对整个 Flash 区重算 CRC32 与上位机比对 → 防"帧都对但总体不对"（如漏帧、顺序错）

### 2.3 为什么校验不过就不写 magic

`magic = 0xA5A5A5A5`（0x08010100）是 Bootloader 判断"APP 有效"的唯一标志。升级完成且校验通过才写；校验不过就保持原样（或空），下次启动 `NO APP`，不会跑损坏固件。

---

## 3. 实验环境（Environment）

硬件：STM32F407ZGTx + USB-TTL（待烧录验证）

软件：Python 3（zlib.crc32）、STM32CubeIDE 2.2.0

工程：`firmware/bootloader`（bl_crc.c）、`tools/upgrade_tool.py`

当前阶段：第五阶段 IAP Bootloader

---

## 4. 代码实现

### 4.1 固件侧（`bl_crc.c`，逐位反射实现）

```c
uint32_t BlCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint32_t b = 0; b < 8; b++) {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}
```

与 Python `zlib.crc32` 逐字节一致（已用 0xCBF43926 向量验证）。

### 4.2 FINISH 全镜像校验（`bl_protocol.c`）

```c
static int BlFinishImage(uint32_t total_len, uint32_t crc_expect)
{
    uint32_t crc_calc = BlCrc32((const uint8_t *)APP_START, total_len);
    if (crc_calc != crc_expect) return 0;      /* 不通过 → NACK，不写 magic */
    BlFlashWriteMagic();                        /* 通过 → 写有效标志 */
    return 1;
}
```

### 4.3 上位机（`upgrade_tool.py`）

```python
crc = (zlib.crc32(image) & 0xFFFFFFFF).to_bytes(4, "little")
send(ser, frame(CMD["FINISH"], 0, len(image).to_bytes(4, "little") + crc))
```

---

## 5. 实验过程（Experiment）

### 实验 1：算法一致性验证（已完成）

现象：Python 位运算实现与 `zlib.crc32(b"123456789")` 均为 0xCBF43926。

### 实验 2：损坏固件拒绝（待烧录）

预期：把 bin 末尾 1 字节取反后升级 → FINISH NACK、工具退出码 1；复位后 `NO APP`。

状态：⏳ 随 Bootloader 烧录。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：CRC 表 vs 位运算

问题：查表实现快但 256 项表容易抄错。

解决：用逐位反射实现（速度对 960KB 足够），算法经 Python 验证与 zlib 一致，避免转抄错误。

---

## 7. 今日成果（Result）

完成：

* [x] CRC32 固件实现（与 zlib 一致，向量验证）
* [x] FINISH 全镜像校验 + 失败拒绝
* [x] 损坏固件拒绝逻辑（不写 magic）
* [ ] 损坏固件实测（待 Bootloader 烧录）

---

## 8. 工程总结（Engineering Summary）

## 完整性校验是升级的最后一道闸

没有 CRC，"帧都对但固件错"的坏包也会被当成有效固件；有了全镜像校验，损坏固件在写入前就被拒之门外。

---

## 9. 面试问答（Interview Prep）

### Q1：CRC32 和简单校验和有什么区别？

答题要点：

* 校验和只是按位相加，对特定错误模式不敏感
* CRC 按多项式做除法余数，检出能力强（突发错误/任意位翻转）

### Q2：为什么升级完成才写 magic？

答题要点：

* magic 是"APP 有效"的唯一依据
* 校验通过才写，失败保持无效 → Bootloader 不会跑损坏固件

### Q3：全镜像校验和单帧校验分别防什么？

答题要点：

* 单帧 CRC：防传输噪声改坏某帧
* 全镜像 CRC：防漏帧/乱序/帧都合法但整体不对

---

## 10. Git 提交

```bash
git add firmware/bootloader tools docs/day34.md 学习进度.md
git commit -m "docs(day34): CRC32 integrity - full-image verify, corrupted firmware rejected"
```

---

## 11. 下一步计划（Next Step）

第五阶段 Bootloader 学习完成：

* 代码全部就绪（跳转/升级/CRC/上位机）
* 上电验证待 PID 闭环完成后执行（烧录顺序约束）
