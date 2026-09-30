# 文档索引

> 本目录全部文档的分类索引。项目总览、协议表和整体进度见仓库根目录 [README.md](../README.md)。
> 归档内容（早期 FreeRTOS 练习、Keil 尝试）在仓库根目录 `archive/`，不参与主工程。

---

## 接手 / 进度

- [HANDOFF.md](HANDOFF.md) — 新线程交接文档：当前状态、约束、下一步
- [../学习进度.md](../学习进度.md) — Day1-40 进度跟踪表

---

## 每日学习记录（Day 1-34）

模板：[day_template.md](day_template.md)

**阶段一：STM32 基础**

- [Day 1](day01.md) STM32 开发环境搭建
- [Day 2](day02.md) GPIO + UART 通信
- [Day 3](day03.md) UART 调试系统
- [Day 4](day04.md) Timer 定时器与中断

**阶段二：FreeRTOS 实时系统**

- [Day 5](day05.md) FreeRTOS 基础移植
- [Day 6](day06.md) 任务管理与调度
- [Day 7](day07.md) Queue 任务通信
- [Day 8](day08.md) 同步机制（Semaphore & Mutex）
- [Day 9](day09.md) 软件定时器与临界区保护
- [Day 10](day10.md) 内存管理（heap_1 ~ heap_5）
- [Day 11](day11.md) 任务设计与优先级管理（优先级反转）
- [Day 12](day12.md) 事件组与任务通知
- [Day 13](day13.md) 异常检测与调试（栈溢出/HardFault）
- [Day 14](day14.md) 综合架构设计（Phase 2 收官）

**阶段三：CAN 通信系统**

- [Day 15](day15.md) CAN 通信（F407 CAN1 500kbps）
- [Day 16](day16.md) F103 节点工程搭建
- [Day 17](day17.md) F103 CAN 排障与节点打通
- [Day 18](day18.md) 三节点联调（协议映射 + 双 Bank 过滤）
- [Day 19](day19.md) 心跳协议文档化 + 故障注入测试
- [Day 20](day20.md) CAN 理论收尾（帧格式/位同步/仲裁/过滤器）
- [Day 21](day21.md) 请求/应答命令帧 + 压力测试（Phase 3 收官）

**阶段四：电机控制闭环**

- [Day 22](day22.md) 电机 PID 预热（理论 + 硬件接线）
- [Day 23](day23.md) Node2 TIM2 PWM + TIM3 编码器驱动
- [Day 24](day24.md) CAN 命令驱动电机 + 真实编码器上报
- [Day 25](day25.md) 编码器标定 + RPM 换算
- [Day 26](day26.md) SerialPlot 波形 + 开环阶跃
- [Day 27](day27.md) PID 速度闭环
- [Day 28](day28.md) PID 调参与三节点联调

**阶段五：IAP Bootloader**

- [Day 29](day29.md) Bootloader 理论 + Flash 布局
- [Day 30](day30.md) APP 地址偏移 + VTOR
- [Day 31](day31.md) Bootloader 跳转 APP
- [Day 32](day32.md) 升级模式设计
- [Day 33](day33.md) UART 固件传输（帧协议 + 分包 + ACK）
- [Day 34](day34.md) CRC32 完整性校验

---

## 架构与协议

- [system-architecture.md](system-architecture.md) — 系统架构与 FreeRTOS 任务设计
- [can-protocol.md](can-protocol.md) — CAN 应用层协议（帧 ID 总表）
- [bootloader-plan.md](bootloader-plan.md) — Bootloader 实施计划（Flash 分区/跳转/升级协议）

---

## 设计与代码解析

- [can-ota-design.md](can-ota-design.md) — **CAN OTA 设计文档**（下一步要实现）
- [code-walkthrough-full.md](code-walkthrough-full.md) — 全项目代码逐文件解析（[PDF](code-walkthrough-full.pdf)）
- [can-walkthrough.md](can-walkthrough.md) — CAN 代码五层详解 + 面试速查
- [can-pid-notes.md](can-pid-notes.md) — CAN / PID 要点整理

---

## 复盘 / 面试 / 学习

- [phase3-review.md](phase3-review.md) — Phase 3 复盘与排障方法论
- [project-narrative.md](project-narrative.md) — 项目叙事（简历版 + 口述版）
- [interview-notes.md](interview-notes.md) — 面试问答笔记（FreeRTOS/CAN/PID/Bootloader）
- [project-deep-self-quiz.md](project-deep-self-quiz.md) — 项目深度自测题库（[PDF](project-deep-self-quiz.pdf)）
- [embedded-c-learning-path.md](embedded-c-learning-path.md) — 嵌入式 C 学习路线与练习（[PDF](embedded-c-learning-path.pdf)）
- [hardware-general-interview-bank.md](hardware-general-interview-bank.md) — 硬件通用面试题库（[PDF](hardware-general-interview-bank.pdf)）
