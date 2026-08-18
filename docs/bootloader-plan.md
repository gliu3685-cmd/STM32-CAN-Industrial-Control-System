# IAP Bootloader（F407ZGTx + UART）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: 使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 按任务逐项实施。步骤用 `- [ ]` 复选框跟踪进度。

**Goal:** 在 F407ZGTx 主控上实现 UART 串口 IAP：Bootloader 引导 + CRC32 校验 + Python 上位机升级，APP 从 0x08010000 运行并支持串口热升级。

**Architecture:** Bootloader 占用 Flash 前 64KB（Sector 0-3），上电打印版本并等待 1s 升级握手；收到 `u` 进入升级模式（擦除→分包写入→CRC32 全量校验→写有效标志→重启），收到 `j` 强制跳转（调试用），超时则校验 APP 有效后跳转 0x08010000。APP 工程只改链接脚本与向量表偏移，业务代码不动。

**Tech Stack:** STM32F407ZGTx + HAL（STM32CubeIDE 2.2.0）、USART1 115200 8N1、Flash 扇区擦写、CRC32（IEEE 802.3）、Python 3 + pyserial。

**Spec:** 《完整项目方案与40天学习计划.md》第 5 阶段（Day 29-34）；工程约束来自 docs/HANDOFF.md、hardware/ 接线文档与本计划。

## Global Constraints

- 芯片：STM32F407ZGT6（LQFP144，1024KB Flash，128KB RAM）。**已由用户确认板载丝印为 F407ZGTx**，master 工程 CubeMX 配置（ZGTx/1024KB）与实际一致。
- Flash 布局：Bootloader `0x08000000~0x0800FFFF`（64KB，Sector 0-3，各 16KB）；APP 起始 `0x08010000`（Sector 4，64KB 起），最大 960KB（`0xF0000`）。
- 有效 APP 栈指针校验范围：`0x20000000~0x2001FFFF`（主 SRAM 128KB）。
- UART：USART1 PA9/PA10，115200 8N1，与现有调试串口复用；升级模式下 Bootloader 不输出任何非协议字节。
- CRC32：IEEE 802.3（多项式 0xEDB88320），C 实现与 Python `zlib.crc32` 逐字节一致；校验向量 `crc32("123456789") == 0xCBF43926`。
- 协议帧：`A5 5A | CMD(1B) | ADDR(4B LE) | LEN(2B LE) | DATA(LEN<=256) | CRC32(4B LE，仅 DATA)`；应答 `0x06`=ACK，`0x15`=NACK；Python 侧超时 500ms、每帧重试 3 次。
- 升级命令：`0x01`=ERASE，`0x02`=WRITE，`0x03`=FINISH，`0x04`=REBOOT。
- 有效标志：升级校验通过后，在 `APP_START + 0x100` 写入 `0xA5A5A5A5`（向量表保留区，不影响运行）。
- 依赖限制：Python 工具唯一第三方依赖为 pyserial；固件不引入新库。
- Git：不自动 push；每个任务一个 commit；改动 master 工程前先提交当前基线（可回退）。
- 构建命令（全任务通用）：
  ```powershell
  & "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe" -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data $env:TEMP\iap_ws -import "<工程绝对路径>" -build "<工程名>/Debug"
  ```
  预期：`Build Finished. 0 errors, 0 warnings`。

---

### Task 1: 新建 Bootloader 工程（芯片已确认 F407ZGTx）

**Files:**
- Create: `firmware/bootloader`（CubeIDE 新工程 `f407_bootloader`，设备 STM32F407ZGTx）
- Modify: 新工程 `STM32F407ZGTX_FLASH.ld`

**Interfaces:**
- Produces: 可烧录的最小 Bootloader 骨架（串口打印 `BOOT v0.1`）。

- [x] **Step 1: 记录芯片结论**

  用户已确认板载丝印为 `STM32F407ZGT6`（LQFP144，1024KB Flash）。按本计划 960KB APP 布局执行；master 工程设备配置已是 ZGTx，无需修改。结论写入本计划“结论”处。

- [x] **Step 2: CubeIDE 新建工程**

  新建 STM32 工程 `f407_bootloader`，设备选 STM32F407ZGTx（LQFP144）；仅开启 USART1（115200、8N1、中断）与默认 SysTick；不启用 FreeRTOS。时钟参照 master 工程：HSE 8MHz → SYSCLK 168MHz。

- [x] **Step 3: 修改链接脚本 FLASH 长度**

  新工程的链接脚本（`STM32F407ZGTX_FLASH.ld`）MEMORY 段改为：

  ```c
  FLASH    (rx)    : ORIGIN = 0x08000000,   LENGTH = 64K
  RAM      (xrw)   : ORIGIN = 0x20000000,   LENGTH = 128K
  CCMRAM   (xrw)   : ORIGIN = 0x10000000,   LENGTH = 64K
  ```

- [x] **Step 4: main.c 打印 BOOT 版本**

  在 `main()` 完成时钟/串口初始化后：

  ```c
  printf("BOOT v0.1\r\n");
  ```

- [x] **Step 5: CLI 编译验证**

  运行 Global Constraints 中的构建命令，预期 0 errors、0 warnings。

- [x] **Step 6: ST-Link 烧录 + 串口验证**

  USB-TTL 接 PA9(RX)/PA10(TX)/GND，115200 打开串口，上电应见 `BOOT v0.1`。

- [x] **Step 7: Commit**

  ```bash
  git add firmware/bootloader
  git commit -m "feat(bootloader): F407 UART IAP skeleton (BOOT v0.1, 64KB layout)"
  ```

---

### Task 2: 启动路径选择（1s 握手窗口）

**Files:**
- Create: `firmware/bootloader/Core/Src/bl_uart.c` / `firmware/bootloader/Core/Inc/bl_uart.h`
- Modify: `firmware/bootloader/Core/Src/main.c`

**Interfaces:**
- Consumes: `huart1`（CubeMX 生成，USART1 115200）。
- Produces: `void UART_Send(const uint8_t *buf, uint16_t len);`、`int UART_GetByte(uint8_t *b);`（非阻塞，返回 1=有数据）。

- [x] **Step 1: 实现 bl_uart.c**

  ```c
  void UART_Send(const uint8_t *buf, uint16_t len)
  {
      HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
  }

  int UART_GetByte(uint8_t *b)
  {
      return (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) ?
             (HAL_UART_Receive(&huart1, b, 1, 0) == HAL_OK) : 0;
  }
  ```

- [x] **Step 2: main.c 加入启动分支**

  ```c
  printf("BOOT v0.1\r\n");
  uint8_t ch = 0;
  int got = 0;
  for (uint32_t i = 0; i < 1000; i++) {          /* 1s 窗口 */
      if (UART_GetByte(&ch)) { got = 1; break; }
      HAL_Delay(1);
  }
  if (got && ch == 'u') {
      printf("UPGRADE MODE\r\n");
      /* Task 6 接入协议循环 */
      while (1) { HAL_Delay(1000); }
  } else if (got && ch == 'j') {
      /* Task 7 实现 JumpToApp 后接入 */
      printf("JUMP\r\n");
      while (1) { HAL_Delay(1000); }
  } else {
      printf("NO APP\r\n");                      /* Task 7 改为有效检测 + 跳转 */
      while (1) { HAL_Delay(1000); }
  }
  ```

- [x] **Step 3: 编译 + 烧录验证**

  上电不发字节 → 打印 `NO APP`；1s 内发 `u` → `UPGRADE MODE`；发 `j` → `JUMP`。

- [x] **Step 4: Commit**

  ```bash
  git add firmware/bootloader
  git commit -m "feat(bootloader): boot path selection with 1s UART handshake"
  ```

---

### Task 3: CRC32 模块

**Files:**
- Create: `firmware/bootloader/Core/Src/bl_crc.c` / `firmware/bootloader/Core/Inc/bl_crc.h`

**Interfaces:**
- Produces: `uint32_t BlCrc32(const uint8_t *data, uint32_t len);`（标准 IEEE 802.3，初值 0xFFFFFFFF，异或输出）。

- [x] **Step 1: 实现 bl_crc.c**

  ```c
  static const uint32_t crc_table[256] = {
      /* 用下方 Step 2 的生成脚本输出，粘贴到这里（256 项） */
  };

  uint32_t BlCrc32(const uint8_t *data, uint32_t len)
  {
      uint32_t crc = 0xFFFFFFFFU;
      for (uint32_t i = 0; i < len; i++) {
          crc = crc_table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
      }
      return crc ^ 0xFFFFFFFFU;
  }
  ```

- [x] **Step 2: 生成校验表并用标准向量核对**

  在仓库根目录运行以下脚本，把输出的 256 项粘贴进 `crc_table[]`：

  ```powershell
  python -c "
  t = []
  for i in range(256):
      c = i
      for _ in range(8):
          c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
      t.append(c)
  for r in range(0, 256, 8):
      print('    ' + ', '.join('0x%08XU' % x for x in t[r:r+8]) + ',')
  "
  ```

  核对标准向量：

  ```powershell
  python -c "import zlib; print(hex(zlib.crc32(b'123456789')))"
  ```

  预期输出 `0xcbf43926`；固件中 `BlCrc32((uint8_t*)"123456789", 9)` 必须返回同一值（在 Task 5 的协议自测里断言）。

- [x] **Step 3: 编译验证**

  CLI 构建 0 errors。

- [x] **Step 4: Commit**

  ```bash
  git add firmware/bootloader
  git commit -m "feat(bootloader): CRC32 (IEEE 802.3) matching Python zlib"
  ```

---

### Task 4: Flash 擦写模块

**Files:**
- Create: `firmware/bootloader/Core/Src/bl_flash.c` / `firmware/bootloader/Core/Inc/bl_flash.h`

**Interfaces:**
- Produces:
  - `int BlFlashEraseRange(uint32_t addr, uint32_t len);`（按 sector 擦除覆盖区间，返回 0=成功）
  - `int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len);`（64 位双字编程，尾块补 0xFF）
  - `void BlFlashWriteMagic(void);`（在 `APP_START+0x100` 写 `0xA5A5A5A5`）
  - `int BlAppValid(void);`（栈指针在 0x20000000~0x2001FFFF 且 magic 正确）

- [x] **Step 1: 实现 bl_flash.c**

  ```c
  #define APP_START  0x08010000U
  #define APP_MAGIC  0xA5A5A5A5U

  int BlFlashEraseRange(uint32_t addr, uint32_t len)
  {
      FLASH_EraseInitTypeDef erase = {0};
      uint32_t fail = 0;
      uint32_t start_sec = AddrToSector(addr), end_sec = AddrToSector(addr + len - 1);
      HAL_FLASH_Unlock();
      for (uint32_t sec = start_sec; sec <= end_sec; sec++) {
          erase.TypeErase = FLASH_TYPEERASE_SECTORS;
          erase.Sector = sec;
          erase.NbSectors = 1;
          erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
          if (HAL_FLASHEx_Erase(&erase, &fail) != HAL_OK) { HAL_FLASH_Lock(); return -1; }
      }
      HAL_FLASH_Lock();
      return 0;
  }

  static uint32_t AddrToSector(uint32_t addr)
  {
      if (addr < 0x08010000U) return (addr - 0x08000000U) / 0x4000U; /* sector 0-3，16KB */
      if (addr < 0x08020000U) return 4U;                             /* sector 4，64KB */
      return 5U + (addr - 0x08020000U) / 0x20000U;                   /* sector 5-11，128KB */
  }

  int BlFlashWriteRange(uint32_t addr, const uint8_t *data, uint32_t len)
  {
      uint64_t word = 0xFFFFFFFFFFFFFFFFULL;
      HAL_FLASH_Unlock();
      for (uint32_t i = 0; i < len; i++) {
          word &= ~(0xFFULL << ((i % 8) * 8));
          word |= (uint64_t)data[i] << ((i % 8) * 8);
          if ((i % 8) == 7 || i == len - 1) {
              if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                    addr + i - (i % 8), word) != HAL_OK) {
                  HAL_FLASH_Lock(); return -1;
              }
              word = 0xFFFFFFFFFFFFFFFFULL;
          }
      }
      HAL_FLASH_Lock();
      return 0;
  }

  void BlFlashWriteMagic(void)
  {
      HAL_FLASH_Unlock();
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START + 0x100U,
                        (uint32_t)APP_MAGIC);
      HAL_FLASH_Lock();
  }

  int BlAppValid(void)
  {
      uint32_t sp = *(volatile uint32_t *)APP_START;
      uint32_t magic = *(volatile uint32_t *)(APP_START + 0x100U);
      return (sp >= 0x20000000U && sp < 0x20020000U) && (magic == APP_MAGIC);
  }
  ```

  > 注意：F407 扇区边界为 16KB/64KB/128KB，擦除必须按 `AddrToSector` 精确映射，不能按地址直接除以 16KB；`BlFlashWriteMagic` 与 `BlFlashWriteRange` 不能同时擦同一 sector（顺序在协议层保证）。

- [x] **Step 2: 编译验证**

  CLI 构建 0 errors。

- [x] **Step 3: Commit**

  ```bash
  git add firmware/bootloader
  git commit -m "feat(bootloader): flash erase/write/magic/validity module"
  ```

---

### Task 5: 升级协议帧解析

**Files:**
- Create: `firmware/bootloader/Core/Src/bl_protocol.c` / `firmware/bootloader/Core/Inc/bl_protocol.h`
- Modify: `firmware/bootloader/Core/Src/main.c`（升级模式接入协议循环）

**Interfaces:**
- Consumes: `UART_GetByte`、`UART_Send`、`BlCrc32`。
- Produces: `void BlProtocolRun(void);`（阻塞式协议循环，仅回 ACK/NACK，暂不处理 Flash 命令）。

- [x] **Step 1: 实现帧状态机 bl_protocol.c**

  ```c
  void BlProtocolRun(void)
  {
      uint8_t rx = 0, buf[7 + 256 + 4];   /* CMD+ADDR4+LEN2 + DATA(<=256) + CRC4 */
      uint16_t plen = 0;
      uint32_t idx = 0;
      int stage = 0;
      for (;;) {
          if (!UART_GetByte(&rx)) { HAL_Delay(1); continue; }
          if (stage == 0) { if (rx == 0xA5) stage = 1; continue; }
          if (stage == 1) { stage = (rx == 0x5A) ? 2 : 0; continue; }
          if (stage == 2) {                    /* 帧头：CMD+ADDR4+LEN2 */
              buf[idx++] = rx;
              if (idx == 7) {
                  plen = (uint16_t)(buf[5] | (buf[6] << 8));
                  if (plen > 256) { idx = 0; stage = 0; continue; }
                  stage = 3;
              }
              continue;
          }
          if (stage == 3) {                    /* DATA + CRC32(4B，仅 DATA) */
              buf[7 + idx] = rx;
              idx++;
              if (idx == plen + 4) {
                  uint32_t crc_rx = (uint32_t)buf[7 + plen] |
                      ((uint32_t)buf[8 + plen] << 8) |
                      ((uint32_t)buf[9 + plen] << 16) |
                      ((uint32_t)buf[10 + plen] << 24);
                  uint32_t crc_calc = BlCrc32(&buf[7], plen);
                  if (crc_calc != crc_rx) {
                      uint8_t nack = NACK;
                      UART_Send(&nack, 1);
                  } else {
                      /* 命令结果（ACK/NACK）由 BlDispatchFrame 统一回执 */
                      BlDispatchFrame(buf, plen);   /* Task 5 空实现回 ACK，Task 6 填充 */
                  }
                  idx = 0; stage = 0;
              }
              continue;
          }
      }
  }
  ```

  `bl_protocol.h` 定义命令/应答常量与分发函数：

  ```c
  #define CMD_ERASE   0x01U
  #define CMD_WRITE   0x02U
  #define CMD_FINISH  0x03U
  #define CMD_REBOOT  0x04U
  #define ACK         0x06U
  #define NACK        0x15U

  void BlProtocolRun(void);
  void BlDispatchFrame(const uint8_t *hdr, uint16_t plen);
  ```

  帧布局约定：`buf[0]=CMD`，`buf[1..4]=ADDR`（小端），`buf[5..6]=LEN`（小端，payload 长度，≤256），`buf[7..7+plen-1]=DATA`，`buf[7+plen..10+plen]=CRC32`（仅 DATA，小端）。

  `bl_protocol.c` 中 Task 5 阶段的 `BlDispatchFrame` 为空实现（仅回 ACK，不执行命令），Task 6 替换为真正分发：

  ```c
  void BlDispatchFrame(const uint8_t *hdr, uint16_t plen)
  {
      (void)hdr;
      (void)plen;
      uint8_t ack = ACK;      /* Task 5：仅确认帧合法，不执行命令 */
      UART_Send(&ack, 1);
  }
  ```

- [x] **Step 2: main.c 升级模式改为调用协议循环**

  把 Task 2 中 `UPGRADE MODE` 分支的 `while(1)` 替换为 `BlProtocolRun();`。

- [x] **Step 3: 自测帧收发**

  用 Python 脚本发一帧 WRITE（8 字节数据，目标地址 0x08010000）：

  ```python
  import zlib, serial
  data = bytes([0x02]) + (0x08010000).to_bytes(4, "little") + (8).to_bytes(2, "little") \
       + bytes([1, 2, 3, 4, 5, 6, 7, 8])
  frame = b"\xA5\x5A" + data + (zlib.crc32(data[7:]) & 0xFFFFFFFF).to_bytes(4, "little")
  ser = serial.Serial("COM3", 115200, timeout=1)
  ser.write(frame)
  print(ser.read(1).hex())          # 预期 06；改坏一个数据字节后预期 15
  ```

  注：`BlDispatchFrame` 目前为空实现，因此合法帧仍回 `0x06`（CRC 通过），坏帧回 `0x15`。

- [x] **Step 4: Commit**

  ```bash
  git add firmware/bootloader
  git commit -m "feat(bootloader): upgrade protocol frame parser with ACK/NACK"
  ```

---

### Task 6: 升级流程整合（ERASE→WRITE→FINISH→REBOOT）

**Files:**
- Modify: `firmware/bootloader/Core/Src/bl_protocol.c`
- Create: `tools/proto_smoke.py`（临时联调脚本，验证后可保留为回归工具）

**Interfaces:**
- Consumes: `BlFlashEraseRange`、`BlFlashWriteRange`、`BlFlashWriteMagic`、`BlCrc32`。
- Produces: 完整升级流程：收到 ERASE 按长度擦 sector；WRITE 校验 `0x08010000 <= addr` 且 `addr+len <= 0x08100000` 后写入；FINISH 收到总长度与全图 CRC32，对 Flash 区间重算比对，通过则写 magic 并 ACK；REBOOT 复位跳转。

- [x] **Step 1: 命令分发**

  在 `bl_protocol.c` 实现 `BlDispatchFrame`（替换 Task 5 的空实现）：

  ```c
  void BlDispatchFrame(const uint8_t *b, uint16_t plen)
  {
      uint32_t addr = (uint32_t)b[1] | ((uint32_t)b[2] << 8) |
                      ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 24);
      int ok = 0;
      switch (b[0]) {
      case CMD_ERASE:  /* DATA = 镜像总长度(4B LE)，按长度擦对应 sector */
          ok = (BlFlashEraseRange(APP_START, LE32(&b[7])) == 0);
          break;
      case CMD_WRITE:  /* ADDR = 目标地址，DATA = 分块(<=256B) */
          ok = (addr >= APP_START && addr + plen <= APP_START + APP_LEN_MAX)
               && (BlFlashWriteRange(addr, &b[7], plen) == 0);
          break;
      case CMD_FINISH: /* DATA = 镜像总长度(4B LE) + CRC32(4B LE) */
          ok = BlFinishImage(LE32(&b[7]), LE32(&b[11]));
          break;
      case CMD_REBOOT:
          NVIC_SystemReset();
          break;
      default:
          ok = 0;
          break;
      }
      if (b[0] != CMD_REBOOT) {
          UART_Send(&(uint8_t){ ok ? ACK : NACK }, 1);
      }
  }
  ```

  其中 `APP_START=0x08010000`、`APP_LEN_MAX=0xF0000`、`LE32(p)=p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24)`，定义在 `bl_flash.h`。

- [x] **Step 2: 实现 BlFinishImage**

  ```c
  static int BlFinishImage(uint32_t total_len, uint32_t crc_expect)
  {
      uint32_t crc_calc = BlCrc32((const uint8_t *)APP_START, total_len);
      if (crc_calc != crc_expect) return 0;
      BlFlashWriteMagic();
      return 1;
  }
  ```

- [x] **Step 3: 用 proto_smoke.py 走完整流程**

  脚本流程：等 `BOOT` → 发 `u` → 发 ERASE（ADDR=0x08010000，DATA=4096 的 4B LE）→ 分 16 帧 WRITE 写入 4096 字节 `0xAA` → 发 FINISH（DATA=4096 的 4B LE + CRC32 的 4B LE）→ 发 REBOOT。串口应依次见 ACK；用调试器读 `0x08010100` 应等于 `0xA5A5A5A5`（magic 已写）。注意 `0xAA` 填充不是合法 APP（SP=0xAAAAAAAA 不在 RAM 范围），`BlAppValid()` 为假属预期，真实跳转由 Task 7/8 验证。

- [x] **Step 4: Commit**

  ```bash
  git add firmware/bootloader tools/proto_smoke.py
  git commit -m "feat(bootloader): full UART upgrade flow with CRC verify + magic flag"
  ```

---

### Task 7: APP 工程改造（0x08010000 + VTOR）

**Files:**
- Modify: `firmware/master_node/STM32F407ZGTX_FLASH.ld`
- Modify: `firmware/master_node/Core/Src/main.c`

**Interfaces:**
- Produces: 可被 Bootloader 跳转的 APP（打印 `F407 App v1.0 Start!`）。

- [x] **Step 1: 先提交当前 master 基线（可回退）**

  ```bash
  git add firmware/master_node
  git commit -m "chore(master): baseline before IAP relocation"
  ```

- [x] **Step 2: 修改链接脚本**

  `STM32F407ZGTX_FLASH.ld` 的 FLASH 段改为：

  ```c
  FLASH    (rx)    : ORIGIN = 0x08010000,   LENGTH = 960K
  ```

- [x] **Step 3: main.c 设置向量表偏移**

  `main()` 第一行（HAL_Init 之前）：

  ```c
  SCB->VTOR = 0x08010000U;
  ```

  并把启动打印改为 `printf("F407 App v1.0 Start!\r\n");`（原为 `F407 Master Start!`，仅用于区分版本）。

- [x] **Step 4: 实现跳转函数并接入 Bootloader 启动路径**

  `bl_flash.c` 增加：

  ```c
  void BlJumpToApp(void)
  {
      uint32_t sp = *(volatile uint32_t *)APP_START;
      void (*reset)(void) = (void (*)(void))(*(volatile uint32_t *)(APP_START + 4));
      __disable_irq();          /* 跳转前关闭中断，交给 APP 重新初始化 */
      __set_MSP(sp);
      reset();                  /* 永不返回 */
  }
  ```

  Bootloader `main.c` 的默认分支改为：

  ```c
  if (BlAppValid()) { BlJumpToApp(); }
  printf("NO APP\r\n");
  while (1) { HAL_Delay(1000); }
  ```

  同时把 Task 2 的 `j` 分支（强制跳转）改为直接调用：

  ```c
  } else if (got && ch == 'j') {
      BlJumpToApp();          /* 强制跳转（调试用，不检查 magic） */
  }
  ```

- [x] **Step 5: CLI 编译验证**

  0 errors；确认生成的 `.elf`/`.bin` 加载地址以 0x08010000 开头。

- [x] **Step 6: 联调跳转**

  1. ST-Link 把 Bootloader 烧到 0x08000000（Task 1-6 产物）。
  2. ST-Link 把 APP 烧到 0x08010000（链接脚本已改，直接下载即可）。
  3. 上电后 Bootloader 1s 内发 `j` → 串口应见 `F407 App v1.0 Start!`。
  4. 默认路径（magic 校验后自动跳转）的验证并入 Task 8 Step 2：工具升级完成后复位，Bootloader 无需发 `j` 即进入 APP。
  5. 若已接 CAN：PCAN-View 应能继续看到 0x100 心跳。

- [x] **Step 7: Commit**

  ```bash
  git add firmware/master_node
  git commit -m "feat(master): relocate APP to 0x08010000 with VTOR for IAP"
  ```

---

### Task 8: Python 上位机升级工具

**Files:**
- Create: `tools/upgrade_tool.py`

**Interfaces:**
- Consumes: 串口名、bin 文件、Bootloader 协议（Task 5/6 定义）。
- Produces: `python tools/upgrade_tool.py COM3 app.bin` 完成升级（退出码 0=成功，1=失败）。

- [x] **Step 1: 实现工具**

  ```python
  import sys, time, zlib, serial

  ACK, NACK = 0x06, 0x15
  CMD = {"ERASE": 0x01, "WRITE": 0x02, "FINISH": 0x03, "REBOOT": 0x04}
  APP_START = 0x08010000

  def frame(cmd, addr, payload):
      data = bytes([cmd]) + addr.to_bytes(4, "little") + \
             len(payload).to_bytes(2, "little") + payload
      return b"\xA5\x5A" + data + (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "little")

  def send(ser, buf):
      for _ in range(3):                       # 重试 3 次
          ser.write(buf)
          if ser.read(1) == bytes([ACK]):
              return True
          time.sleep(0.5)
      return False

  def main():
      port, path = sys.argv[1], sys.argv[2]
      image = open(path, "rb").read()
      ser = serial.Serial(port, 115200, timeout=1)
      ser.reset_input_buffer()
      ser.write(b"u")                          # 进入升级模式
      time.sleep(0.3)                          # 等 Bootloader 打印横幅并进入协议循环
      ser.reset_input_buffer()                 # 丢弃 "UPGRADE MODE" 横幅，之后只剩协议字节
      if not send(ser, frame(CMD["ERASE"], APP_START, len(image).to_bytes(4, "little"))):
          sys.exit("ERASE failed")
      print("ERASE OK")
      for off in range(0, len(image), 256):
          chunk = image[off:off + 256]
          if not send(ser, frame(CMD["WRITE"], APP_START + off, chunk)):
              sys.exit(f"WRITE failed at 0x{APP_START+off:X}")
          print(f"WRITE 0x{APP_START+off:X} +{len(chunk)}")
      crc = (zlib.crc32(image) & 0xFFFFFFFF).to_bytes(4, "little")
      if not send(ser, frame(CMD["FINISH"], 0, len(image).to_bytes(4, "little") + crc)):
          sys.exit("FINISH failed")
      print("FINISH OK, rebooting...")
      ser.write(frame(CMD["REBOOT"], 0, b""))
      sys.exit(0)

  if __name__ == "__main__":
      main()
  ```

  > 说明：ERASE 帧 payload = 镜像总长度（4B LE）；WRITE 帧 payload = 分块数据；FINISH 帧 payload = 镜像总长度（4B LE）+ CRC32（4B LE），与固件 `BlDispatchFrame` 解析一致。pyserial 安装：`pip install pyserial`。

- [x] **Step 2: 端到端升级验证**

  1. 把 APP 打印改为 `F407 App v1.1 Start!` 并重新编译出 bin。
  2. 运行 `python tools/upgrade_tool.py COM3 app.bin`，全程 ACK。
  3. 复位后串口应见 `F407 App v1.1 Start!`（版本已更新）。

- [x] **Step 3: Commit**

  ```bash
  git add tools/upgrade_tool.py firmware/master_node
  git commit -m "feat(tools): UART IAP upgrade tool with CRC32 verify"
  ```

---

### Task 9: 异常场景 + 回归收尾

**Files:**
- Modify: `docs/学习进度.md`、`README.md`（Phase 5 状态与 Bootloader 说明）
- Modify: `docs/bootloader-plan.md`（标记完成情况）

- [ ] **Step 1: 损坏固件拒绝升级测试**

  对 app.bin 末尾 1 字节取反，重跑工具：FINISH 应 NACK、工具退出码 1；复位后 Bootloader 应打印 `NO APP`（升级流程先擦除旧 APP，且损坏镜像不会写入 magic），重新用完好 bin 升级可恢复。

- [ ] **Step 2: 升级中断测试（可选）**

  升级中途拔串口，复位后 Bootloader 应打印 `NO APP`（ERASE 已执行、magic 未写），不会半砖；再次升级可恢复。

- [ ] **Step 3: 三节点回归**

  F407 新 APP（v1.1）接回 CAN 总线，PCAN-View 应见 0x100 心跳；发 0x201，Node2 串口应打印 `CMD set-speed`（若 12V 未到，验证命令帧链路即可，电机转动延后）。

- [x] **Step 4: 文档更新**

  - `学习进度.md`：第五阶段标记为已完成（或进行中），Day 29-34 填入本计划记录。
  - `README.md`：Phase 5 状态更新 + 指向 `docs/bootloader-plan.md`。
  - 本计划任务勾选全部完成。

- [x] **Step 5: Commit**

  ```bash
  git add docs README.md tools
  git commit -m "docs(bootloader): Phase 5 complete, update progress and README"
  ```

---

## 结论（Task 1 完成后填写）

- 板载芯片丝印：`STM32F407ZGT6`（用户已确认）
- Flash 布局：Bootloader 64KB（0x08000000~0x0800FFFF），APP 960KB（0x08010000~0x080FFFFF）

## 后续可选（不在本计划）

- CAN IAP：把升级通道从 UART 换成 CAN（复用 0x201/0x202 协议家族），作为加分方向。
- 双 Bank 升级（F407 支持）：运行旧 APP 同时写另一 Bank，掉电安全，复杂度高，暂不做。
