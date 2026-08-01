# Day01 - STM32 开发环境搭建

## 1. 今日目标（Objectives）

建立 STM32 开发环境，完成第一次程序下载运行。

今日完成：

* 配置 STM32CubeMX
* 配置 STM32CubeIDE
* 完成 ST-Link 下载调试
* 创建第一个 STM32 工程

---

## 2. 实验环境（Environment）

MCU：

* STM32F407VET6

开发工具：

* STM32CubeMX
* STM32CubeIDE
* ST-Link V2

---

## 3. 开发流程（Development Process）

### 3.1 STM32CubeMX 配置

完成：

* MCU 选择 STM32F407VET6
* Clock Configuration（系统时钟 168MHz）
* GPIO 配置
* 工程生成

### 3.2 STM32CubeIDE

完成：

* 导入 CubeMX 工程
* 编译工程
* 下载程序

---

## 4. 首次测试（First Test）

实现：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);   /* LED 闪烁 */
```

测试结果：

* STM32 成功运行
* ST-Link 下载正常

---

## 5. 知识总结（Knowledge Learned）

STM32 标准开发流程：

```
STM32CubeMX（配置）
    ↓
Generate Code（生成代码）
    ↓
CubeIDE Compile（编译）
    ↓
ST-Link Download（下载）
    ↓
Hardware Test（硬件测试）
```

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：ST-Link 无法识别

解决：

* 检查 USB 驱动
* 更新 ST-Link Firmware
* 重新连接调试器

---

## 7. 今日成果（Result）

* [x] STM32CubeMX 工程创建与代码生成
* [x] STM32CubeIDE 编译与下载
* [x] ST-Link 调试链路验证
* [x] 第一个 LED Blink 程序运行

---

## 8. 工程总结（Engineering Summary）

完成 STM32 基础开发环境搭建，跑通"配置 → 生成 → 编译 → 下载 → 测试"完整链路，为后续 HAL 库和 RTOS 开发建立基础。

---

## 9. 下一步计划（Next Step）

Day02：GPIO 控制与 UART 通信（见 [day02.md](day02.md)）。
