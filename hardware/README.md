# Hardware

硬件设计文档与接线说明。

## 组件清单

- STM32F407VET6 最小系统板（Master，×1）
- STM32F103C8T6 最小系统板（Sensor / Motor Node，×2）
- TJA1050 CAN 收发器模块（×3，三块板各一个）
- ST-Link V2 调试器（×1）
- USB-TTL CH340G 模块（×1）
- MPU6050 六轴姿态模块（×1，Sensor Node）
- 直流减速电机 + 霍尔编码器（×1 套，Motor Node）
- L298N 电机驱动模块（×1）
- 12V 2A 电源适配器（5.5×2.1mm 圆头，×1）
- USB-CAN 分析仪（可选）
- 杜邦线、面包板、120Ω 终端电阻、排针排母

## CAN 总线接线

```
所有 TJA1050 的 CAN_H 连在一起 → 总线 CAN_H
所有 TJA1050 的 CAN_L 连在一起 → 总线 CAN_L
总线最两端（Master 端和 Node2 端）各接一个 120Ω 终端电阻
```

## 引脚连接

| 板子 | CAN_RX | CAN_TX | 对应 TJA1050 |
|---|---|---|---|
| F407 Master | PA11 | PA12 | RXD / TXD |
| F103 Sensor Node | PA11 | PA12 | RXD / TXD |
| F103 Motor Node | PA11 | PA12 | RXD / TXD |

## 电机接线（Motor Node）

### L298N 与 F103（Node2）

| L298N 引脚 | 接到 | 说明 |
|---|---|---|
| 12V（VCC） | 12V 适配器 + | 电机电源 |
| GND | 12V 适配器 − **且与 F103 GND 共地** | 关键：必须共地 |
| 5V 输出 | 编码器 VCC（可选） | 板上稳压输出，勿给 F407 用 |
| IN1 | F103 PA0（PWM，TIM2_CH1） | 速度控制 |
| IN2 | F103 PA1（方向） | 高/低决定正反转 |
| ENA | 保持跳帽短接（5V） | 使能通道 A |
| OUT1/OUT2 | 电机两根电源线 | 接电机 |

### 编码器（6 线制，以实物标签为准）

| 编码器线 | 接到 | 说明 |
|---|---|---|
| VCC（红） | 5V（L298N 5V 输出或 USB-TTL 5V） | 霍尔编码器供电 |
| GND（黑） | 公共 GND | 必须共地 |
| A 相（绿） | F103 PA6（TIM3_CH1） | 测速信号 A |
| B 相（蓝/白） | F103 PA7（TIM3_CH2） | 测速信号 B |

### 供电关系

```text
12V 适配器 ──→ L298N 12V（电机动力）
F407 USB   ──→ F407 / 两块 F103（逻辑电源，维持现状）
L298N 5V   ──→ 编码器 VCC（可选）
所有 GND 连成同一网络：L298N GND、12V 适配器 GND、F103 GND、编码器 GND、TJA1050 GND
```

注意：电机电流走 L298N，不经 F103；F103 只输出 PWM/方向信号，不可直接驱动电机。

## 供电建议

- F407：USB 线供电
- F103：USB-TTL 模块供电
- L298N：独立 12V 电源（其 5V 输出不给 F407 使用，电流不足）
