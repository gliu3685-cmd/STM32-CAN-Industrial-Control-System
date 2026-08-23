# Day31 - Bootloader 跳转 APP（MSP + 函数指针 + 有效性校验）

## 1. 今日目标（Objectives）

本日打通 "Bootloader → APP" 的第一跳，重点掌握：

* 向量表前两个字（初始 MSP、Reset_Handler）的含义
* 跳转三步：关中断 → 设 MSP → 函数指针跳 Reset_Handler
* 跳转前如何判断 APP 有效（栈指针范围 + magic）

> 代码完成；跳转上电验证随 Bootloader 烧录一并延后。

---

## 2. 理论学习（Theory）

### 2.1 向量表前两个字就是"启动手册"

任何 Cortex-M 程序的向量表开头固定是：

```text
地址 +0x00：初始栈指针 MSP
地址 +0x04：Reset_Handler 地址
```

CPU 复位就是照着这两个字启动的。Bootloader 跳转 APP 也一样——把 APP 的这两个字读出来，照着做。

### 2.2 跳转三步

1. `__disable_irq()`：关中断——Bootloader 期间开过的中断（如 UART）不能带进 APP，否则状态错乱；
2. `__set_MSP(sp)`：把栈指针换成 APP 的初始 MSP（APP 用自己的栈）；
3. `reset()`：把函数指针指到 APP 的 Reset_Handler 并调用，**永不返回**（APP 的 Reset_Handler 会重新初始化 SystemInit/时钟/HAL，所以不能跳 main，必须跳 Reset_Handler）。

### 2.3 有效性校验

跳转前先确认 APP 区真的有个能跑的程序：

* 首字（MSP）落在合法 RAM 范围 0x20000000~0x2001FFFF
* `APP_START + 0x100` 处 magic == 0xA5A5A5A5（升级成功后由 Bootloader 写入）

两者都过才跳，防止跳进空白 Flash 直接 HardFault。

---

## 3. 实验环境（Environment）

硬件：STM32F407ZGTx + ST-Link + USB-TTL（待烧录验证）

软件：STM32CubeIDE 2.2.0（CLI 编译）

工程：`firmware/bootloader`、`firmware/master_node`（APP）

当前阶段：第五阶段 IAP Bootloader

---

## 4. 代码实现

### 4.1 跳转函数（`bl_flash.c`）

```c
void BlJumpToApp(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_START;          /* 读 APP 初始 MSP */
    void (*reset)(void) = (void (*)(void))(*(volatile uint32_t *)(APP_START + 4));

    __disable_irq();          /* 跳转前关闭中断 */
    __set_MSP(sp);
    reset();                  /* 永不返回 */
}
```

### 4.2 有效性判断（`bl_flash.c`）

```c
int BlAppValid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_START;
    uint32_t magic = *(volatile uint32_t *)(APP_START + 0x100U);
    return (sp >= 0x20000000U && sp < 0x20020000U) && (magic == APP_MAGIC);
}
```

### 4.3 启动路径（`main.c`）

```c
if (got && ch == 'u')      { BlProtocolRun(); }   /* 升级模式 */
else if (got && ch == 'j') { BlJumpToApp(); }     /* 强制跳转（调试） */
else {
    if (BlAppValid()) { BlJumpToApp(); }          /* 默认：校验后跳转 */
    printf("NO APP\r\n");
}
```

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译验证（已完成）

现象：`Build Finished. 0 errors, 0 warnings`。

### 实验 2：跳转上电验证（待 Bootloader 烧录）

预期：Bootloader 打印 `BOOT v0.1` → 校验通过 → 串口出现 `F407 App v1.0 Start!`。

状态：⏳ 随 Bootloader 烧录（遵守烧录顺序约束）。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：为什么跳转前必须关中断

问题：不关中断直接跳，APP 起来后中断状态不确定。

原因：Bootloader 的外设中断（如 UART 接收）可能正挂着，带着旧上下文进 APP 会错乱。

解决：`__disable_irq()`，中断的重新初始化交给 APP 的 Reset_Handler/HAL。

### Problem 2：为什么先校验再跳

问题：直接跳空白/无效 Flash 地址会 HardFault。

解决：栈指针范围 + magic 双条件校验，过不了就打印 `NO APP` 停在原地。

---

## 7. 今日成果（Result）

完成：

* [x] BlJumpToApp（关中断 + MSP + 函数指针）
* [x] BlAppValid（栈指针范围 + magic）
* [x] 启动路径 u/j/默认三分支
* [x] CLI 编译 0 errors
* [ ] 跳转上电验证（待 Bootloader 烧录）

---

## 8. 工程总结（Engineering Summary）

## 跳转不是 goto，是"照手册重启"

跳 APP 的本质是模拟一次复位：按 APP 自己的向量表设置 MSP 并进入它的 Reset_Handler，让 APP 从零开始完整初始化。

---

## 9. 面试问答（Interview Prep）

### Q1：Bootloader 怎么跳转到 APP？

答题要点：

* 关中断
* 从 APP 向量表读初始 MSP 并 `__set_MSP`
* 函数指针指向 APP 的 Reset_Handler 并调用

### Q2：为什么跳 Reset_Handler 而不是直接跳 main？

答题要点：

* Reset_Handler 负责 SystemInit（时钟）、初始化栈/数据段、调用 HAL
* 直接跳 main 会跳过初始化，外设和内存状态不对

### Q3：怎么判断 APP 有效？

答题要点：

* 初始 MSP 是否落在合法 RAM 范围
* 固定偏移处 magic 标志是否正确
* 升级场景再加 CRC32 全量校验（Day34）

---

## 10. Git 提交

```bash
git add firmware/bootloader docs/day31.md 学习进度.md
git commit -m "docs(day31): bootloader jump - MSP setup, function-pointer reset, APP validity check"
```

---

## 11. 下一步计划（Next Step）

Day32：升级模式设计

* 普通启动 vs 升级等待两条路径（u/j/默认已就绪）
* 进入升级模式后的协议循环
