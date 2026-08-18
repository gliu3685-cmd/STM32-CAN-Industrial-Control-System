# Day29 - IAP Bootloader 理论：启动流程、向量表与 Flash 分区设计

## 1. 今日目标（Objectives）

本日进入第五阶段 IAP Bootloader 的学习，重点掌握：

* STM32 复位启动流程（初始 SP、Reset_Handler、向量表）
* 向量表与 VTOR 偏移原理（APP 搬迁后中断为什么还能工作）
* F407ZGTx Flash 分区与 Bootloader/APP 地址分配设计
* 输出 Flash 布局说明，并落地 Bootloader 工程骨架与 APP 搬迁（代码随计划先行完成）

---

## 2. 理论学习（Theory）

### 2.1 STM32 复位启动流程

Cortex-M 复位后硬件固定执行三步：

1. 从地址 0x00000000 读取初始栈指针（MSP）；
2. 从地址 0x00000004 读取复位入口（Reset_Handler）；
3. 初始化 MSP 后跳转 Reset_Handler 执行。

当 BOOT0=0（从主 Flash 启动）时，F4 的 0x00000000 由硬件别名映射到 Flash 0x08000000。因此“CPU 每次复位都从 0x08000000 开始”这句话的准确含义是：**向量表的首两个字（初始 SP 与 Reset_Handler 地址）必须位于 0x08000000**。这也是 IAP 中 Bootloader 必须常驻 0x08000000 的根因。

### 2.2 向量表与 VTOR

向量表是一张“异常/中断处理函数地址”的数组，Cortex-M3/4 用 SCB->VTOR（地址 0xE000ED08）定位它的基址。复位默认 VTOR=0x08000000。

APP 被搬到 0x08010000 后，它的向量表也到了 0x08010000。若不设置 VTOR，任何中断触发时 CPU 仍去 0x08000000 查向量表——那里是 Bootloader 的向量表（或空数据），取到的“处理函数地址”是错的，表现为**一开中断就进 HardFault/跑飞**。因此 APP 的 main() 第一行必须执行：

```c
SCB->VTOR = 0x08010000U;
```

设置时机必须在使能任何中断之前。

### 2.3 Flash 分区与地址分配

F407ZGTx 的 1024KB Flash 扇区布局（擦除最小单位是扇区）：

| 扇区 | 大小 | 地址范围 |
|---|---|---|
| Sector 0-3 | 各 16KB | 0x08000000 ~ 0x0800FFFF |
| Sector 4 | 64KB | 0x08010000 ~ 0x0801FFFF |
| Sector 5-11 | 各 128KB | 0x08020000 ~ 0x080FFFFF |

分区设计遵循“从扇区边界对齐”原则，Bootloader 与 APP 可以独立擦写互不干扰：

* **Bootloader 区**：0x08000000 ~ 0x0800FFFF（64KB，Sector 0-3）
* **APP 区**：0x08010000 ~ 0x08100000（960KB，Sector 4-11）
* **有效标志**：APP_START + 0x100 写入 0xA5A5A5A5（向量表保留区），配合栈指针范围校验判断 APP 是否可跳转

---

## 3. 实验环境（Environment）

硬件：

* STM32F407ZGTx 主控（LQFP144，1024KB Flash）
* USB-TTL 串口（USART1 115200）、ST-Link（烧录验证待硬件就绪）

软件：

* STM32CubeIDE 2.2.0（headless CLI 编译验证）

工程：`firmware/bootloader`（Bootloader）+ `firmware/master_node`（APP）

当前阶段：第五阶段 IAP Bootloader

---

## 4. Flash 布局说明（今日输出）

```text
0x08000000 ┌──────────────────────┐
           │ Bootloader（64KB）   │  Sector 0-3（各 16KB）
0x0800FFFF ├──────────────────────┤
0x08010000 ┌──────────────────────┐
           │ APP（960KB）         │  Sector 4-11
           │ 向量表 + 有效标志     │  magic @ +0x100
0x08100000 └──────────────────────┘
```

链接脚本对应：

* Bootloader：`FLASH ORIGIN = 0x08000000, LENGTH = 64K`
* APP：`FLASH ORIGIN = 0x08010000, LENGTH = 960K`

---

## 5. 工程落地（代码已随 bootloader-plan 完成）

### 5.1 Bootloader 工程骨架

`firmware/bootloader`：最小工程（USART1 115200 + BOOT v0.1 打印），链接在 0x08000000。核心启动路径：

```c
printf("BOOT v0.1\r\n");
/* 1s 握手窗口：u=升级，j=强制跳转，超时=默认校验跳转 */
if (BlAppValid()) { BlJumpToApp(); }
```

### 5.2 APP 工程搬迁

`firmware/master_node`：链接脚本改到 0x08010000 / 960K，main() 首行设置向量表偏移：

```c
SCB->VTOR = 0x08010000U;
```

---

## 6. 实验过程（Experiment）

### 实验 1：Bootloader 工程 CLI 编译验证

现象：`Build Finished. 0 errors, 0 warnings`；`f407_bootloader.elf` text 约 10.8KB，远小于 64KB 分区。

验证：工程结构（.cproject/.project/链接脚本）与源码自洽，headless 导入编译通过。

### 实验 2：APP 搬迁 CLI 编译验证

现象：`Build Finished. 0 errors, 0 warnings`；`f407_blink.map` 中 `.isr_vector 0x08010000`。

验证：链接脚本生效，代码与向量表确实落在 APP 区。

### 实验 3：上电联调（待硬件）

预期现象：ST-Link 烧 Bootloader → 烧 APP → 上电串口依次见 `BOOT v0.1` → `F407 App v1.0 Start!`；无 ST-Link 时延后。

状态：⏳ 待 ST-Link/USB-TTL 就绪。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：芯片型号歧义（VET6 vs ZGTx）

问题：硬件文档写 F407VET6（512KB），但工程配置为 F407ZGTx（1024KB）。

原因：CubeIDE 工程创建时设备型号与文档记录不一致。

解决：用户确认板载丝印为 STM32F407ZGT6，按 1024KB 布局执行（APP 960KB），master 工程设备配置无需修改。

### Problem 2：CubeIDE 工程无法用 GUI 创建

问题：自动化环境没有 CubeIDE 图形界面，新建工程困难。

原因：工程文件（.project/.cproject）结构复杂。

解决：以 master 工程为模板整体复制，裁剪掉 FreeRTOS/Middlewares 与业务代码，用 headless CLI 反复编译验证，0 errors / 0 warnings。

---

## 8. 今日成果（Result）

完成：

* [x] STM32 启动流程、向量表、VTOR、Flash 分区理论
* [x] F407ZGTx Flash 布局说明（Bootloader 64KB + APP 960KB，扇区边界对齐）
* [x] Bootloader 骨架工程（BOOT v0.1）编译通过
* [x] master APP 搬迁到 0x08010000 + VTOR，编译通过
* [ ] 硬件上电验证（待 ST-Link）

当前第五阶段架构：

```text
复位 → 0x08000000 Bootloader → 校验 APP 有效 → 跳转 0x08010000 APP
                              ↘ 无效/收到 'u' → 升级模式（Day30+ 联调）
```

---

## 9. 工程总结（Engineering Summary）

## Bootloader 与 APP 完全独立链接

两个工程各有自己的向量表、链接脚本与入口，互不引用符号——APP 只需知道自己被搬到哪个地址并设置 VTOR。这是 IAP 最核心的松耦合设计。

## 分区从扇区边界对齐

擦除最小单位是扇区；Bootloader（Sector 0-3）与 APP（Sector 4 起）边界对齐后，升级 APP 时不会误擦 Bootloader，反之亦然。

## 安全默认态

无有效 APP 时 Bootloader 不跳转，避免执行垃圾数据；执行器类系统“无指令 = 安全态”的同一思想。

---

## 10. 面试问答（Interview Prep）

### Q1：STM32 上电后是如何启动的？

答题要点：

* 复位后从 0x00000000 读初始 MSP、从 0x00000004 读 Reset_Handler
* BOOT0=0 时 0x00000000 别名映射到主 Flash 0x08000000
* 先初始化 MSP 再跳转 Reset_Handler，由它完成时钟/外设初始化后进 main

### Q2：IAP 中 APP 为什么必须设置 VTOR？不设置会怎样？

答题要点：

* 中断向量表位置由 SCB->VTOR 决定，默认 0x08000000
* APP 搬到 0x08010000 后向量表跟着搬走，不设 VTOR 则中断按旧地址查表
* 后果：中断处理函数地址错误 → 进 HardFault 或跑飞；必须在使用中断前设置

### Q3：Bootloader 怎么判断 APP 是否有效？

答题要点：

* 校验 APP 首字（初始 SP）是否落在合法 RAM 范围
* 校验固定偏移（APP_START+0x100）的 magic 标志 0xA5A5A5A5
* 升级场景进一步用 CRC32 全量校验固件完整性（Day34 内容）

---

## 11. Git 提交

本日内容（理论 + 布局 + 工程落地）已随计划分步提交（未 push）：

```text
7bb5112 feat(bootloader): F407 UART IAP skeleton (BOOT v0.1, 64KB layout)
71854f3 feat(master): relocate APP to 0x08010000 with VTOR for IAP
2a91c3e docs(bootloader): Phase 5 code complete, update progress and README
```

---

## 12. 下一步计划（Next Step）

Day30（Application 地址偏移实操验证）：

* ST-Link 烧 Bootloader + APP，上电验证 `F407 App v1.0 Start!` 打印
* 验证 Bootloader 默认跳转与 `j` 强制跳转路径
* 为 Day31 升级模式联调建立硬件基础

代码侧 Day29-34 主要内容（跳转/升级协议/CRC32/Python 工具）已提前完成，后续按硬件就绪顺序逐日联调验证。
