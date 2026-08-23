# Day30 - Application 地址偏移（APP 从 0x08010000 运行）

## 1. 今日目标（Objectives）

本日进入第五阶段 Bootloader 实操，重点掌握：

* 链接脚本如何决定程序在 Flash 中的位置
* 向量表偏移（VTOR）的原理与设置时机
* APP 在新地址编译、map 验证，为 Day31 跳转做准备

> 代码与文档已完成；"App running" 上电验证随 Bootloader 烧录一并延后（烧录顺序约束）。

---

## 2. 理论学习（Theory）

### 2.1 链接脚本：程序放哪、从哪执行

CubeIDE 工程的 `.ld` 文件用 MEMORY 段描述芯片资源：

```c
FLASH  (rx)  : ORIGIN = 0x08010000,  LENGTH = 960K
```

`ORIGIN` = 代码/向量表起始地址，`LENGTH` = 可用大小。改 ORIGIN 就等于"把 APP 整个搬家"。

### 2.2 向量表与 VTOR

向量表是"中断处理函数地址"的数组，默认位于 0x08000000。复位后 CPU 靠 `SCB->VTOR` 找它：

* 默认 VTOR = 0x08000000（Bootloader 区）
* APP 搬到 0x08010000 后向量表也搬走了，必须把 VTOR 指过去
* 设置时机：**main 第一行、使能任何中断之前**

不设 VTOR 的后果：中断一来 CPU 按 0x08000000 的旧表找处理函数 → 取到错误地址 → HardFault。

---

## 3. 实验环境（Environment）

硬件：STM32F407ZGTx 主控（待 Bootloader 阶段一并上电验证）

软件：STM32CubeIDE 2.2.0（CLI 编译 + map 校验）

工程：`firmware/master_node`（APP）

当前阶段：第五阶段 IAP Bootloader

---

## 4. 代码实现

### 4.1 链接脚本（`STM32F407ZGTX_FLASH.ld`）

```c
FLASH  (rx)  : ORIGIN = 0x08010000,  LENGTH = 960K
```

Bootloader 占前 64KB（Sector 0-3），APP 从 Sector 4（0x08010000）开始，960KB 到 0x08100000。

### 4.2 向量表偏移（`main.c` 第一行）

```c
SCB->VTOR = 0x08010000U;      /* IAP：向量表偏移到 APP 区 */
```

启动打印同步改为 `F407 App v1.0 Start!`，用于区分版本。

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译 + map 验证（已完成）

现象：`Build Finished. 0 errors, 0 warnings`；`f407_blink.map` 中：

```text
FLASH        0x08010000  0x000f0000
.isr_vector  0x08010000
```

验证：链接脚本生效，向量表与代码确实落在 APP 区。

### 实验 2：App running 上电验证（待 Bootloader）

预期：Bootloader 跳转后串口打印 `F407 App v1.0 Start!`。

状态：⏳ 随 Bootloader 烧录验证（遵守"PID 完成后"约束）。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：直接烧录 APP 到新地址后上电不跑

问题：APP 单独烧到 0x08010000 后，复位无输出。

原因：CPU 复位固定从 0x08000000 取向量表；0x08000000 没有 Bootloader（或旧数据）。

解决：0x08000000 必须常驻 Bootloader，由它校验后跳转 APP（Day31）。

---

## 7. 今日成果（Result）

完成：

* [x] 链接脚本 ORIGIN 0x08010000 / LENGTH 960K
* [x] main() 首行设置 VTOR
* [x] CLI 编译 0 errors + map 验证 `.isr_vector @ 0x08010000`
* [ ] App running 上电打印（待 Bootloader 烧录）

---

## 8. 工程总结（Engineering Summary）

## 地址是程序的一部分

链接脚本决定了代码"住在哪"，VTOR 决定了中断表"在哪查"——IAP 的本质就是给 APP 重新安排住处，并告诉 CPU 新的门牌号。

---

## 9. 面试问答（Interview Prep）

### Q1：IAP 中为什么要改链接脚本的 ORIGIN？

答题要点：

* 0x08000000 留给 Bootloader，APP 必须让位
* ORIGIN 决定代码与向量表起始地址
* APP 从 0x08010000 起，Bootloader 才能跳转过去

### Q2：VTOR 不设置会怎样？

答题要点：

* 中断按 0x08000000 查向量表，取到错误处理地址
* 一开中断就 HardFault/跑飞
* 必须在使能中断前设置

### Q3：为什么复位后 CPU 不能直接跑新地址的 APP？

答题要点：

* 复位取向量表的位置由硬件决定（0x08000000）
* APP 搬走后 0x08000000 无有效入口
* 需要 Bootloader 在 0x08000000 做引导跳转

---

## 10. Git 提交

```bash
git add firmware/master_node docs/day30.md 学习进度.md
git commit -m "docs(day30): APP relocation - linker origin 0x08010000 + VTOR"
```

---

## 11. 下一步计划（Next Step）

Day31：Bootloader 跳转 APP

* 设置 MSP + 函数指针跳转
* 跳转前校验 APP 有效性（栈指针范围 + magic）
* 打通"Bootloader → APP"第一跳
