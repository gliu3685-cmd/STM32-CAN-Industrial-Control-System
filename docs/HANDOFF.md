# HANDOFF - 新线程交接文档

> 更新：2026-09-30（原版停在 Day 24，已过时，本次重写）
> 用法：新 Codex 线程第一条消息粘贴：`先读仓库 docs/HANDOFF.md，然后继续。`
> 新线程无需旧对话历史，关键状态都在这个文件 + 仓库里。

---

## 1. 项目是什么

基于 STM32 + FreeRTOS + CAN 总线的三节点工业控制系统原型（40 天学习项目，Day1-40 已完成）。

```text
传感器采集 -> CAN 通信 -> 主控决策 -> CAN 指令 -> 电机执行 -> 编码器反馈 -> PID 闭环
```

- 仓库：`STM32-CAN-Industrial-Control-System`
- 本地副本：`C:\Users\zwq66\Documents\柳\STM32-CAN-Industrial-Control-System`（唯一工作副本）
- 远程：`git@github.com:gliu3685-cmd/STM32-CAN-Industrial-Control-System.git`（public）
- `main` 与 origin 同步

---

## 2. 当前状态

| 阶段 | 状态 |
|---|---|
| Phase 1 基础（Day1-4） | 完成 |
| Phase 2 FreeRTOS（Day5-14） | 完成 |
| Phase 3 CAN 三节点（Day15-21） | 完成（30 分钟压力测试错误计数 0） |
| Phase 4 电机闭环（Day22-28） | 代码就绪，实测延后 |
| Phase 5 IAP Bootloader（Day29-34） | 代码 + 文档完成，上电验证待执行 |
| Phase 6 项目整理（Day35-40） | 基本完成 |

**Phase 4 的阻塞点**：编码器在电机转动时被 PWM 噪声污染（停转读数 0 干净、转动时 40-120 乱跳）。已做内部上拉 + EMA 平滑 + PWM 降到 2kHz，判断仍需硬件滤波（RC/屏蔽/共地）。

**下一步主线：CAN OTA** —— 把 Bootloader 合并进三节点项目，实现「PC → 主控 → CAN → 节点」的远程升级。设计已定稿：`docs/can-ota-design.md`（未实现）。

---

## 3. 三节点角色

| 节点 | 硬件 | 职责 | 串口提示 |
|---|---|---|---|
| Master | STM32F407VET6 | 心跳 0x100、命令 0x101/0x201、故障监控 | `F407 App v1.0 Start!` |
| Node1 (sensor) | STM32F103C8T6 | 温度 0x102 + MPU6050 加速度 0x103 | `F103 Node1 Sensor Start!` |
| Node2 (motor) | STM32F103C8T6 | 收 0x201 驱动 L298N，0x202 上报速度 | `F103 Node2 Motor Start!` |

---

## 4. CAN 协议（速查）

| 帧 ID | 方向 | DLC | 内容 | 周期 |
|---|---|---|---|---|
| 0x100 | F407 → 总线 | 8 | 心跳：seq(2B 小端) + 0x55AA | 1s |
| 0x101 | F407 → Node1 | 1 | data[0]=0x01 请求上报温度 | 按需 |
| 0x102 | Node1 → F407 | 2 | temp(2B 小端) | 1s |
| 0x103 | Node1 → 总线 | 6 | MPU6050 ax/ay/az 各 2B 小端（主控未启用） | 1s |
| 0x201 | F407 → Node2 | 3 | data[0]=0x01，data[1..2]=目标速度(2B 小端) | 按需 |
| 0x202 | Node2 → F407 | 2 | RPM(2B 小端) | 1s |

完整协议见 `docs/can-protocol.md`。

---

## 5. 关键技术约束（容易踩坑）

1. **F103 的 `can.c` 是手写的**（含 Day17 的 CAN_RX 输入模式修复），不要用 CubeMX 重新生成，会覆盖。
2. **F1 HAL 用 V1.8.7**：`HAL_TIM_Encoder_Init` 第二参数要 `TIM_Encoder_InitTypeDef` 结构体，不能直接传宏。
3. **F103 的 CAN_RX（PA11）必须配 `GPIO_MODE_INPUT`**（F1 无复用功能选择寄存器）；**切勿调用 `__HAL_AFIO_REMAP_CAN1_1()`**（会把 CAN 挪到 PB8/PB9）。
4. **F1 HAL 错误中断会把 State 锁死为 ERROR**，必须在 `ErrorCallback` 里恢复 READY，否则发送永远失败。
5. **ISR 里禁止 `printf`**（会与任务打印抢占 UART），错误详情由任务周期读 ESR 打印。
6. 写代码默认 **ponytail 模式**（最简方案，不过度抽象）。
7. **推送**：SSH 直连可用；HTTPS 直连常被重置，必要时 `git -c http.proxy=http://127.0.0.1:7897 push origin main`。

---

## 6. 代码与文档位置

- 三个节点工程 + Bootloader：`firmware/{master_node,sensor_node,motor_node,bootloader}`
- 归档（不参与主工程）：`archive/{freertos-practice,keil-attempt}`
- 上位机工具：`tools/{upgrade_tool.py,proto_smoke.py}`
- 文档：`docs/`（完整索引见 `docs/README.md`）

---

## 7. 下一步

1. **实现 CAN OTA**（按 `docs/can-ota-design.md`）；
2. 解决编码器噪声，完成 **PID 实测调参**；
3. **Bootloader 上电验证**（注意：仓库里的主控副本 `main.c` 设了 `SCB->VTOR = 0x08010000`，本地日常联调工程维持 `0x08000000`，别混）。

---

## 8. 常用文档

- 完整文档索引：`docs/README.md`
- 学习进度：`学习进度.md`
- 协议：`docs/can-protocol.md`；架构：`docs/system-architecture.md`
- 代码全解析：`docs/code-walkthrough-full.md`
- CAN OTA 设计：`docs/can-ota-design.md`
