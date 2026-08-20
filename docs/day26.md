# Day26 - SerialPlot/VOFA+ 波形与开环阶跃

## 1. 今日目标（Objectives）

本日建立 PID 调参前的数据基础，重点掌握：

* 用波形工具观察电机转速实时曲线（20Hz 采样）
* 开环阶跃响应的测量方法（上升时间、超调、稳定值）
* 为 Day27 PID 速度闭环提供被控对象特性

---

## 2. 理论学习（Theory）

### 2.1 采样率

编码器每秒计数换算 RPM 需要足够采样率：1s 一次看不到阶跃过程，本日改为 50ms（20Hz）采样，能分辨秒级以内的动态。50ms 差值换算每秒等效计数：`cps = delta * 20`。

### 2.2 开环阶跃

不给反馈、直接改变 PWM 占空比，观察电机转速从稳态 A 到稳态 B 的过渡：

* 上升时间：到达 63.2%（一阶近似）或 90% 的时间
* 超调：过渡中超过新稳态的幅度（直流电机+PWM 通常很小）
* 稳态值：新命令下稳定后的 RPM

这些数据直接决定 PID 初始参数（P 从稳态增益反推，I 与响应速度相关）。

---

## 3. 实验环境（Environment）

硬件：Node2（F103）+ L298N + 电机 + 编码器 + 12V，USB-TTL（115200）

软件：STM32CubeIDE 2.2.0、SerialPlot（或 VOFA+ FireWater 模式）

工程：`firmware/motor_node`

当前阶段：第四阶段 电机控制闭环

---

## 4. 代码实现

CanTxTask 改为 50ms 采样，串口输出纯数据行（CSV）：

```c
uint32_t cps = (uint32_t)delta * 20U;            /* 50ms 差值 -> 每秒等效计数 */
uint16_t rpm = (uint16_t)(cps * 60U / ENC_COUNTS_PER_REV);
NodePrint("%u,%u\r\n", (unsigned)rpm, (unsigned)g_speed);   /* rpm,target */
```

0x202 保持每 1s 上报一次（20 个 50ms 差值累加换算 RPM）。

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译验证（已完成）

现象：`Build Finished. 0 errors, 0 warnings`。

### 实验 2：开环阶跃波形（待实测）

预期现象：SerialPlot 两通道曲线（rpm 黄、target 蓝）：

* 空载：rpm=0 平线
* 发 500：rpm 快速上升并稳定在 ~286
* 发 800：rpm 稳定在 ~547
* 发 0：rpm 下降回 0

记录：上升时间 / 超调 / 稳定值（待填）。

状态：⏳ 待实测。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：串口日志与波形数据混行

问题：命令打印（`[NODE2] CMD ...`）会混入纯数据流。

解决：波形行用无前缀 CSV；SerialPlot 自动跳过非数字行；DBG 周期打印已移除，命令行仅出现在发命令瞬间。

---

## 7. 今日成果（Result）

完成：

* [x] 50ms 采样 + CSV 波形输出（编译通过）
* [ ] 开环阶跃实测（待烧录）

---

## 8. 工程总结（Engineering Summary）

## 看得见的动态

控制的前提是能测量：波形工具把“转速变化”变成可读曲线，PID 调参从拍脑袋变成看数据。

---

## 9. 面试问答（Interview Prep）

### Q1：怎么测电机开环阶跃响应？

答题要点：

* 固定输入（PWM 阶跃），记录转速曲线
* 提取上升时间、超调、稳态值
* 用于确定 PID 初始参数与被控对象模型

---

## 10. 下一步计划（Next Step）

Day27：PID 速度闭环（增量式 + 积分限幅/分离）

* 以本日阶跃数据确定 P/I 初值
* 20ms 控制周期输出 PWM
* SerialPlot 观察收敛过程
