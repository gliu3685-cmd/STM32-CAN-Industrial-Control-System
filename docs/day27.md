# Day27 - PID 速度闭环（增量式 + 积分分离 + 限幅）

## 1. 今日目标（Objectives）

本日实现电机速度闭环，重点掌握：

* 增量式 PID 的离散化与实现
* 积分分离与输出限幅（防饱和）
* 20ms 控制周期，转速反馈来自编码器（EMA 平滑）

---

## 2. 理论学习（Theory）

### 2.1 增量式 PID

位置式输出 → 输出增量：

```text
Δout = Kp(e - e1) + Ki·e + Kd(e - 2e1 + e2)
out += Δout
```

增量式只算变化量，无需累加历史误差，天然抗积分漂移，也便于限幅。

### 2.2 积分分离与限幅

* 积分分离：|误差| 过大时 Ki 项置 0，避免启动/大阶跃积分饱和（windup）
* 输出限幅：out 钳在 [0, MOTOR_DUTY_LIMIT]，电机只能正向驱动

### 2.3 目标速度映射

协议 g_speed(0~1000) 是占空比抽象值，PID 需要 RPM 目标：实测 500≈257RPM，故
`target_rpm ≈ g_speed / 2`（近似，调参时可按标定修正）。

---

## 3. 实验环境（Environment）

硬件：Node2（F103）+ L298N + 电机 + 编码器 + 12V，USB-TTL（115200）

软件：STM32CubeIDE 2.2.0、SerialPlot

工程：`firmware/motor_node`

当前阶段：第四阶段 电机控制闭环

---

## 4. 代码实现

`app_motor.c` 新增增量式 PID（积分分离 + 输出限幅）：

```c
uint16_t MotorPidCompute(uint16_t target_rpm, uint16_t rpm_fb)
{
    int32_t e  = (int32_t)target_rpm - (int32_t)rpm_fb;
    int32_t ea = (e < 0) ? -e : e;
    int32_t dout = PID_KP * (e - pid_err1)
                 + ((ea > PID_INT_SEP_ERR) ? 0 : PID_KI) * e
                 + PID_KD * (e - 2 * pid_err1 + pid_err2);
    pid_out += dout;
    if (pid_out > (int32_t)MOTOR_DUTY_LIMIT) pid_out = MOTOR_DUTY_LIMIT;
    if (pid_out < 0) pid_out = 0;
    pid_err2 = pid_err1;
    pid_err1 = e;
    return (uint16_t)pid_out;
}
```

CanTxTask 每 20ms：读编码器 → 换算 RPM（实际 dt）→ EMA 平滑 → PID 输出占空比。

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译验证（已完成）

现象：`Build Finished. 0 errors, 0 warnings`。

### 实验 2：PID 闭环实测（待烧录）

预期现象：

* 发 500：rpm 从 0 上升并收敛到 ~250（目标=500/2）
* 改变目标（800）：rpm 跟踪到 ~400
* 发 0：停转

记录：上升时间 / 超调 / 稳态误差 / 是否震荡（待填）。

状态：⏳ 待实测（依赖编码器反馈干净，见注意事项）。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：编码器反馈在电机运行时被 PWM 噪声污染

问题：电机转动时 50ms/20ms 瞬时 rpm 读数 40-120 乱跳，停转时读数 0 干净。

原因：PWM 开关噪声耦合进编码器 A/B 线；编码器为开集输出、引脚未上拉（NOPULL）时更易受扰。

应对：PA6/PA7 改内部上拉（GPIO_PULLUP）；软件 EMA 平滑；PWM 降频到 2kHz。

注意：若上拉后读数仍乱，需硬件处理（A/B 加 10k 上拉 + 100nF 滤波、分线、共地），否则 PID 调参会非常困难。

---

## 7. 今日成果（Result）

完成：

* [x] 增量式 PID（积分分离 + 输出限幅），编译通过
* [x] 20ms 控制周期接入 CanTxTask
* [x] 编码器输入改为上拉（抗噪声）
* [ ] PID 闭环实测收敛（待烧录）

---

## 8. 工程总结（Engineering Summary）

## 反馈质量决定闭环上限

PID 只能控制“看到的转速”；反馈被噪声污染时，误差抖动会让输出占空比剧烈波动。先保证测量，再谈控制。

---

## 9. 面试问答（Interview Prep）

### Q1：位置式 PID 和增量式 PID 的区别？

答题要点：

* 位置式输出绝对量、需累加所有历史误差，易积分饱和
* 增量式只输出变化量，无累积误差，便于限幅与切换

### Q2：什么是积分饱和？怎么处理？

答题要点：

* 执行器饱和后误差持续积分，恢复时严重超调
* 处理：积分限幅、积分分离、遇限消积分

---

## 10. 下一步计划（Next Step）

Day28：调参 + 三节点联调

* 用 SerialPlot 观察阶跃/抗扰，调 Kp/Ki/Kd
* CAN 改目标速度验证跟踪
* 三节点联调回归
