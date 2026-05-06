# STORAGE_FLASH_MIGRATION

项目：`CommomBQ769x0_16series_030C8T6_C.uvprojx`

目标：
- 旧外部 EEPROM / EEPROM 风格接口迁移到内部 Flash
- 休眠启动标志迁移到备份域寄存器
- 保留原有通信写参数流程，但持久化落到内部 Flash

## 1. 结论

本次改造把原来的 EEPROM 持久化拆成两类：

1. **内部 Flash 持久化**
   - 采用 `双快照槽 + journal`。
   - 快照用于低频配置和首次恢复。
   - journal 用于通信写入、事件记录、SOC/参数增量更新。
   - 所有写入都带 `magic + version + length + sequence + crc` 校验。

2. **备份域寄存器持久化**
   - 休眠启动标志不再走 Flash / EEPROM。
   - 使用 `BKP1/BKP2` 存 boot flag，`BKP3/BKP4` 存唤醒显示状态。
   - 采用 `value + inverse`，读取时做 XOR 校验。

Keil 构建已验证通过，当前工程可正常链接生成 `AXF/HEX/BIN`。

## 2. 旧 EEPROM 地址梳理

### 2.1 地址块分类

| 旧地址 / 区间 | 逻辑内容 | 典型读写入口 / 写标志 | 新归属 |
|---|---|---|---|
| `0x0000 - 0x0080` | 保护参数，65 个 word | `u32E2P_Pro_VolCur_WriteFlag` / `u32E2P_Pro_Temp_WriteFlag` / `u32E2P_Pro_Other_WriteFlag`，`ReadEEPROM_Word_NoZone()` / `WriteEEPROM_Word_NoZone()` | 内部 Flash 快照 + journal |
| `0x0082 - 0x0098` | RTC 参数，12 个 word | `u32E2P_RTC_Element_WriteFlag` | 内部 Flash 快照 + journal |
| `0x009A / 0x00F8` | K/B 校准值（K、B 两组） | `u8E2P_KB_WriteFlag`，通信命令写入后由 `WriteEEPROM_ByteData_Circle()` 分步落盘 | 内部 Flash 快照 + journal |
| `0x0156 / 0x01AA / 0x01CA` | SOC table / copper loss | `u8E2P_SocTable_WriteFlag`、`u8E2P_CopperLoss_WriteFlag` | 内部 Flash 快照 + journal |
| `0x01EA - 0x02A2` | 故障记录 / 事件环形记录 | `LogRecord.c`，通过 `WriteEEPROM_Word_NoZone()` 追加/清空 | 内部 Flash journal |
| `0x02A4 起` | `OtherElement1` | `u32E2P_OtherElement1_WriteFlag` | 内部 Flash 快照 + journal |
| `0x02E4 起` | `Heat_Cool` | `u32E2P_HeatCool_WriteFlag` | 内部 Flash 快照 + journal |
| `0x0316 起` | SOC 扩展参数 | `SocEnhance.c` | 内部 Flash 快照 + journal |
| `0x033E / 0x0366 / 0x038E` | 序列号 / 硬件版本 / 软件版本 | `ProductionID.c` | 内部 Flash 快照 + journal |
| `0x03E8 - 0x04B0` | 历史事件记录 / 事件点位 | `LogRecord.c` | 内部 Flash journal |
| `0x07F8 / 0x07FC` | 开关机 / 系统功能选择 | `System_Monitor.c`、`Sci_Upper.c` 直接写 `EEPROM_ADDR_SWITCH_ONOFF` / `EEPROM_ADDR_SYS_FUNC_SELECT` | 内部 Flash 快照 + journal |
| `0x3FFA` | 旧 sleep 标志 | 旧 `EEPROM_ADDR_SLEEP` 逻辑地址 | 兼容保留，不再是主休眠来源 |
| `0x3FFC` | PASS 标志 | `EEPROM_ADDR_PASS` | 内部 Flash 特殊字 |
| `0x3FFE` | Flash update 标志 | `EEPROM_ADDR_FLASHUPDATE` | 内部 Flash 特殊字 |

说明：
- `EEPROM_ADDR_SLEEP` 仍保留为兼容逻辑地址，但**主休眠启动不再读取它**，现在由备份域 `BootFlag_*` 接管。
- `EEPROM_ADDR_PASS` / `EEPROM_ADDR_FLASHUPDATE` 继续作为逻辑状态位使用，但底层改为内部 Flash 特殊字存储。
- 旧的 `dev/rtc_sleep.c`、`dev/rtc_sleep copy.c` 仍有历史引用，但它们不在当前 Keil Target 中。

### 2.2 旧读写函数与新后端

| 旧函数 | 现状 |
|---|---|
| `ReadEEPROM_Byte()` | 读内部 Flash 的逻辑视图，优先 journal，再回退快照 |
| `WriteEEPROM_Byte()` | 写入 journal，必要时触发快照压缩 |
| `ReadEEPROM_Word_NoZone()` | 读内部 Flash 的逻辑视图 |
| `WriteEEPROM_Word_NoZone()` | 写入 journal，必要时触发快照压缩 |
| `InitE2PROM()` | 先初始化内部 Flash 持久化层，再进入原有参数装载流程 |
| `WriteEEPROM_ByteData_Circle()` | 保留为统一的“脏标志顺序落盘”入口 |

### 2.3 旧写标志分类

| 写标志 | 对应内容 | 触发模块 |
|---|---|---|
| `u32E2P_Pro_VolCur_WriteFlag` | 保护电压/电流参数 | `EEPROM.c`、`Sci_Upper.c` |
| `u32E2P_Pro_Temp_WriteFlag` | 保护温度参数 | `EEPROM.c`、`Sci_Upper.c` |
| `u32E2P_Pro_Other_WriteFlag` | 其他保护参数 | `EEPROM.c`、`Sci_Upper.c` |
| `u32E2P_RTC_Element_WriteFlag` | RTC 相关参数 | `EEPROM.c` |
| `u8E2P_KB_WriteFlag` | K/B 校准对 | `EEPROM.c`、`Sci_Upper.c` |
| `u8E2P_SocTable_WriteFlag` | SOC 表 | `EEPROM.c`、`SocEnhance.c` |
| `u8E2P_CopperLoss_WriteFlag` | 铜损补偿 | `EEPROM.c`、`SocEnhance.c` |
| `u32E2P_OtherElement1_WriteFlag` | `OtherElement1` | `EEPROM.c` |
| `u32E2P_HeatCool_WriteFlag` | 热/冷控制参数 | `EEPROM.c` |

## 3. 新 Flash 存储布局

### 3.1 页布局

Flash 页面大小：`1KB`

应用程序结束地址约在 `0x0800C390`，因此从页面边界 `0x0800C400` 开始划分持久化区域。

| 区域 | 起始地址 | 大小 | 页数 | 作用 |
|---|---:|---:|---:|---|
| Snapshot slot 0 | `0x0800C400` | `3KB` | 3 | 第一份完整快照 |
| Snapshot slot 1 | `0x0800D000` | `3KB` | 3 | 第二份完整快照 |
| Journal | `0x0800DC00` | `5KB` | 5 | 增量写入日志 |

补充：
- `FLASH_ADDR_TEST_BMS_PARAM` 现已别名到 `FLASH_ADDR_STORAGE_SLOT0`
- `FLASH_ADDR_WAKE_TYPE`、`FLASH_ADDR_UPDATE_FLAG` 仍保留在 `0x0800F400` / `0x0800F800`

### 3.2 记录格式

#### Snapshot

Snapshot header：
- `magic = 0xE2F1`
- `version = 0x0001`
- `length = 1027 words`
- `sequence`
- `crc16`
- `commit = ~magic`

Payload：
- 低地址区 `0x0000 - 0x07FF`：1024 个 word
- 特殊字：
  - `EEPROM_ADDR_SLEEP`
  - `EEPROM_ADDR_PASS`
  - `EEPROM_ADDR_FLASHUPDATE`

#### Journal

Journal header：
- `magic = 0xE2F2`
- `version = 0x0001`
- `length = 2552 words`
- `base sequence`
- `crc16`
- `commit = ~magic`

Journal entry：
- `sequence`
- `addr | byte_flag`
- `value`
- `crc16(sequence, addr, value)`
- `commit = 0xA55A`

规则：
- `addr` 的 bit15 作为 byte 写标志
- 所有读取都先看最新 journal，再回退 snapshot
- journal 满了以后，先压缩到另一份 snapshot，再清空 journal 重建 header

## 4. 备份域寄存器

| 寄存器 | 内容 | 校验方式 | 说明 |
|---|---|---|---|
| `BKP1` | boot flag | `BKP1 ^ BKP2 == 0xFFFF` | 休眠启动状态 |
| `BKP2` | `~boot flag` | 同上 | 反码校验 |
| `BKP3` | `wake display value` | `BKP3 ^ BKP4 == 0xFFFFFFFF` | `mode << 16 | soc` |
| `BKP4` | `~wake display value` | 同上 | 反码校验 |

#### Boot flag 值

| 名称 | 值 |
|---|---:|
| `FLASH_NORMAL_SLEEP_VALUE` | `0x1234` |
| `FLASH_HICCUP_SLEEP_VALUE` | `0x1235` |
| `FLASH_DEEP_SLEEP_VALUE` | `0x1236` |
| `BOOT_FLAG_RESET_VALUE` | `0xFFFF` |

#### Wake display

- `WAKE_DISPLAY_MODE_NONE`
- `WAKE_DISPLAY_MODE_SOC_PREVIEW`
- `WAKE_DISPLAY_MODE_BOOT_SEQUENCE`

`WakeDisplayState_CaptureForBoot()` 会把 `mode/soc` 从备份域读回到 RAM shadow，`WakeDisplayState_Clear()` 再清空。

## 5. 通信写参数路径

### 5.1 运行时落盘路径

1. 通信层在 `Sci_Upper.c` 修改 RAM 中的参数结构体
2. 对应写标志置位
3. 主循环调用 `WriteEEPROM_ByteData_Circle()`
4. `WriteEEPROM_ByteData_Circle()` 每次只写一个最小粒度条目，避免一次性大块擦写
5. `WriteEEPROM_Word_NoZone()` / `WriteEEPROM_Byte()` 现在会直接进入内部 Flash journal

### 5.2 关键入口

| 模块 | 作用 |
|---|---|
| `Sci_Upper.c` | 通信写参数、默认值恢复、`System_OnOFF_Func` 选择写回 |
| `EEPROM.c` | dirty flag 顺序写、首次量产初始化、读回校验 |
| `System_Monitor.c` | 系统功能选择参数的读取 / 写回 |
| `ProductionID.c` | 序列号 / 硬件版本 / 软件版本的初始化与写回 |
| `LogRecord.c` | 事件记录追加、清空、恢复 |
| `SocEnhance.c` | SOC 相关状态和持久化参数 |

### 5.3 默认值和复位

- `InitProID()`：先校验长度，再加载序列号/硬件版本/软件版本
- `WriteProID_Default()`：首次上电写默认生产信息
- `InitSystemMonitorData_EEPROM()`：优先读取 `EEPROM_ADDR_SYS_FUNC_SELECT`
- `SystemMonitorResetData_EEPROM()`：恢复默认系统功能位后写回 Flash

## 6. 验证结果

### 6.1 构建环境

- 工程：`CommomBQ769x0_16series_030C8T6_C.uvprojx`
- Target：`Target 1`
- 芯片：`STM32F030C8`
- 工具链：`ARMCC`
- Keil：`MDK-ARM 5.32.0.0`

### 6.2 构建结论

- 结果：成功
- 错误：`0`
- 警告：`49`
- 耗时：`00:00:04`
- 产物：`AXF / HEX / BIN` 均生成

### 6.3 资源占用

- `Code = 41296`
- `RO-data = 2436`
- `RW-data = 676`
- `ZI-data = 6396`
- `Flash ≈ 43.4 KB`
- `RAM ≈ 6.9 KB`

### 6.4 备注

- `EEPROM.c` 已不再保留整块 2KB shadow RAM
- 本次新增 warning 已清零；剩余 warning 主要来自工程中原有的历史代码
- 旧的 `dev/rtc_sleep.c` / `dev/rtc_sleep copy.c` 仍保留历史引用，但不参与当前 target 构建

## 7. 迁移决策总结

- 低频配置：内部 Flash 双快照槽
- 高频增量/事件：内部 Flash journal
- 休眠启动标志：备份域寄存器
- 唤醒显示状态：备份域寄存器 + RAM shadow
- 旧 EEPROM 接口：保留逻辑 API，底层已切换到内部 Flash
