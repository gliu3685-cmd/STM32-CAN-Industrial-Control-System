# CAN PID 交接 · 面试与项目要点

> 聚焦本项目的 **CAN 通信 + 电机 PID 速度闭环** 这一段，作为接手/复习/面试准备的一页式要点文档。
> 依据：GitHub 仓库 `gliu3685-cmd/STM32-CAN-Industrial-Control-System` @ main（`8f5ba2c`，Day40 Phase 6 收尾），
> 本地工作副本 `STM32-CAN-Industrial-Control-System` 与其同步；核心源码 `firmware/motor_node/Core/Src/app_motor.c`、`app_can.c`，
> 文档 `docs/day26-28`、`docs/can-walkthrough.md`、`docs/can-protocol.md`。
> 整理日期：2026-09-06。

---

## 0. 一句话概括

在“一主两从”的三节点 CAN 工业控制原型里，把 **电机节点（Node2）** 做成 `CAN 命令 → PWM 驱动 → 编码器测速 → 增量式 PID 闭环 → RPM 上报` 的独立闭环链路；**代码已实现并编译通过，闭环实测因编码器反馈噪声未解决而暂缓**。

---

## 1. 项目要点

### 1.1 系统链路与角色

```text
F407 主控 -> CAN 0x201(目标速度) -> Node2(F103) 电机节点
                                    -> PWM(L298N) -> 电机 -> 编码器反馈
                                    -> PID 闭环 -> 0x202(RPM) 回传主控
```

| 节点 | MCU | CAN 职责 |
|---|---|---|
| 主控 | STM32F407 | 心跳 0x100、命令 0x201、故障监控 |
| Node1(传感器) | STM32F103 | 温度 0x102 + MPU6050 0x103 |
| Node2(电机) | STM32F103 | 收 0x201 驱动电机、回 0x202 速度 |

### 1.2 硬件与接线（电机闭环相关）

- Node2：PA0 = L298N IN1(PWM)、PA1 = IN2(方向)、PA6/PA7 = 编码器 A/B、编码器 VCC=5V、GND=共地
- L298N：12V 适配器（内正外负/红正黑负）→ 12V 端子；**L298N GND 必须与 F103 共地**
- 编码器 A/B 用**内部上拉**（`GPIO_PULLUP`）——开集输出悬空会被 PWM 噪声误计
- 总线：3 个 TJA1050，CANH/CANL 并到一总线，CAN 分析仪提供终端电阻；CAN 2.0B 标准帧，**500kbps**

### 1.3 CAN 协议（电机相关）

| 帧 | 方向 | DLC | 内容 |
|---|---|---|---|
| 0x201 | 主控 → Node2 | 3 | data[0]=0x01；data[1..2]=目标速度(2B 小端, 0~1000) |
| 0x202 | Node2 → 主控 | 2 | RPM(2B 小端)，每 1s 上报 |

- 字节序一律小端；速度 0 = 停转，>0 = 正向驱动（占空比上限 800，即 80%）
- Node2 过滤：掩码模式 Mask=0x7FF、Filter=0x201，精确只收命令帧；ID 左移 5 位对齐 32 位过滤器寄存器
- 接收走 RX0 中断 → `xQueueSendFromISR` 投递 → `CanRxTask` 解析（ISR 最小化）

### 1.4 电机驱动与编码器测速

- TIM2_CH1(PA0) PWM，**2kHz**（PSC=35 → 2MHz，ARR=999），占空比 0~1000（0.1% 分辨率）
- 方向脚 PA1：`dir=1` → PA1 低（IN1 接 PWM，速度与 duty 成正比）；避免 IN2 恒高导致“duty 越大越刹车”
- TIM3 编码器模式 `TIM_ENCODERMODE_TI12`（A/B 正交 **4 倍频**），16 位 CNT，正转递增/反转递减
- 测速：`rpm = delta * 60000 / (dt_ms * ENC_COUNTS_PER_REV)`，用实际 dt 换算；取绝对值规避 16 位回绕
- 反馈平滑：`rpm_sm = (rpm_sm*3 + rpm) / 4`（EMA），对噪声做软件滤波

> 关键坑：F1 HAL 用 V1.8.7，`HAL_TIM_Encoder_Init` 第二参数要传 `TIM_Encoder_InitTypeDef` 结构体，不能直接传 `TIM_ENCODERMODE_TI12`。

### 1.5 PID 速度闭环

- 形式：**增量式**，控制周期 **20ms**（在 `CanTxTask` 内调用）
- 公式：`Δout = Kp*(e-e1) + Ki*e + Kd*(e-2e1+e2)`，`out += Δout`，再限幅
- 抗饱和：**积分分离**（`|e| > PID_INT_SEP_ERR` 时 Ki 项置 0）+ **输出限幅**（钳在 [0, MOTOR_DUTY_LIMIT]）
- 目标映射：协议 `g_speed(0~1000)` 近似 `/2` 得 RPM 目标（实测 500≈257RPM），按标定可调整
- `g_speed==0` 时直接 duty=0 停转

```c
uint16_t MotorPidCompute(uint16_t target_rpm, uint16_t rpm_fb)
{
    int32_t e  = (int32_t)target_rpm - (int32_t)rpm_fb;
    int32_t ea = (e < 0) ? -e : e;
    int32_t dout = PID_KP * (e - pid_err1)
                 + ((ea > PID_INT_SEP_ERR) ? 0 : PID_KI) * e
                 + PID_KD * (e - 2*pid_err1 + pid_err2);
    pid_out += dout;
    if (pid_out > MOTOR_DUTY_LIMIT) pid_out = MOTOR_DUTY_LIMIT;
    if (pid_out < 0)               pid_out = 0;
    pid_err2 = pid_err1; pid_err1 = e;
    return pid_out;
}
```

### 1.6 关键参数速查

| 参数 | 值 | 含义 |
|---|---|---|
| ENC_COUNTS_PER_REV | 1079 | 输出轴每转编码器计数（标定） |
| MOTOR_DUTY_MAX / LIMIT | 1000 / 800 | 占空比范围 / 上限 |
| PID_KP / KI / KD | 5 / 1 / 0 | 增量式增益（初值，待硬件微调） |
| PID_INT_SEP_ERR | 30 | 积分分离误差阈值 |
| PID 周期 | 20ms | 闭环控制周期 |
| PWM 频率 | 2kHz | 降低开关噪声 |
| CAN 波特率 | 500kbps | 标准帧 11 位 ID |

### 1.7 难点与坑

1. **F103 的 CAN_RX 必须配输入模式**（F1 无 AF 选择寄存器），照抄 F4 AF_PP 会卡初始化
2. **L298N 极性反转**：IN2 恒高时速度与 duty 成反比（0=全速、满占空=刹车），改为正向 IN2 低 + duty 钳位
3. **编码器 PWM 噪声污染**：电机转动时 20ms/50ms 读数 40-120 乱跳，停转 0 干净；上拉 + EMA + 2kHz 后仍振荡（硬件 RC/屏蔽/共地待处理）
4. **F1 HAL 错误中断会把 CAN State 锁死为 ERROR**，必须在回调恢复 READY，否则发送永远失败
5. **F103 的 can.c 是手写的**，不要用 CubeMX 重新生成覆盖

### 1.8 当前状态与待办

- 已完成：PID 代码 + 编译通过；编码器标定（PPR=1079）；0x202 上报 RPM；CAN 过滤/命令/闭环接入
- 待办：**PID 闭环实测与调参挂起**（反馈噪声未解决前调参无意义）→ 硬件 RC 滤波/分线/共地；随后按 Day28 流程先 P 后 I 再 D；三节点联调回归
- 约束：IAP Bootloader（Day29-34，代码+文档已完成）烧录验证须等 PID 阶段完成后进行（14ccd52 明确延后硬件调参）
- 收尾：Day35-40 完成重构与文档化（Day37 清理 ISR 内 printf / 临时诊断 / 死代码；Day38 项目叙事；Day40 面试问答），第四阶段（电机控制闭环）仍为“代码完成、实测未过”状态

---

## 2. 面试要点（Q&A）

### 2.1 CAN 通信

**Q：为什么 CAN 用掩码模式？为什么 ID 要左移 5 位？**
掩码可让一组 ID 按位匹配；`(收到ID & Mask) == (Filter & Mask)` 即接收。11 位 ID 在 32 位过滤器寄存器里右对齐到高位、低 5 位不参与比较，所以需 `<<5` 对齐；Mask=0x7FF 即 11 位全比较 = 精确匹配。

**Q：中断里收到帧为什么只入队不在中断里处理？**
中断会抢占所有任务，处理越久系统抖动越大；队列让中断只花几微秒投递（`xQueueSendFromISR`），解析/业务放 `CanRxTask`，任务可阻塞挂起不空转。

**Q：F1 和 F4 的 CAN 引脚配置为何不同？**
F4 有“复用功能选择寄存器”，RX/TX 都配 `AF_PP + AF9`；F1 无该寄存器，CAN_RX 本质是普通输入引脚，必须配输入模式+上拉。移植照抄是最常见坑。

**Q：怎么区分节点故障和总线故障？**
单节点离线 = 仅该节点超时、主控收发正常；总线/主控故障 = 主控发帧报 ACK 错误（LEC=3）、所有节点一起离线。靠错误计数与 5s 超时交叉定位。

**Q：CAN 帧类型与波特率、采样点？**
数据帧/遥控帧/错误帧/过载帧；标准帧 11 位、扩展帧 29 位。波特率由 1 位时间 TQ 和 SYNC+PROP+PHASE1+PHASE2 分段决定，采样点一般 70%~80%（本项目 F103 88.9%）。

### 2.2 编码器测速

**Q：M 法 / T 法 / M/T 法怎么选？**
M 法数脉冲，适合高速；T 法测周期，适合低速；M/T 混合兼顾。相邻读数取差值可规避 16 位计数器回绕；采样周期应 ≤ PID 周期；先保证反馈干净再谈控制。

**Q：编码器反馈有噪声怎么办？**
硬件：RC 滤波、屏蔽、布线远离 PWM、降低开关频率；软件：低通/EMA/滑动平均（代价是响应变慢）。本项目中上拉 + EMA + 2kHz 为软件/参数层缓解，硬件层仍待处理。

### 2.3 PID 闭环

**Q：位置式 vs 增量式 PID？**
位置式输出绝对量、需累加全部历史误差，易积分饱和；增量式只输出变化量 `Δout`，无累积误差，便于限幅与模式切换。本项目用增量式。

**Q：什么是积分饱和/积分分离？**
执行器饱和后误差持续积分，恢复时会严重超调；处理有积分限幅、积分分离（|误差| 大时关积分）、遇限消积分。本项目用积分分离 + 输出限幅。

**Q：调参顺序？**
先 P 找到临界震荡（等幅），Kp 减半留裕量；再加 I 消稳态误差；最后加 D 抑超调。一次只动一个参数、一个方向；电机惯量小，D 通常可保持 0。

**Q：输出等幅震荡说明什么？**
增益过大（通常 Kp 或 Ki），减小增益或检查死区/滞环。

**Q：PID 为什么先保证反馈干净？**
PID 只能控制“看到的转速”；反馈被噪声污染时误差抖动会让输出占空比剧烈波动，调参会失去意义。所以本次闭环实测被反馈噪声阻塞。

### 2.4 系统综合 / 延伸

**Q：这个项目难点在哪？怎么证明是你做的？**
难点：L298N 极性反转（H 桥逻辑分析定位）、F103 CAN_RX 输入模式、编码器 PWM 噪声（硬件滤波延后）。GitHub 有完整提交链 + 每日学习记录（Day1-40）+ 协议/架构文档 + 上位机工具。

**Q：还能怎么扩展？**
CAN IAP（升级走 CAN 总线）、双 Bank 升级、姿态闭环（0x103 接入主控）、增加更多节点、故障注入自检。

---

## 3. 数字速查表

| 项 | 值 |
|---|---|
| CAN 波特率 | 500 kbps，标准帧 11 位 ID |
| PWM | TIM2_CH1，2kHz，ARR=999，duty 0~1000 |
| 编码器 | TIM3，TI12 4 倍频，PPR=1079 |
| PID 周期 / 增益 | 20ms；Kp=5 Ki=1 Kd=0 |
| 命令/反馈帧 | 0x201 设速 / 0x202 RPM(1s) |
| 速度上限 | duty 上限 800（80%） |
| 目标映射 | 协议值 ≈ RPM × 2（500≈257RPM） |
| 反馈平滑 | EMA `(rst*3+r)/4` |

---

## 4. 相关文件索引

- 源码：`firmware/motor_node/Core/Src/app_motor.c`、`app_can.c`；`Core/Inc/app_motor.h`
- 文档：`docs/day26.md`（波形/开环阶跃）、`docs/day27.md`（增量式 PID）、`docs/day28.md`（调参+联调）、`docs/can-walkthrough.md`（CAN 五层详解）、`docs/can-protocol.md`（协议表）、`docs/HANDOFF.md`（整体交接）
- 全局：`docs/interview-notes.md`（全项目面试问答）、`docs/project-narrative.md`（简历/口述）
