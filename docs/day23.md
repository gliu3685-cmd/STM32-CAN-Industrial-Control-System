# Day23 - Node2 电机驱动：TIM2 PWM 与 TIM3 编码器模式

## 1. 今日目标（Objectives）

本日启动第四阶段电机控制闭环，重点掌握：

* TIM2_CH1（PA0）PWM 输出驱动 L298N，理解占空比与频率计算
* TIM3 编码器模式（PA6/PA7）计数，理解 A/B 正交信号与 4 倍频
* F1 定时器 GPIO 配置（PWM 输出用 AF_PP、编码器输入用 INPUT）
* 在不动 CubeMX 生成代码的前提下手写 HAL 驱动（避免覆盖 CAN 修复）

---

## 2. 理论学习（Theory）

### 2.1 PWM 原理

TIM 计数器从 0 递增到 ARR（自动重装载值），CCR（比较寄存器）决定翻转点：

```text
CNT < CCR  → 输出高
CNT ≥ CCR  → 输出低
占空比 = CCR / (ARR+1)
频率 = 定时器时钟 / (PSC+1) / (ARR+1)
```

本工程：定时器时钟 72MHz，PSC=7 → 9MHz，ARR=999 → 9kHz，占空比 0~1000（0.1% 分辨率）。

### 2.2 编码器模式

TIM3 编码器模式直接接 A/B 两路正交信号，硬件自动按相位关系加减计数：

* A 超前 B（正转）→ CNT 递增；B 超前 A（反转）→ CNT 递减
* `TIM_ENCODERMODE_TI12`：双通道 4 倍频，A/B 双沿都计数
* 读取 CNT 即可得到带方向的累计计数

### 2.3 F1 定时器 GPIO

* PA0（TIM2_CH1）输出：`GPIO_MODE_AF_PP`（F1 复用推挽）
* PA6/PA7（TIM3_CH1/CH2）输入：`GPIO_MODE_INPUT`
* PA1：普通推挽输出，控制 L298N 方向

### 2.4 为什么手写 HAL 而不重新生成 CubeMX

Node2 的 can.c/stm32f1xx_hal_msp.c 包含 Day17 手写的 F103 CAN_RX 输入模式修复。CubeMX 重新生成会把这些修复覆盖回有 bug 的默认配置。因此 TIM 驱动采用"手动补齐 HAL 源文件 + 手写初始化"的方式，与 can.c 的做法一致。

---

## 3. 实验环境（Environment）

硬件：

* Node2（F103 电机节点）+ L298N + 直流减速电机 + 霍尔编码器
* 12V 电源线待收货（上电验证延后）

软件：

* STM32CubeIDE 2.2.0、STM32Cube_FW_F1 V1.8.7 HAL 包

工程：`firmware/motor_node`

当前阶段：第四阶段电机控制闭环

---

## 4. 代码实现

### 4.1 补齐 HAL TIM 驱动

工程原 HAL 目录缺少 TIM 模块源文件，从 STM32Cube_FW_F1_V1.8.7（与工程 HAL 版本哈希一致）拷贝：

* `stm32f1xx_hal_tim.c` / `stm32f1xx_hal_tim_ex.c`（Src）
* `stm32f1xx_hal_tim.h` / `stm32f1xx_hal_tim_ex.h`（Inc）

并在 `stm32f1xx_hal_conf.h` 打开 `HAL_TIM_MODULE_ENABLED`。

### 4.2 电机驱动（app_motor.c）

```c
void MotorInit(void)
{
    /* PA0 = TIM2_CH1 PWM（AF_PP）、PA1 = 方向、PA6/PA7 = 编码器输入 */
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* TIM2：PWM 9kHz（PSC=7 → 9MHz，ARR=999 → 9kHz） */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 7U;
    htim2.Init.Period = 999U;
    HAL_TIM_PWM_Init(&htim2);
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

    /* TIM3：编码器模式（TI12 4 倍频） */
    TIM_Encoder_InitTypeDef enc = {0};
    enc.EncoderMode = TIM_ENCODERMODE_TI12;
    ...
    HAL_TIM_Encoder_Init(&htim3, &enc);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}
```

### 4.3 接入发送任务（app_can.c）

CanTxTask 在 CanStart() 后初始化电机并输出 30% 测试占空比，每秒打印编码器计数与增量：

```c
MotorInit();
MotorSetDir(1U);
MotorSetDuty(300U);   /* 30% 测试占空比 */
...
NodePrint("[NODE2] ENC cnt=%ld delta=%ld\r\n", enc, enc - last_enc);
```

### 4.4 等待电源期间：Node1 MPU6050 软件 I2C 驱动（app_mpu6050.c）

利用等电源时间完成 Node1 传感器接入：

* 软件 I2C（PB6=SCL、PB7=SDA，开漏输出 + 模块上拉），避开 F1 硬件 I2C 兼容性问题
* 寄存器配置：唤醒（PWR_MGMT_1=0）、采样率 125Hz、±2g、±250°/s，WHO_AM_I=0x68 校验
* 0x3B 起突发读 14 字节：加速度(6) + 温度(2) + 陀螺仪(6)
* 每秒读取并串口打印；新增 **0x103 帧**上报加速度计（DLC=6，ax/ay/az 小端），主控过滤器暂不接收，PCAN-View 可观测

---

## 5. 实验过程（Experiment）

### 实验 1：CLI 编译验证

现象：`Build Finished. 0 errors, 0 warnings`，生成 f103_blink_node2.elf（text=36604）。

验证：新文件（app_motor.c + HAL TIM 源文件）被构建系统自动纳入，代码编译通过。

### 实验 2：上电验证（待 12V 电源线到货）

预期现象：

* 串口打印 `[NODE2] MotorInit ok, PWM 9kHz, encoder ready`
* 电机以 30% 占空比转动
* `[NODE2] ENC cnt=... delta=...` 每秒递增（正转）

状态：✅ 固件就绪，⏳ 等待电源。

### 实验 3：MPU6050 串口/CAN 验证（待烧录）

现象（烧录 Node1 后）：

* 串口打印 `[NODE1] MPU6050 init ok`
* 每秒 `[NODE1] MPU ax=... ay=... az=... gx=... gy=... gz=...`，数据实时变化
* 实测静止读数：`ax=16428 ay=-1764 az=1464`（模块竖插面包板，X 轴朝上）；三轴模长 ≈ 16586 ≈ 16384（1g），物理一致
* PCAN-View 每秒出现 0x103 帧（DLC=6）

验证：I2C 通信正常（WHO_AM_I=0x68）、读数实时变化、任意姿态下模长恒等 1g。

状态：✅ 验证通过。

---

## 6. 遇到的问题与解决（Problems & Solutions）

### Problem 1：HAL TIM 源文件缺失

问题：工程 HAL 目录无 `stm32f1xx_hal_tim.c`，`HAL_TIM_PWM_Init` 等 API 不可用。

原因：CubeMX 创建工程时未启用 TIM 外设，未拷贝对应 HAL 源文件。

解决：从 STM32Cube_FW_F1 V1.8.7（与工程 HAL 哈希一致）手动补齐 4 个文件，并在 hal_conf.h 打开模块开关。

### Problem 2：HAL_TIM_Encoder_Init 参数类型

问题：首次写法 `HAL_TIM_Encoder_Init(&htim3, TIM_ENCODERMODE_TI12)` 编译报错"makes pointer from integer"。

原因：F1 HAL V1.8.7 该 API 第二参数是 `TIM_Encoder_InitTypeDef*` 结构体，而非直接传模式值（新版 HAL 才支持直接传值）。

解决：构造编码器配置结构体（EncoderMode/IC1/IC2 极性等）后传入。

---

## 7. 今日成果（Result）

完成：

* [x] TIM2 PWM（PA0）配置与 9kHz 驱动代码
* [x] TIM3 编码器模式（PA6/PA7）配置与计数读取
* [x] L298N 方向控制（PA1）
* [x] HAL TIM 驱动补齐与模块开关
* [x] CLI 编译通过（0 错误 0 警告）
* [x] 测试占空比 30% + 编码器计数打印（待电源上电验证）
* [x] Node1 MPU6050 软件 I2C 驱动 + 0x103 加速度帧（CLI 编译通过）

---

## 8. 工程总结（Engineering Summary）

## PWM 频率与占空比的计算闭环

PSC 与 ARR 决定频率，CCR 决定占空比；改任何一个都要重算，面试常考公式。

## 手写 HAL 的代价与收益

补齐 HAL 源文件 + 手写初始化绕开了 CubeMX 覆盖风险，但要注意 API 版本差异（本日 Encoder_Init 参数踩坑），编译验证不可或缺。

---

## 9. 面试问答（Interview Prep）

### Q1：PWM 频率怎么算？

答题要点：

* 频率 = 定时器时钟 / (PSC+1) / (ARR+1)
* 占空比 = CCR / (ARR+1)
* 本工程 72M/8/1000 = 9kHz，占空比 0.1% 分辨率

### Q2：为什么编码器用 A/B 两路？

答题要点：

* 正交信号可同时测速与判方向
* TIM 编码器模式硬件 4 倍频，减少软件开销

### Q3：为什么不能直接用 CubeMX 重新生成？

答题要点：

* 会覆盖手写的 CAN_RX 输入模式修复（F1 关键差异）
* 手写驱动需要核对 HAL API 版本差异，并做编译验证

---

## 10. Git 提交

```bash
git add firmware/motor_node docs/day23.md 学习进度.md README.md
git commit -m "feat(day23): Node2 TIM2 PWM + TIM3 encoder driver, compile verified"
```

---

## 11. 下一步计划（Next Step）

Day24：

* PWM 驱动验证：电源到位后电机转动、编码器计数递增
* CAN 命令闭环：主控 0x201 设置速度 → Node2 实际驱动电机
* 0x202 上报真实编码器读数

向 PID 闭环迈进。
