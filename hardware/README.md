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
| F407 Master | PB8 | PB9 | RXD / TXD |
| F103 Sensor Node | PA11 | PA12 | RXD / TXD |
| F103 Motor Node | PA11 | PA12 | RXD / TXD |

## 电机接线（Motor Node）

```
F103 PA0 → L298N IN1 (PWM, TIM2_CH1)
F103 PA1 → L298N IN2 (方向)
L298N OUT1/OUT2 → 电机两根线
12V 适配器 → L298N 12V 输入
编码器 A 相 → PA6 (TIM3_CH1)
编码器 B 相 → PA7 (TIM3_CH2)
```

## 供电建议

- F407：USB 线供电
- F103：USB-TTL 模块供电
- L298N：独立 12V 电源（其 5V 输出不给 F407 使用，电流不足）
