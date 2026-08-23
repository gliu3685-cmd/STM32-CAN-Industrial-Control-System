# Day32 - 升级模式设计（普通启动 vs 升级等待）

## 1. 今日目标（Objectives）

本日实现"要不要进升级模式"的选择逻辑，重点掌握：

* 普通启动路径与升级等待路径的区分
* 1s 握手窗口的意义（串口命令进升级，超时自动跑 APP）
* 升级模式下的协议循环（BlProtocolRun 不返回）

---

## 2. 理论学习（Theory）

### 2.1 为什么需要两条路径

Bootloader 上电后要回答一个问题：**这次是正常启动 APP，还是要等电脑来升级？**

* 普通启动：校验 APP 有效 → 直接跳转（工业默认"无指令 = 安全态"）
* 升级等待：收 UART 命令/协议帧 → 擦写 Flash → 升级完成后重启

### 2.2 1s 握手窗口

上电后 Bootloader 留 1 秒窗口等一个字节：

* 收到 `u` → 升级模式（不再跳 APP）
* 收到 `j` → 强制跳转（调试用，跳过校验）
* 超时 → 默认路径（校验后跳 APP 或报 NO APP）

窗口的意义：平时上电不用额外操作就能跑 APP，需要升级时电脑在 1s 内发 `u` 即可，两边都省事。

---

## 3. 实验环境（Environment）

硬件：STM32F407ZGTx + USB-TTL（待烧录验证）

软件：STM32CubeIDE 2.2.0（CLI 编译）

工程：`firmware/bootloader`

当前阶段：第五阶段 IAP Bootloader

---

## 4. 代码实现

`main.c` 启动路径（1s 窗口 + 三分支）：

```c
for (uint32_t i = 0; i < 1000; i++) {
    if (UART_GetByte(&ch)) { got = 1; break; }
    HAL_Delay(1);
}
if (got && ch == 'u')      { BlProtocolRun(); }   /* 升级模式，不返回 */
else if (got && ch == 'j') { BlJumpToApp(); }     /* 强制跳转（调试） */
else {
    if (BlAppValid()) { BlJumpToApp(); }
    printf("NO APP\r\n");
}
```

升级模式进入 `BlProtocolRun()` 后进入协议状态机（Day33 细讲），不再返回。

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译验证（已完成）

现象：`Build Finished. 0 errors, 0 warnings`。

### 实验 2：路径选择验证（待烧录）

预期：上电不发字节 → `NO APP`（无有效 APP 时）；1s 内发 `u` → `UPGRADE MODE`；发 `j` → 跳转。

状态：⏳ 随 Bootloader 烧录。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：升级命令和普通数据怎么区分

问题：既想上电直跑 APP，又想能进升级。

解决：用握手窗口 + 特定字节（`u`/`j`）区分；升级帧有自己独立的协议头（A5 5A），不会误判。

---

## 7. 今日成果（Result）

完成：

* [x] u/j/默认三分支启动路径
* [x] 1s 握手窗口（UART_GetByte 非阻塞轮询）
* [x] CLI 编译 0 errors
* [ ] 路径选择上电验证（待 Bootloader 烧录）

---

## 8. 工程总结（Engineering Summary）

## 把"要不要升级"做成显式状态

用超时窗口把启动路径收敛成明确三分支，避免"永远在等升级"或"永远跳过升级"的两种错误。

---

## 9. 面试问答（Interview Prep）

### Q1：怎么进入升级模式？

答题要点：

* 上电 1s 握手窗口内发 `u` 进入升级
* 也可用按键/跳线（本实现用串口命令，最省硬件）

### Q2：为什么用超时窗口而不是一直等？

答题要点：

* 一直等会让正常启动变慢/卡死
* 窗口保证"无操作时快速进入 APP"

### Q3：升级模式下 Bootloader 应该做什么？

答题要点：

* 收帧 → 校验 → 擦写 Flash → 校验完整性 → 写有效标志 → 重启
* 全程不跳 APP，避免升级中被"打断"

---

## 10. Git 提交

```bash
git add firmware/bootloader docs/day32.md 学习进度.md
git commit -m "docs(day32): upgrade mode design - dual boot path with 1s handshake"
```

---

## 11. 下一步计划（Next Step）

Day33：UART 固件传输

* 帧格式与命令（ERASE/WRITE/FINISH/REBOOT）
* ACK/NACK + 超时重传
