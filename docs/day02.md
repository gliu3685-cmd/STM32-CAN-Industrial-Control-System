# Day02 - GPIO 控制与 UART 通信

## 1. 今日目标（Objectives）

学习 STM32 HAL 库基础外设开发，建立串口调试能力。

今日完成：

* GPIO 控制（LED）
* UART 通信（USART1）
* printf 重定向到串口

---

## 2. 理论学习（Theory）

### 2.1 GPIO：输出模式

GPIO（通用输入输出）可配置为输入/输出/复用等功能。本日用**推挽输出**
控制 LED：输出高电平点亮、低电平熄灭。

```c
HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);    /* 输出高 */
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);                 /* 翻转 */
```

### 2.2 UART：异步串行通信

UART 是异步串行通信：一条 TX、一条 RX，无时钟线，靠约定**波特率**
（每秒传输的比特数）对齐时序。一个字符帧通常由起始位、数据位、
停止位组成，本工程配置为 **115200-8N1**（115200 波特率、8 数据位、
无校验、1 停止位）。

USART1 引脚：PA9 = TX，PA10 = RX（复用功能 AF7）。

### 2.3 printf 重定向原理

C 库的 `printf` 最终会调用底层写函数，嵌入式里把它重定向到串口：

```
printf("...")
    ↓
_write()（C 库底层）
    ↓
__io_putchar(ch)
    ↓
HAL_UART_Transmit(&huart1, ...)   ← 一个字节一个字节发
    ↓
USART1 → PA9 TX → PC 串口助手
```

这样所有 `printf` 输出都自动走串口，调试极其方便。

---

## 3. 实验环境（Environment）

硬件：

* STM32F407VET6
* ST-Link V2
* USB-TTL（CH340G）

软件：

* STM32CubeMX / STM32CubeIDE
* HAL Library

工程：`f407_blink`（主工程，与仓库 master_node 同步）

当前阶段：Phase 1 STM32 基础

---

## 4. GPIO 实现

使用 HAL 库控制 LED 闪烁：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
HAL_Delay(500);
```

实现：LED 周期闪烁。

---

## 5. UART 与 printf 实现

USART1 参数：

| 参数 | 值 |
|---|---|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |

printf 重定向（usart.c）：

```c
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

测试输出：

```c
printf("STM32 Start\r\n");
```

---

## 6. 实验过程（Experiment）

现象：

* LED 按设定周期闪烁；
* PC 串口助手（115200, 8N1）收到 `STM32 Start`。

验证：

* GPIO 推挽输出正常；
* USART1 收发链路正常；
* printf 已成功重定向到串口。

---

## 7. 遇到的问题与解决（Problems & Solutions）

本日未遇到阻塞性问题。

注意点：GPIO 配置为 UART 复用功能时，必须在 CubeMX 里选择正确的
Alternate 功能（USART1 对应 AF7），且 TX/RX 引脚不要接反，否则串口
无输出或乱码（Day03 详述排查过程）。

---

## 8. 今日成果（Result）

完成：

* [x] GPIO 控制 LED 闪烁
* [x] USART1 串口通信（115200, 8N1）
* [x] printf 重定向到串口

---

## 9. 工程总结（Engineering Summary）

本日学习重点：

## 调试手段先于业务开发

串口 printf 是嵌入式开发最重要的调试手段之一。先把"日志输出"这个管道
打通，后续所有实验（RTOS 任务、CAN 通信）都能用它观察内部状态。

## 底层机制要理解

printf 重定向不是魔法：`printf → _write → __io_putchar → HAL_UART_Transmit`。
面试中能画出这条链，说明真的理解。

---

## 10. 面试问答（Interview Prep）

### Q1：GPIO 推挽输出和开漏输出有什么区别？

答题要点：

* 推挽：能主动输出高低电平，驱动能力强，常用；
* 开漏：只能拉低或释放（高阻），需要外部上拉，常用于 I2C 或电平转换。

### Q2：printf 是怎么重定向到串口的？

答题要点：

* printf → C 库底层写函数（_write）→ 自定义 `__io_putchar` →
  `HAL_UART_Transmit` 逐字节发送；
* 关键：实现底层写函数，把"写到哪里"从屏幕改成串口。

### Q3：UART 的 115200-8N1 是什么意思？

答题要点：

* 115200 = 波特率（每秒比特数）；
* 8 = 8 个数据位，N = 无校验，1 = 1 个停止位；
* 收发双方必须完全一致才能正确解析数据。

---

## 11. Git 提交

建议提交：

```bash
git add .
git commit -m "feat: Day02 GPIO LED and UART printf redirect"
```

---

## 12. 下一步计划（Next Step）

Day03：UART 调试系统（见 [day03.md](day03.md)）。
