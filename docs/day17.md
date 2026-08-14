# Day17 - F103 CAN 排障与节点打通

## 1. 今日目标（Objectives）

本日进入第三阶段（CAN 通信系统）的排障与联调准备，重点掌握：

* 定位并解决 F103 CAN 初始化卡死（MSR.INAK 恒为 1）的根因
* 打通 Node1（0x102）与 Node2（0x202）的 CAN 收发
* 掌握 F1 与 F4 在 GPIO 复用配置上的差异

---

## 2. 理论学习（Theory）

### 2.1 F1 与 F4 的 CAN 引脚复用配置差异

F407（Cortex-M4）有独立的 GPIO AF 选择寄存器，CAN 引脚统一配置为 `GPIO_MODE_AF_PP` + 对应 AF（如 `GPIO_AF9_CAN1`），输入/输出路径由硬件自动连接。

F103（Cortex-M3）没有 AF 选择寄存器，复用功能由外设固定映射。**CAN_RX 是纯输入，必须配置为 `GPIO_MODE_INPUT`（输入模式，建议加内部上拉）**；CAN_TX 才是 `GPIO_MODE_AF_PP`。若把 CAN_RX 误配成 `AF_PP`（照抄 F4 写法），引脚电平不会被送进 CAN 内核，表现为初始化永远卡死。

### 2.2 CAN 退出初始化模式的条件

CAN 进入初始化模式（`INRQ=1` → `INAK=1`）后，退出初始化（`INRQ=0` → `INAK=0`）需要 CAN 内核在 RXD 引脚上**检测到连续 11 个隐性位**（总线空闲）。因此 RXD 引脚必须真实接收到总线电平：引脚映射错误、引脚悬空、收发器未供电都会导致 `INAK` 无法清零。

### 2.3 DBF 调试冻结位

`CAN_MCR.DBF`（Debug Freeze）置 1 时，调试器暂停内核会让 CAN 时钟冻结。调试器（ST-LINK）连接时可能置位该位，导致退不出初始化模式。工程中应在启动 CAN 前主动清除（`CAN1->MCR &= ~CAN_MCR_DBF`）。

### 2.4 ISR 中的 printf 危害

错误中断回调里直接 `printf` 会打断任务中的打印（两处同时操作 UART），造成串口输出交错、截断（如 `code=0x000` 被切断）。正确做法：ISR 只恢复状态，错误详情由任务周期读取 ESR 寄存器打印。

---

## 3. 实验环境（Environment）

硬件：

* STM32F103C8T6 最小系统板 ×2（Node1 传感器 / Node2 电机）
* TJA1050 CAN 收发器模块 ×3
* USB-CAN 分析仪（PCAN-View，兼容 PCAN）
* USB-TTL 串口模块（115200，3.3V 给 F103、5V 给 TJA1050）

软件：

* STM32CubeIDE 2.2.0 + STM32Cube FW_F1 V1.8.7
* PCAN-View（500 kbit/s）

工程：`STM32-CAN-Industrial-Control-System`

当前阶段：第三阶段 CAN 通信系统

---

## 4. PA11 电平诊断实现

定位“CAN 内核收不到隐性电平”的关键实验：把 PA11 临时配成 GPIO 输入读取电平，再恢复 CAN 复用。

```c
diag.Pin = GPIO_PIN_11;
diag.Mode = GPIO_MODE_INPUT;
diag.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &diag);
NodePrint("[NODE1] PA11 level=%u\r\n",
          (unsigned)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11));
```

实验结论：PA11 读到高电平（TJA1050 输出正常），但 CAN 仍卡初始化 → 证明问题不在收发器，而在 GPIO 复用配置（CAN_RX 被配成了输出模式）。

---

## 5. F103 CAN GPIO 修复

根因修复：CAN_RX 必须配输入模式。

```c
/* PA11=CAN1_RX：F103 的 CAN_RX 必须配成输入模式 */
GPIO_InitStruct.Pin = GPIO_PIN_11;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* PA12=CAN1_TX：复用推挽输出 */
GPIO_InitStruct.Pin = GPIO_PIN_12;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

同时完成三项加固：删除错误的 AFIO 重映射（F103 CAN1 默认就在 PA11/PA12）、启动前清除 DBF、`AutoBusOff=ENABLE` 自动恢复总线关闭。

---

## 6. 实验过程（Experiment）

### 实验 1：PA11 直接接 3.3V

现象：`CanStart` 后出现 `CAN_ERR 0x00000080`（TX_TERR0，发送真实发生）。

验证：CAN 内核、代码、时钟正常，问题锁定在 RXD 电平侧。

### 实验 2：接 TJA1050 后读 PA11 电平

现象：`PA11 level=1`（高电平），但 `INAK` 仍为 1、`HAL_CAN_Start` 超时。

验证：收发器输出正常，问题在 GPIO 配置——CAN_RX 配成 `AF_PP` 导致内核收不到电平。

### 实验 3：修复后 Node1/Node2 收发

现象：`CanStart f=0 s=0 n=0 State=2`；PCAN-View 收到 Node1 的 `102h` 帧（temp 递增）。

验证：F103 CAN 500k 收发打通。

---

## 7. 遇到的问题与解决（Problems & Solutions）

### Problem 1：CAN 初始化卡死（INAK 恒 1）

问题：`HAL_CAN_Start` 超时，`MSR.INAK=1`，两块 F103 同样现象。

原因：① 误调用 `__HAL_AFIO_REMAP_CAN1_1()` 把 CAN 挪到 PB8/PB9（RXD 悬空）；② 根因是 CAN_RX 误配 `GPIO_MODE_AF_PP`（F4 写法），F103 必须配 `GPIO_MODE_INPUT`。

解决：删除重映射；CAN_RX 配输入模式 + 内部上拉。

### Problem 2：串口输出交错截断

问题：`CAN_ERR code=0x000` 被截断、两行打印拼在一起。

原因：错误中断回调中直接 printf，与任务打印抢占 UART。

解决：ISR 只恢复 `State=READY`，错误详情由任务每 5 秒读取 ESR 打印。

### Problem 3：REC 错误计数上涨

问题：`REC` 从 48 涨到 128（Error Passive）。

原因：PCAN-View 的 Transmit 周期发送与 Node2 速率/时序不匹配，接收侧持续解码错误。

解决：停掉 Transmit 后 REC 停止增长；正常收发后计数自动递减。

---

## 8. 今日成果（Result）

完成：

* [x] 定位并修复 F103 CAN 初始化卡死根因（GPIO 复用模式）
* [x] Node1（0x102）CAN 收发成功，PCAN-View 可见
* [x] Node2（0x202）CAN 初始化与发送成功
* [x] 固件加固：清 DBF、ABOM、ISR 无打印、周期 ESR 诊断

当前第三阶段架构：

```
          PCAN-View (500 kbit/s)
                 |
        USB-CAN 分析仪
                 |
      CANH/CANL 总线（共地）
        /        |        \
   F407 Master  Node1(0x102)  Node2(0x202)
```

---

## 9. 工程总结（Engineering Summary）

## F1 与 F4 的 GPIO 复用差异是坑

F1 无 AF 选择寄存器，输入型复用（RX 类）必须配输入模式，照抄 F4 的 `AF_PP` 会导致外设收不到信号。这是本次两天排障的核心教训。

## 用观察实验代替测量工具

无万用表时，通过“PA11 接 3.3V 对比”“GPIO 读电平”“打印 PCLK1/SYSCLK/时间戳”等行为实验逐层排除，同样能精确定位硬件/软件问题。

---

## 10. 面试问答（Interview Prep）

### Q1：F103 与 F407 的 GPIO 复用配置有什么区别？

答题要点：

* F4 有 GPIOx_AFRL/AFRH 选择复用外设，输入输出统一配 AF_PP
* F1 无 AF 选择寄存器，复用映射固定：RX 类引脚配输入模式，TX 类引脚配复用推挽
* 配置错误会导致外设收不到引脚信号（本次 CAN_RX 卡初始化的根因）

### Q2：CAN 初始化时 INAK 位一直为 1 可能有哪些原因？

答题要点：

* RXD 引脚映射错误或悬空（检测不到连续 11 个隐性位）
* 收发器未供电/损坏导致 RXD 输出低
* 调试器置位 DBF 冻结 CAN 时钟
* GPIO 复用模式配置错误

### Q3：CAN 总线关闭（Bus-Off）如何恢复？

答题要点：

* TEC 达到 255 进入 Bus-Off，CAN 停止收发
* 软件恢复：重新进入初始化模式再退出，或置位 `ABOM` 由硬件自动恢复
* 排查根因（无 ACK、波特率不匹配、接线）避免反复 Bus-Off

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "fix(day17): F103 CAN init stuck on INAK - RX pin must be GPIO_MODE_INPUT, clear DBF, ABOM, ISR-safe logging"
```

---

## 12. 下一步计划（Next Step）

Day18：

* F407 协议映射：0x102→node0、0x202→node1
* F407 过滤器精确过滤（只收 0x102/0x202）
* 三节点双向通信验证与心跳/离线检测

为第三阶段验收（三节点 CAN 双向通信）建立基础。
