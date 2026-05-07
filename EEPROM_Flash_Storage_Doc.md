# 旧项目 EEPROM 迁移到内部 Flash 的完整梳理

## 1. 结论

当前旧项目已经把**原外部 EEPROM 芯片的运行时读写路径**切换为**STM32F0 内部 Flash 持久化**。  
对上层来说，`ReadEEPROM_*` / `WriteEEPROM_*` / `InitE2PROM()` / `App_E2promDeal()` 这些接口仍然保留，因此 `SCI_Upper`、`SOC`、`LogRecord`、`ProductionID`、`System_Monitor` 等模块不需要改调用点。

但是，从“完全替换完成、无 bug”这个标准看，当前状态应当更准确地表述为：

- 主存储链路已经完成替换。
- 关键数据已经能在内部 Flash 中读写和恢复。
- SOC 运行状态、事件记录、生产信息、系统开关位都已经接入新的 Flash-backed EEPROM 层。
- 仍有少量 legacy 空实现和历史兼容代码残留，说明工程还没有到“所有历史分支都被彻底清理”的程度。
- 没有完成整工程编译和板级联调，因此不能严谨地宣称“绝对无 bug”。

## 2. 当前存储架构

### 2.1 运行时统一入口

旧项目对外仍然只有一组 EEPROM 风格接口：

- `ReadEEPROM_Byte(UINT16 addr)`
- `WriteEEPROM_Byte(UINT16 addr, UINT8 val)`
- `ReadEEPROM_Word_NoZone(UINT16 addr)`
- `WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)`
- `InitE2PROM()`
- `App_E2promDeal()`

现在这些接口不再访问外部 I2C EEPROM，而是：

1. 先在 RAM 里维护一份 shadow。
2. 写入时只改 RAM shadow，并置 dirty。
3. 到达提交条件后，把整份 shadow 以双槽快照写入内部 Flash。
4. 启动时从 Flash 双槽中恢复最新且 CRC 正确的一份。

### 2.2 Flash 存储分区

目前工程里存在两套独立的内部 Flash 持久化区域：

- 旧 EEPROM 兼容层：`0x0800D000` / `0x0800D800`
- `param.c` 参数层：`0x0800E000` / `0x0800E800`

两者互不重叠，当前没有看到地址冲突。

### 2.3 旧 EEPROM 兼容层覆盖的数据

`EEPROM.c` 的 payload 覆盖了原 EEPROM 地址空间的主干部分，包括：

- 保护参数
- 校准系数
- 一般参数块
- 热管理参数
- 事件记录
- 产品序列号 / 硬件版本 / 软件版本
- 系统功能开关位
- `PASS` / `SLEEP` / `FLASHUPDATE` 等状态字

其中少量散落在高地址上的特殊 word，被单独映射为专用字段处理。

## 3. 读写逻辑

### 3.1 写入链路

典型写入路径是：

1. `SCI_Upper` 或业务模块调用 `WriteEEPROM_Word_NoZone()` / `WriteEEPROM_Byte()`
2. 数据写进 `EEPROM.c` 里的 RAM shadow
3. dirty 标志置位
4. `App_E2promDeal()` 周期性检查 dirty
5. 满足提交延迟后，调用内部 Flash 双槽写入
6. 写入完成后做回读校验

这意味着：

- 上层仍然以为自己在写 EEPROM。
- 实际上写的是内部 Flash 的快照镜像。
- 读写行为对上层是透明的。

### 3.2 读取链路

读取路径是：

1. 上电后 `InitE2PROM()` 执行
2. `EEPROM.c` 先尝试从 A/B 双槽读取最新有效快照
3. 若两槽都无效，则进入默认值初始化
4. 业务模块调用 `ReadEEPROM_Byte()` / `ReadEEPROM_Word_NoZone()`
5. 从 RAM shadow 中直接取值

因此正常运行时，读操作不会直接打到 Flash，而是读 RAM 镜像。

### 3.3 双槽快照格式

`EEPROM.c` 使用了头部 + payload 的双槽格式：

- `magic`
- `storage_version`
- `payload_size`
- `sequence`
- `crc`
- `reserved`

每次提交只写一份完整快照，另一份保留上一个稳定版本。

这样做的意义是：

- 掉电写坏一份时，另一份仍可恢复。
- 通过 sequence 可选择最新版本。
- 通过 CRC 可以识别半写、bit 翻转、异常擦写。

### 3.4 回读验证

写入后会立即回读：

- 先验证整页擦除结果
- 再验证 half-word 编程结果
- 再验证整份 payload 的 CRC 和内容

这一步对掉电和编程异常非常关键，属于旧 EEPROM 方案升级为 Flash 方案后必须补上的可靠性保障。

## 4. SCI_Upper 可读可写现状

### 4.1 已经接入 EEPROM 兼容层的读写对象

这些模块仍然通过旧 EEPROM API 访问数据，但底层已经变成内部 Flash：

- `ProductionID.c`
  - 读写序列号、硬件版本、软件版本
- `System_Monitor.c`
  - 读写 `System_OnOFF_Func`
- `LogRecord.c`
  - 事件记录写入与恢复
- `SocEnhance.c`
  - SOC 严重故障标志、SOC 快照、历史恢复
- `SCI_Upper.c`
  - 校准参数、保护参数、其他参数、系统开关、SOC 相关参数、产品 ID、事件记录

### 4.2 仍然存在的 legacy 空实现

`SCI_Upper.c` 里目前仍有几个写函数是空的：

- `Sci_WrRegs_0x10_SocTable()`
- `Sci_WrRegs_0x10_CopperLoss()`
- `Sci_WrRegs_0x10_RTC()`

这意味着：

- 对外“读”这些数据是成立的。
- “写”这几组数据的代码路径还没有真正落地。

如果你的目标是“所有 SCI 可写参数都闭环”，这部分还需要继续补实现。

## 5. SOC 存储逻辑

### 5.1 SOC 运行态

SOC 相关核心状态主要在 `SocEnhance.c` 中管理，包括：

- 当前 SOC
- 放电 SOC 累计
- 循环次数
- 满充容量
- 起始恢复原因
- 启动策略
- 历史保存序列号

### 5.2 SOC 持久化的真实入口

SOC 的持久化不是直接写外部 EEPROM，而是走旧 EEPROM 接口：

- `ReadEEPROM_Word_NoZone()`
- `WriteEEPROM_Word_NoZone()`

现在这两个接口底层已经是内部 Flash，所以 SOC 的状态也随之变成 Flash 持久化。

### 5.3 SOC 快照内容

`SocEnhance.c` 的 SOC snapshot 主要存：

- `magic`
- `seq`
- `SOC`
- `DSG_SOC_Int`
- `CycleTimes`
- `CapFullAh`
- `checksum`

它还使用了 2 槽机制：

- 两份快照轮换写
- 通过 `seq` 选最新
- 通过校验和确认有效性

### 5.4 SOC 启动恢复流程

启动时，SOC 的恢复逻辑大致是：

1. 读取 `SeriousFaultFlag`
2. 如果是掉电/睡眠类恢复：
   - 优先读最新有效 snapshot
   - 否则尝试 legacy snapshot
   - 再不行就走 OCV 初始估算
3. 如果是数据更新类恢复：
   - 按刷新原因重新构建 SOC 状态
4. 恢复后，如果需要，会把新状态再写回快照

这套逻辑比“只存一个裸 word”更可靠，能抗掉电和半写。

### 5.5 SOC 周期性保存

SOC 不是每次变化都立即落 Flash，而是做了节流：

- 先在 RAM 中累计变化
- `SOC_EEPROM_Deal_Monitor()` 检测到变化后，再延迟若干周期保存
- 只有确认状态稳定后才提交 snapshot

这样做的目的：

- 减少 Flash 擦写次数
- 避免 SOC 连续抖动导致写放大

### 5.6 SOC 仍然需要注意的点

当前 SOC 逻辑已经能工作，但还要注意：

- `SOC_Table_Set`、`CopperLoss`、`CopperLoss_Num` 这类表项的写入路径还没有完整闭环
- 这意味着“SOC 状态快照”是完成的，“SOC 曲线/补偿表的可写持久化”还需要继续补齐

## 6. Flash 寿命分析

### 6.1 现在为什么比原来耐用

现在的写入策略比原 EEPROM 或裸 Flash 直写更耐用，原因是：

- 写前先在 RAM 聚合
- 同值不重复写
- 周期性延迟提交
- 双槽轮换，不在原地反复改同一页
- 写完校验后才接受结果

### 6.2 影响寿命的主要因素

真正影响 Flash 寿命的，不是“是否用了 Flash”，而是“擦写频率”：

- 每次提交都要擦页
- 每次擦页会消耗 Flash erase cycle
- 持续高频写参数会缩短寿命

### 6.3 粗略寿命估算方法

如果内部 Flash 的页擦写寿命按 **10,000 次** 估算，那么：

- 单槽反复写：寿命 = 10,000 次擦写
- 双槽轮换写：每个槽的擦写频率减半，系统寿命约可按 **20,000 次提交** 粗算

换成时间估算：

```text
寿命天数 ≈ 2 * 单页寿命 / 每天提交次数
```

举例：

- 如果平均 1 次提交 / 天，理论寿命非常长
- 如果 100 次提交 / 天，寿命会明显下降

### 6.4 实际工程中的寿命判断

这套方案适合：

- 参数偶发修改
- 上位机偶发配置
- SOC 状态低频保存
- 事件记录低频更新

不适合：

- 高频日志每秒落盘
- 频繁调试写同一组参数
- 轮询式强制保存

如果后续要支撑更高频的持久化，应该继续做：

- 更强的写合并
- 分块存储
- 环形日志化
- 更细粒度 wear leveling

## 7. 当前替换完成度

### 7.1 已经完成的部分

- 外部 EEPROM 芯片运行时访问已被替换
- 旧 EEPROM API 已改成内部 Flash-backed 兼容层
- 保护参数、校准系数、系统开关、产品 ID、事件记录已能读写
- SOC 核心快照已改为内部 Flash 保存
- `param.c` 也已改成独立的内部 Flash 双槽存储

### 7.2 还没完全闭合的部分

- `SCI_Upper` 中 `SocTable` / `CopperLoss` / `RTC` 的写处理函数仍为空
- 这些模块虽然“能读”，但“能写并持久化”还没完全闭环
- `EEPROM.h` 中仍有外部 EEPROM 时代的宏和注释残留
- `SOC_Table_Set`、`CopperLoss`、`CopperLoss_Num` 是否需要启动时从 Flash 反向恢复，还没有做成完整统一的加载逻辑

### 7.3 结论

所以，严格来说：

- **不是“完全没有遗留点”的最终状态**
- 但已经是**主存储路径完成替换、并且可工作**的状态

## 8. 风险点

1. **未完成整工程编译验证**
   - 当前只是基于代码审视和局部路径梳理。
   - 没有证明所有编译单元都在真实工程里无误通过。

2. **写后延迟提交存在小窗口**
   - 由于先写 RAM shadow，再延迟提交 Flash，掉电时可能丢失最近一次未提交修改。
   - 这是典型 write-back 策略的代价。

3. **历史兼容代码残留**
   - 外部 EEPROM 时代的宏、注释、命名仍然很多。
   - 功能上已经切换，但代码可读性还有清理空间。

4. **部分 SCI 写接口未补齐**
   - `SocTable` / `CopperLoss` / `RTC` 的写路径如果后续要对外开放，需要继续实现。

## 9. 建议的下一步

1. 把 `SCI_Upper` 里空的写函数补齐，统一让 SOC 表、补偿表也能真正写入 Flash-backed EEPROM。
2. 给 `SOC_Table_Set`、`CopperLoss`、`CopperLoss_Num` 增加启动加载逻辑，避免重启后变成未定义值。
3. 做一次整工程编译和一次板级实测，重点看：
   - 上电恢复
   - 掉电恢复
   - 上位机写参数
   - 事件记录连续写入
   - SOC 恢复与保存
4. 后续如果要继续简化架构，可以把 `param.c` 与 legacy EEPROM 兼容层进一步收敛成单一持久化体系。

