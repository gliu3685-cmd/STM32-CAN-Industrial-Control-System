# Day18 - 三节点联调：CAN 应用层协议映射与节点在线检测

## 1. 今日目标（Objectives）

本日完成第三阶段 CAN 通信系统的核心联调，重点掌握：

* CAN 标识符过滤器：掩码模式与列表模式的区别，多 Bank 精确过滤
* CAN 应用层协议：总线帧 ID → 逻辑节点号的映射方法
* 节点在线/离线检测：基于"数据新鲜度"的心跳超时机制
* 回顾 Day16-18 三天排障：F1/F4 GPIO 差异、终端电阻、面包板接线

---

## 2. 理论学习（Theory）

### 2.1 标识符过滤器：掩码模式 vs 列表模式

bxCAN 过滤器的两种工作模式：

* **掩码模式（Mask）**：指定一组"必须匹配"的位，掩码位为 1 的位必须与 ID 一致，为 0 的位不关心。适合过滤"一段 ID 范围"。
* **列表模式（List）**：每个寄存器精确存放一个要接收的 ID，只匹配完全相同者。适合过滤"几个固定 ID"。

STM32F407 有 28 个过滤器 Bank，可配置多个 Bank 各自精确匹配一个 ID。本日使用两个 Bank（Mask=0x7FF 全比较），分别精确接收 `0x102` 与 `0x202`。

### 2.2 应用层协议映射：帧 ID → 节点号

总线上流通的是"帧 ID"（0x102/0x202），而应用层需要的是"逻辑节点号"（node0/node1）。映射是协议分层的关键一步：

```text
总线帧 ID（0x102/0x202）──映射──> 节点号（0/1）──> 系统状态表
```

映射前 `frame.node_id = rx.StdId` 直接把 0x102（十进制 258）当节点号，超出节点表范围导致 `node_online` 永不更新——这是此前 `node0=OFF node1=OFF` 的直接原因。

### 2.3 心跳与离线检测：数据新鲜度

"节点在线"的本质是"数据新鲜"：主控记录每个节点最近一次收到数据的时间戳（`last_rx_tick`），周期检查 `now - last_rx_tick` 是否超过阈值（协议约定 5s）。超过则判离线；节点重新上报时自动恢复在线。

### 2.4 三天排障的知识点回顾

* **F1 与 F4 GPIO 复用差异**：F1 无 AF 选择寄存器，输入型复用（CAN_RX）必须配 `GPIO_MODE_INPUT`；照抄 F4 的 `AF_PP` 会导致外设收不到信号
* **CAN 退出初始化模式**：`INAK` 归零需要 RXD 检测到连续 11 个隐性位；引脚映射错误、RXD 悬空、`DBF` 冻结都会卡死
* **终端电阻**：高速 CAN 总线两端各一个 120Ω，防反射 + 拉回隐性；短距离实验单终端可工作
* **面包板结构**：中间凹槽两侧插孔不连通，"行"的方向必须认准

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6 主控 + STM32F103C8T6 ×2（Node1 传感器 / Node2 电机）
* TJA1050 CAN 收发器 ×3、USB-CAN 分析仪、USB-TTL、面包板
* F407 USB 供电：5V → TJA1050×3，3.3V → F103×2

软件：

* STM32CubeIDE 2.2.0、PCAN-View（500 kbit/s）

工程：`STM32-CAN-Industrial-Control-System`

当前阶段：第三阶段 CAN 通信系统

---

## 4. F407 精确过滤实现（双 Bank）

```c
/* Bank0：精确接收 0x102（Node1 传感器数据） */
filter.FilterIdHigh     = (uint16_t)(0x102U << 5);
filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);
filter.FilterBank       = 0U;
HAL_CAN_ConfigFilter(&hcan1, &filter);

/* Bank1：精确接收 0x202（Node2 电机状态） */
filter.FilterIdHigh = (uint16_t)(0x202U << 5);
filter.FilterBank   = 1U;
HAL_CAN_ConfigFilter(&hcan1, &filter);
```

---

## 5. ID → 节点号映射实现

```c
/* CAN RX0 中断回调中：帧 ID → 节点号（0=Node1 传感器，1=Node2 电机） */
frame.node_id = (rx.StdId == 0x202U) ? 1U : 0U;
```

系统状态表（`gSys`）由互斥锁保护，`CanRxTask` 更新 `last_rx_tick / node_online / temp / speed`，`FaultMonitorTask` 按心跳周期检查超时。

---

## 6. 实验过程（Experiment）

### 实验 1：三节点总线打通

现象：PCAN-View 同时收到 `100h`（F407 心跳）、`102h`（Node1）、`202h`（Node2）三路帧。

### 实验 2：F407 协议映射与在线检测

现象：F407 串口 `[DEBUG]` 中 `node0=ON node1=ON`；拔掉某节点后 5s 内打印 `[FAULT] nodeX offline detected!`。

---

## 7. 三天排障总结（Day16 - Day18）

### Problem 1：F103 CAN 初始化卡死（INAK=1）

问题：`HAL_CAN_Start` 超时，`MSR.INAK` 恒 1，两块 F103 同样现象。

根因（三层，每层都伪装成硬件问题）：

1. 误调用 `__HAL_AFIO_REMAP_CAN1_1()`：F103 CAN1 默认在 PA11/PA12，该宏反而把 CAN 挪到 PB8/PB9
2. **最终根因**：CAN_RX（PA11）照抄 F407 配成 `GPIO_MODE_AF_PP`，F1 必须配 `GPIO_MODE_INPUT`
3. ST-LINK Debug 模式置位 `DBF` 调试冻结位，干扰判断

解决：删除重映射 + PA11 配输入模式 + 上拉 + 启动清 DBF。

### Problem 2：串口输出交错截断

问题：`CAN_ERR code=0x000` 被切断、两行打印拼在一起。

根因：错误中断回调（ISR）直接 printf，与任务打印抢占 UART。

解决：ISR 只恢复 `State=READY`，错误详情由任务每 5s 读取 ESR 打印。

### Problem 3：三节点总线不通

问题：三个节点各自单独 + 分析仪都通，合起来 PCAN-View 全空。

根因：**面包板行/列方向理解错误**——总线线看似插在同一"行"，实际因面包板方向反了而未连通。

解决：修正面包板方向后三路帧全部收到。

### 方法论沉淀

1. 先核对最小事实（引脚映射、GPIO 模式、面包板结构），再怀疑硬件
2. 一个根因结论至少两次独立验证（DBF 误判教训）
3. 逐节点隔离实验（单独+分析仪）比全链路排查快

---

## 8. 今日成果（Result）

完成：

* [x] 三节点 CAN 总线打通（100h/102h/202h 全通）
* [x] F407 双 Bank 精确过滤（只收 0x102/0x202）
* [x] 帧 ID → 节点号映射，`node0=ON node1=ON`
* [x] 心跳超时离线检测（5s，状态翻转才告警）

当前第三阶段架构：

```
          CAN Bus (500 kbps)
               │
   F407 Master  Node1(0x102)  Node2(0x202)
   协议映射+过滤     传感器        电机
   心跳/离线检测
```

---

## 9. 工程总结（Engineering Summary）

## 协议分层：ID 是传输层概念，节点号是应用层概念

总线上只有帧 ID，应用层必须显式做映射，否则数据进不了系统状态表。

## 排障成本最低的动作：最小化对照实验

"每节点单独 + 分析仪"一次实验就把问题从"三块板+总线"收敛到"面包板"，这是三天排障里最高效的一步。

---

## 10. 面试问答（Interview Prep）

### Q1：CAN 过滤器掩码模式和列表模式的区别？

答题要点：

* 掩码：按位匹配，掩码 1 的位必须一致，适合 ID 范围
* 列表：精确匹配固定 ID，适合少量固定 ID
* F407 多 Bank 可组合使用

### Q2：怎么判断 CAN 节点在线/离线？

答题要点：

* 基于数据新鲜度：记录最近收帧时间，超时判离线
* 节点周期性上报（心跳），主控只被动监听，不主动查询
* 恢复：节点重新上报即自动在线

### Q3：F103 和 F407 的 GPIO 复用配置为什么不同？

答题要点：

* F4 有 AF 选择寄存器，输入输出统一配 AF_PP + AF 号
* F1 无 AF 选择寄存器，RX 类输入必须配输入模式
* 配置错误的表现：外设收不到信号（本排障的核心教训）

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "feat(day18): F407 protocol mapping + dual-bank filter + node online/offline detection, 3-node CAN bus verified"
```

---

## 12. 下一步计划（Next Step）

Day19：

* Node 心跳帧协议完善（0x102/0x202 周期上报 + 数据格式文档化）
* 故障注入测试：拔线/断电观察离线与恢复
* 准备电机控制阶段（江协 PID 课程 1-1 ~ 1-4）

为第三阶段验收（三节点双向通信 + 离线检测）冲刺。
