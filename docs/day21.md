# Day21 - 请求/应答命令帧与压力测试（Phase 3 收官）

## 1. 今日目标（Objectives）

本日完成第三阶段 CAN 通信系统的收官实验，重点掌握：

* 请求/应答（Request/Response）模式与周期上报的对比与适用场景
* 应用层命令帧设计：命令码、参数、应答三要素
* 主从轮询模型：主控下发指令、从节点执行并回报
* 长时间压力测试方法：错误计数、误报检查、帧连续性验证
* Phase 3 收官：协议核对与遗留问题修复

---

## 2. 理论学习（Theory）

### 2.1 周期上报 vs 请求/应答

| | 周期上报 | 请求/应答 |
|---|---|---|
| 谁主动 | 从节点 | 主控 |
| 带宽占用 | 一直占用 | 需要时才占用 |
| 实时性 | 最坏等一个周期 | 指令随发随到 |
| 用途 | 状态监测（温度/心跳） | 控制命令（设速度/启停） |

本系统两者并用：温度走周期上报（监测），速度走命令帧（控制），这是工业系统中常见的"监测推送 + 控制拉取"混合模型。

### 2.2 命令帧设计三要素

* **命令码**：data[0] 表示"干什么"（本日 0x01 = 请求上报 / 设置速度）
* **参数**：data[1..n] 表示"参数是什么"（0x201 中 data[1..2] = 目标速度，小端）
* **应答**：从节点收到后立即回一帧确认（本日回 0x102 温度 / 0x202 速度）

命令帧与数据帧共用同一 ID 空间，靠 data[0] 的命令码区分具体动作，这是应用层协议的标准做法。

### 2.3 主从轮询模型

CAN 是多主总线，但应用层可以建立主从关系：主控周期轮询各从节点（发命令帧），从节点收到后应答。轮询模型让总线访问可控、便于超时管理，是电机控制等确定性场景的常用结构。

### 2.4 压力测试方法

长时间运行验证三项指标：

* 错误计数归零：LEC=0、TEC=0、REC=0
* 不误报：节点在线状态稳定，无虚假离线告警
* 不丢帧：各帧周期计数持续递增，命令帧按约定周期出现

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6 主控 + STM32F103C8T6 ×2（Node1 传感器 / Node2 电机）
* TJA1050 CAN 收发器 ×3、USB-CAN 分析仪、USB-TTL、面包板

软件：

* STM32CubeIDE 2.2.0、PCAN-View（500 kbit/s）

工程：`STM32-CAN-Industrial-Control-System`

当前阶段：第三阶段 CAN 通信系统（收官）

---

## 4. 代码实现

### 4.1 主控：命令帧发送（app_can.c / app_arch.c）

新增 `CanSendCommand()`，发送任务每 5s 交替向 Node1/Node2 下发命令：

```c
void CanSendCommand(uint8_t node)
{
    CAN_TxHeaderTypeDef tx = {0};
    uint8_t  data[8] = {0};
    uint32_t mailbox = 0;
    static uint16_t cmd_speed = 500;

    if (node == 0U)
    {
        tx.StdId = 0x101U;          /* 请求 Node1 上报温度 */
        tx.DLC   = 1U;
        data[0]  = 0x01U;           /* CMD: 请求上报 */
    }
    else
    {
        tx.StdId = 0x201U;          /* 设置 Node2 目标速度 */
        tx.DLC   = 3U;
        data[0]  = 0x01U;           /* CMD: 设置速度 */
        cmd_speed = (cmd_speed == 500U) ? 1000U : 500U;
        data[1]  = (uint8_t)(cmd_speed & 0xFFU);        /* 速度低字节 */
        data[2]  = (uint8_t)((cmd_speed >> 8) & 0xFFU); /* 速度高字节 */
    }
    HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox);
}
```

### 4.2 Node1：收到命令立即应答

```c
if ((frame.id == CAN_CMD_ID) && (frame.data[0] == 0x01U))
{
    NodePrint("[NODE1] CMD req-report -> respond\r\n");
    CanSendSensorData(g_temp);   /* 立即回一帧 0x102 */
}
```

### 4.3 Node2：收到命令更新速度并保持

```c
if ((frame.id == CAN_CMD_ID) && (frame.data[0] == 0x01U))
{
    g_speed = (uint16_t)(frame.data[1] | ((uint16_t)frame.data[2] << 8));
    g_override_until = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
    CanSendMotorData(g_speed);   /* 立即回一帧 0x202 */
}
```

发送任务中，命令速度保持 5s，超时后恢复自动递增演示。

---

## 5. 实验过程（Experiment）

### 实验 1：请求/应答回路验证

现象：

* 主控每 5s 交替发送 `[CAN_TX] cmd 0x101 req-report` 与 `[CAN_TX] cmd 0x201 set-speed=500/1000`
* Node1 收到 0x101 打印 `[NODE1] CMD req-report -> respond` 并立即回 0x102
* Node2 收到 0x201 打印 `[NODE2] CMD set-speed=500 -> respond`，0x202 速度保持 500/1000 五秒后恢复递增
* PCAN-View：101h/201h 命令帧各约每 10s 一帧，应答帧紧随其后

验证：命令帧 → 从节点执行 → 应答闭环打通。

### 实验 2：30 分钟压力测试

运行约 30 分钟（up=1536s），观察结果：

| 观测项 | 结果 |
|---|---|
| F407 节点状态 | node0=ON node1=ON 稳定，无虚假离线告警 |
| Node1 错误计数 | LEC=0 TEC=0 REC=0（State=1） |
| Node2 错误计数 | LEC=0 TEC=0 REC=0（State=1） |
| 100h 心跳帧 | 1715 帧（约 1 帧/s） |
| 102h 温度帧 | 2365 帧（周期 + 命令应答） |
| 202h 速度帧 | 2314 帧（周期 + 命令应答） |
| 101h/201h 命令帧 | 183 / 180 帧（各约每 10s 一次，与设计一致） |

验证：长时间运行错误计数保持 0、无误报、无丢帧，总线与协议稳定。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：DEBUG 显示 speed=0 而 Node2 实际速度为 500

问题：F407 的 `[DEBUG]` 行显示 `adc=500 speed=0`，但 Node2 串口显示 `speed=500`。

原因：协议约定 0x202 为 DLC=2、data[0..1]=速度，但主控解析仍沿用 Day14 模拟时代的 4 字节格式（adc 2 字节 + speed 2 字节），把速度读进了 adc 字段，speed 读的是不存在的 data[2..3]（恒 0）。

解决：按协议修正主控解析，0x202 只解析速度：

```c
gSys.speed = (uint16_t)(frame.data[0] | (frame.data[1] << 8));
```

修复后 `[DEBUG]` 显示 `speed=1000`，与 Node2 实际一致。

### Problem 2：节点已在线但 DEBUG 仍显示 fault=OFFLINE

问题：主控先于从节点启动，开机阶段判定过一次离线，离线标志置 1 后永远不消失。

原因：`fault_offline` 是锁存标志，无清除逻辑。

解决：故障监控任务在所有节点恢复在线后自动清除标志：

```c
else
{
    gSys.fault_offline = 0;   /* 全部恢复在线，清除离线标志 */
}
```

修复后健康状态显示 `fault=`（空）。

---

## 7. 今日成果（Result）

完成：

* [x] 请求/应答命令帧闭环（0x101 请求上报、0x201 设置速度）
* [x] 命令帧设计三要素掌握：命令码、参数、应答
* [x] 30 分钟压力测试通过：错误计数 0、无误报、无丢帧
* [x] 协议核对修复：0x202 速度解析错位
* [x] 离线标志锁存问题修复：恢复在线自动清除
* [x] Phase 3 CAN 通信系统收官

当前三节点 CAN 架构（Phase 3 最终形态）：

```text
                USB-CAN 分析仪（观测）
                         |
           CANH ─────────┼───────── CANL
                         |
      +---------+    +---------+    +---------+
      | F407 主控 |    | Node1   |    | Node2   |
      | 0x100 心跳 |    | 0x102 温度|    | 0x202 速度|
      | 0x101/201 命令|  | 收 0x101 |    | 收 0x201 |
      +---------+    +---------+    +---------+
```

---

## 8. 工程总结（Engineering Summary）

## 监测推送 + 控制拉取

温度用周期上报（持续监测）、速度用命令帧（按需控制），两种模式并存是工业系统的常态。分清"谁主动、给谁用"是协议设计的第一步。

## 协议文档与实现必须互相核对

压力测试暴露了 0x202 解析与协议文档不一致的问题——文档写 DLC=2 速度，代码还按 4 字节解析。收官阶段逐帧核对协议是防止"文档与代码漂移"的关键。

---

## 9. 面试问答（Interview Prep）

### Q1：周期上报和请求/应答怎么选？

答题要点：

* 持续监测、实时性要求低 → 周期上报（省主控资源）
* 按需控制、确定性要求高 → 请求/应答（带宽按需、指令即时）
* 工业系统常两者并用

### Q2：命令帧怎么设计？

答题要点：

* 命令码：区分动作类型
* 参数：携带执行所需数据
* 应答：从节点执行后回报，形成闭环
* 补充：可加序列号/校验/超时重试增强可靠性

### Q3：从节点不应答怎么办？

答题要点：

* 主控需设命令超时，超时重发 N 次
* 连续失败判定节点故障，配合心跳离线检测
* 这是请求/应答模式比周期上报多出的可靠性设计点

### Q4：怎么证明系统稳定？

答题要点：

* 长时间运行（如 30 分钟）错误计数器保持 0
* 无虚假离线告警
* 帧计数连续递增、周期符合设计

---

## 10. Git 提交

```bash
git add docs/day21.md 学习进度.md README.md firmware/
git commit -m "feat(day21): CAN request/response command frames + 30min stress test, Phase 3 complete"
```

---

## 11. 下一步计划（Next Step）

Day22：

* 江协 PID 课程 1-1 ~ 1-4：PID 原理、离散化、编码电机、SerialPlot 波形
* 电机硬件准备：L298N 驱动接线、编码器接口梳理
* Phase 3 复盘与 README 最终核对

为第四阶段电机 PID 闭环（Day23-28）建立基础。
