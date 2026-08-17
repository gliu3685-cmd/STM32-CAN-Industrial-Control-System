# HANDOFF - 新线程交接文档（2026-08-17，Day 24）

> 用途：当本会话上下文过长时，在新 Codex 线程的第一条消息里粘贴：
> `先读仓库 docs/HANDOFF.md，然后继续 Day 24 上电验证 / Day 25 标定。`
> 新线程无需旧对话历史，所有关键状态都在这个文件 + 仓库里。

---

## 1. 项目是什么

基于 STM32 + FreeRTOS + CAN 总线的三节点工业控制系统原型（40 天学习项目）：

```
传感器采集 -> CAN 通信 -> 主控决策 -> CAN 指令 -> 电机执行 -> 编码器反馈 -> PID 闭环
```

- 仓库：`STM32-CAN-Industrial-Control-System`
- 本地副本：`C:\Users\zwq66\Documents\柳\STM32-CAN-Industrial-Control-System`（唯一工作副本）
- 远程：`https://github.com/gliu3685-cmd/STM32-CAN-Industrial-Control-System`
- 当前阶段：**Phase 4 电机控制闭环**（Day 24 代码完成，等待 12V 电源线验证）

## 2. 当前状态

**阻塞点**：12V 电源线未到货，Node2 无法上电验证电机。其余全部就绪：

- Node1 MPU6050 软件 I2C 驱动 + 0x103 加速度帧：已烧录验证成功（`MPU6050 init ok`，三轴模长 ≈ 1g）
- Node2 电机控制代码全部就绪：TIM2 PWM（PA0, 9kHz）+ TIM3 编码器模式（PA6/PA7, 4 倍频）+ 方向脚 PA1，默认停转
- F407 主控无需改动（已在周期发 0x201）
- 工作区干净，main 分支与 origin/main 同步

**最近提交链**（均已推送）：

```
99b9556 (Day22) -> ce801bb (Day23 Node2 TIM) -> 44cccff (MPU6050) -> 15306df (MPU6050 验证) -> 5e6e68a (Day24)
```

## 3. 三节点角色

| 节点 | 硬件 | 职责 | 串口提示 |
|---|---|---|---|
| Master | STM32F407VET6 | 心跳 0x100、命令 0x101/0x201、故障监控 | `F407 Master Start!` |
| Node1 (sensor) | STM32F103C8T6 | 温度 0x102 + MPU6050 加速度 0x103 | `F103 Node1 Sensor Start!` |
| Node2 (motor) | STM32F103C8T6 | 接收 0x201 命令驱动 L298N 电机，0x202 上报编码器速度 | `F103 Node2 Motor Start!` |

## 4. CAN 协议（500 kbps，11 位标准帧）

| 帧 ID | 方向 | DLC | 内容 |
|---|---|---|---|
| 0x100 | F407 -> 总线 | 8 | 心跳：seq(2B 小端) + 0x55AA + 预留，1s |
| 0x101 | F407 -> Node1 | 1 | data[0]=0x01 请求上报温度 |
| 0x102 | Node1 -> F407 | 2 | temp(2B 小端)，1s |
| 0x103 | Node1 -> 总线 | 6 | MPU6050 ax/ay/az 各 2B 小端，1s（主控暂未启用过滤） |
| 0x201 | F407 -> Node2 | 3 | data[0]=0x01，data[1..2]=目标速度(2B 小端, 0~1000) |
| 0x202 | Node2 -> F407 | 2 | 真实编码器每秒计数(2B 小端，0xFFFF 钳位)，1s |

## 5. 接线现状（全系统已验证可通信）

- F407 USB 供电；F407 5V -> 3 个 TJA1050 VCC；F407 3.3V -> 两个 F103；**所有 GND 共地面包板同一行**
- 3 个 TJA1050 的 CANH 拧/插一起、CANL 一起 -> 面包板 -> USB-CAN 分析仪（终端电阻用分析仪自带 1 个即可）
- Node2：PA0 -> L298N IN1(PWM)、PA1 -> IN2(方向)、OUT1/OUT2 -> 电机粗线、编码器 A -> PA6、B -> PA7、VCC -> 5V、GND -> 共地
- L298N：12V 适配器（**内正外负 / 红正黑负**）-> L298N 12V 端子；**L298N GND 必须与 F103 GND 共地**
- Node1：MPU6050 VCC -> 3.3V、GND -> 共地、SCL -> PB6、SDA -> PB7、AD0 -> GND

## 6. 代码位置

- 本地工程：
  - Master：`C:\Users\zwq66\Documents\f407_blink`
  - Node1：`C:\Users\zwq66\Documents\f103_blink_node1`
  - Node2：`C:\Users\zwq66\Documents\f103_blink_node2`
- 仓库副本：`firmware/{master_node,sensor_node,motor_node}`（与本地已同步）
- 关键文件：Node2 `app_motor.c`（驱动）/ `app_can.c`（命令与上报）；Node1 `app_mpu6050.c` / `app_can.c`

## 7. 关键技术约束（容易踩坑，务必先读）

1. **F103 的 can.c 是手写的**（含 Day17 的 CAN_RX 输入模式修复），不要用 CubeMX 重新生成，会覆盖
2. F1 HAL 用 **V1.8.7**：`HAL_TIM_Encoder_Init` 第二参数要 `TIM_Encoder_InitTypeDef` 结构体，不能直接传 `TIM_ENCODERMODE_TI12`
3. Node2 本地工程留有临时诊断代码（PA11 level、CanStart、DBG 打印），功能正常，可后续清理
4. Git 推送直连常被重置，**必须走代理**：
   `git -c http.proxy=http://127.0.0.1:7897 push origin main`
5. 写代码默认 ponytail 模式（最简方案，不过度抽象）

## 8. 下一步计划（电源线到货后）

1. **Day 24 收官验证**：Node2 烧录 -> 12V 上电（先确认极性）-> 电机默认不转；PCAN-View 发 0x201（data[0]=01, data[1..2]=速度小端）后电机转，Node2 打印 `CMD set-speed=xxx`；0x202 转时递增、停时归零
2. **Day 25**：编码器每转计数标定 + RPM 换算（杂牌电机齿轮比/码盘线数未知，必须实测）
3. **Day 26**：SerialPlot / VOFA+ 波形 + 开环阶跃
4. **Day 27**：PID 速度闭环（增量式 + 积分限幅/分离）
5. **Day 28**：调参 + 三节点联调

## 9. 可选事项（优先级低于电机 PID）

- MPU6050 数据目前只是 0x103 广播，主控过滤器未收，后续可决定是否接入姿态控制
- Node2 临时诊断代码清理

## 10. 常用文档索引

- 每日记录：`docs/day01.md` ~ `docs/day24.md`
- CAN 代码五层详解 + 面试问答：`docs/can-walkthrough.md`
- Phase 3 复盘：`docs/phase3-review.md`
- 总 README（协议表/进度/学习记录索引）：根目录 `README.md`
- 学习进度跟踪：`学习进度.md`
