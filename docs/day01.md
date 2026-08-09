# Day01 - STM32 开发环境搭建

## 1. 今日目标（Objectives）

建立 STM32 开发环境，完成第一次程序下载运行，跑通
"配置 → 生成 → 编译 → 下载 → 测试"完整链路。

今日完成：

* 配置 STM32CubeMX
* 配置 STM32CubeIDE
* 完成 ST-Link 下载调试
* 创建第一个 STM32 工程（LED Blink）

---

## 2. 理论学习（Theory）

### 2.1 STM32CubeMX 是什么

STM32CubeMX 是 ST 官方的**图形化配置工具**：选择芯片型号、勾选外设
（GPIO/UART/Timer/CAN 等）、配置时钟树，然后一键生成工程初始化代码。
它解决的是"寄存器初始化代码又长又容易写错"的问题——工程师只关心
业务逻辑，初始化交给工具。

### 2.2 STM32CubeIDE 与标准开发流程

STM32CubeIDE 是基于 Eclipse 的集成开发环境，内置编译器（arm-none-eabi-gcc）
和调试器。标准流程：

```
STM32CubeMX（配置外设与时钟）
    ↓
Generate Code（生成初始化代码）
    ↓
CubeIDE Compile（编译）
    ↓
ST-Link Download（下载到芯片）
    ↓
Hardware Test（硬件测试）
```

### 2.3 ST-Link 与 SWD

ST-Link 是 ST 的调试下载器，通过 **SWD（Serial Wire Debug）** 接口连接芯片，
负责烧录程序、在线调试（断点、单步、读寄存器）。

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2

软件：

* STM32CubeMX
* STM32CubeIDE

工程：`f407_blink`（主工程，与仓库 master_node 同步）

当前阶段：Phase 1 STM32 基础

---

## 4. 开发流程（Development Process）

### 4.1 STM32CubeMX 配置

完成：

* MCU 选择 STM32F407VET6
* Clock Configuration（系统时钟 168MHz）
* GPIO 配置
* 工程生成

### 4.2 STM32CubeIDE 编译下载

完成：

* 导入 CubeMX 工程
* 编译工程
* 下载程序

---

## 5. 首次测试（First Test）

实现：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);   /* LED 闪烁 */
```

测试结果：

* STM32 成功运行
* ST-Link 下载正常

---

## 6. 实验过程（Experiment）

现象：

* 程序下载成功，开发板 LED 以设定周期翻转；
* CubeIDE 控制台显示下载完成、程序启动。

验证：

* LED 闪烁说明 CPU 正常取指执行、GPIO 输出正常；
* ST-Link 链路（USB → SWD → 芯片）可用。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：ST-Link 无法识别

问题：CubeIDE 找不到 ST-Link 调试器，无法下载。

原因：驱动/固件/连接问题。

解决：

* 检查 USB 驱动是否安装
* 更新 ST-Link Firmware（ST-Link 自带升级工具）
* 重新连接调试器并复位板子

---

## 8. 今日成果（Result）

完成：

* [x] STM32CubeMX 工程创建与代码生成
* [x] STM32CubeIDE 编译与下载
* [x] ST-Link 调试链路验证
* [x] 第一个 LED Blink 程序运行

---

## 9. 工程总结（Engineering Summary）

本日学习重点：

## 工具链决定开发效率

STM32 的标准开发链路是"CubeMX 配置 + CubeIDE 编译 + ST-Link 下载"。
理解每一步干什么、产物是什么，后面所有实验都复用这条链路。

## 第一个程序的意义

LED Blink 虽然简单，但验证了 CPU 时钟、GPIO、下载调试三件事同时正常，
是嵌入式"Hello World"。

---

## 10. 面试问答（Interview Prep）

### Q1：描述一下 STM32 的标准开发流程

答题要点：

* CubeMX 图形化配置外设和时钟，生成初始化代码；
* CubeIDE 编译链接生成 .elf/.hex；
* ST-Link 通过 SWD 下载并支持在线调试；
* 上电验证硬件行为。

### Q2：CubeMX 生成的代码可以直接改吗？

答题要点：

* 可以改，但要改在 `USER CODE BEGIN/END` 保护区之间；
* 保护区外的代码在重新生成时会被覆盖；
* 工程实践中自己新增的代码一律放保护区。

### Q3：ST-Link 是什么？SWD 和 JTAG 区别？

答题要点：

* ST-Link 是调试下载器，SWD 只需 2 根信号线（SWDIO/SWCLK）；
* JTAG 引脚更多，SWD 更省引脚，是 Cortex-M 主流调试接口。

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day01 STM32 environment setup and LED blink"
```

---

## 12. 下一步计划（Next Step）

Day02：GPIO 控制与 UART 通信（见 [day02.md](day02.md)）。
