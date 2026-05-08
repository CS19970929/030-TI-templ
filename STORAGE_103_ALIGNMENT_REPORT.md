# 030 存储改造与 103 对齐说明

## 一、背景

当前项目是 `030-TI-templ`，参考项目是 `/Users/cs/Downloads/work/todo/103-309-template`。

本次目标不是机械复制 103 的全部存储结构，而是在保证 030 当前功能稳定的前提下，补齐 030 存储的关键风险点：

1. 内部 Flash 存储区不能被 APP 代码增长覆盖。
2. 可写运行参数写入后必须可靠落盘，不能出现上位机收到成功响应但断电后参数丢失。
3. 运行参数只能有一个事实来源，避免 `EEPROM` 参数和 `g_tParam` 两套参数互相漂移。
4. 日志记录函数不能重复定义。
5. 改动范围要小，优先保证现有功能稳定。

## 二、最终采用方案

采用“保守收口方案”：

- 保留 030 当前已经实现的内部 Flash 双槽 snapshot 机制。
- 保留原有 EEPROM 虚拟地址表和数据格式。
- 不强行引入 103 的完整领域化存储拆分。
- 对 `protect / OtherElement / Heat_Cool_Element` 这类 RW 参数补齐即时保存和失败回滚。
- `g_tParam` 只作为运行镜像，不再单独维护第二套 ParamFlash 持久化区域。
- SOC、日志、Production ID 等已有路径保持原行为，避免扩大风险面。

这样做的好处是：

- 对现有协议、地址、启动读取逻辑影响最小。
- 断电一致性比原来更强。
- 后续如果要继续拆分 SOC、日志、AFE 域，也有清晰边界。

## 三、Flash 空间布局

### 1. APP 区域

Keil 工程文件：

`CommomBQ769x0_16series_030C8T6_C.uvprojx`

原 APP 区域：

```text
StartAddress = 0x08001C00
Size         = 0x0000E400
End          = 0x08010000
```

改造后：

```text
StartAddress = 0x08001C00
Size         = 0x0000B400
End          = 0x0800D000
```

效果：

- APP 链接范围限制到 `0x0800D000` 之前。
- `0x0800D000` 之后留给内部 Flash 存储使用。

### 2. EEPROM snapshot 区域

当前 `EEPROM.c` 使用两个 slot：

```c
#define EEPROM_FLASH_SLOT_A_ADDR     ((UINT32)0x0800D000)
#define EEPROM_FLASH_SLOT_B_ADDR     ((UINT32)0x0800D800)
#define EEPROM_FLASH_SLOT_SIZE       ((UINT32)0x00000800)
#define EEPROM_FLASH_PAGE_SIZE       ((UINT32)0x00000400)
```

每个 slot 包含：

- `EEPROM_FLASH_HEADER`
- `EEPROM_FLASH_PAYLOAD`

写入策略：

- 读取两个 slot。
- 选择 sequence 更新的一份作为当前有效数据。
- 新数据写入另一个 slot。
- 写入后重新读取校验。
- 校验失败触发 `ERROR_EEPROM_STORE`。

## 四、EEPROM 层改造

### 1. 新增统一 RW 参数保存入口

文件：

`Code/Source/EEPROM.c`

新增函数：

```c
UINT8 EEPROM_SaveRWParametersToFlash(void)
```

职责：

1. 将当前运行参数写入 EEPROM shadow：
   - `PRT_E2ROMParas`
   - `OtherElement`
   - `Heat_Cool_Element`
2. 调用 `Param_SyncFromRuntime()` 同步 `g_tParam`。
3. 调用 `EEPROM_SaveSnapshotNow()` 立即保存到内部 Flash。
4. 保存成功后清除 RW 参数写标志：
   - `u32E2P_Pro_VolCur_WriteFlag`
   - `u32E2P_Pro_Temp_WriteFlag`
   - `u32E2P_Pro_Other_WriteFlag`
   - `u32E2P_OtherElement1_WriteFlag`
   - `u32E2P_HeatCool_WriteFlag`

返回值：

```text
1 = 保存成功
0 = 保存失败
```

### 2. App_E2promDeal 保留后台兼容

`App_E2promDeal()` 仍保留旧的后台分步刷写能力，用于兼容原有写标志路径。

新增同步行为：

```c
if (sync_param)
{
    Param_SyncFromRuntime();
}
```

意义：

- 即使未来仍有旧代码只设置写标志，`g_tParam` 也不会长期滞后。
- 当前通信写参数已改成即时保存，正常情况下不会依赖后台延迟刷写。

## 五、通信写参数改造

文件：

`Code/Source/Sci_Upper.c`

### 1. 新增保存失败回滚辅助函数

新增三个 helper：

```c
static UINT8 Sci_SaveProtectOrRollback(const struct PRT_E2ROM_PARAS *backup, struct RS485MSG *s);
static UINT8 Sci_SaveOtherOrRollback(const struct OTHER_ELEMENT *backup, struct RS485MSG *s);
static UINT8 Sci_SaveHeatCoolOrRollback(const struct HEAT_COOL_ELEMENT *backup, struct RS485MSG *s);
```

行为：

1. 调用 `EEPROM_SaveRWParametersToFlash()`。
2. 成功则返回 `1`。
3. 失败则恢复写入前的 RAM 参数。
4. 尝试再次保存恢复后的参数。
5. 设置 Modbus 负响应：

```c
s->AckType = RS485_ACK_NEG;
s->ErrorType = RS485_ERROR_CMD_INVALID;
```

### 2. 已覆盖的 0x10 写参数函数

以下函数已改为“修改 RAM 参数后立即保存，失败回滚”：

- `Sci_WrRegs_0x10_Protect`
- `Sci_WrRegs_0x10_Balance`
- `Sci_WrRegs_0x10_SysOther`
- `Sci_WrRegs_0x10_SleepElement`
- `Sci_WrRegs_0x10_SocElement`
- `Sci_WrRegs_0x10_SystemElement`
- `Sci_WrRegs_0x10_HeatCoolElement`

### 3. 已覆盖的 0x06 恢复默认参数函数

以下函数已改为“恢复默认后立即保存，失败回滚”：

- `Sci_WrReg_0x06_Reset_ProtectElement`
- `Sci_WrReg_0x06_Reset_OtherCanAdd`
- `Sci_WrReg_0x06_Reset_HeatCool`

### 4. 副作用执行顺序

对会触发运行副作用的参数，改为保存成功后再执行副作用：

- `InitShortCur()`
- `reset_sleep_state = 1`
- `SOC_Manager_Init()`
- `SOC_Enhance_Element.u16_RefreshData_Flag = 2`
- `SeriesNum = OtherElement.u16Sys_SeriesNum`
- `g_u32CS_Res_AFE = ...`
- `InitData_Drivers()`

目的：

- 避免参数未落盘但系统已经按新参数运行。
- 保证上位机收到成功响应时，参数已经具备掉电保持能力。

## 六、g_tParam 收口

文件：

`Code/Source/bsp/param.c`

改造后 `param.c` 只保留运行镜像职责：

```c
PARAM_T g_tParam;

void Param_SyncFromRuntime(void)
{
    g_tParam.ParamVer = PARAM_VER;
    g_tParam.protect = PRT_E2ROMParas;
    g_tParam.other = OtherElement;
    g_tParam.heat = Heat_Cool_Element;
}
```

`LoadParam()`：

```c
void LoadParam(void)
{
    sys_time.test_sizeof_g_tParam = sizeof(g_tParam);
    Param_SyncFromRuntime();
}
```

`SaveParam()`：

```c
void SaveParam(void)
{
    Param_SyncFromRuntime();
    (void)EEPROM_SaveRWParametersToFlash();
}
```

删除内容：

- 独立 `ParamFlash` header。
- 独立 `ParamFlash` 双 slot。
- `PARAM_FLASH_SLOT_A_ADDR`
- `PARAM_FLASH_SLOT_B_ADDR`
- `PARAM_ADDR`
- `PARAM_SAVE_TO_FLASH`

这样做后：

- `g_tParam` 不再从第二套 Flash 区读取。
- 参数事实来源统一为：

```text
EEPROM snapshot -> 启动加载 -> PRT_E2ROMParas / OtherElement / Heat_Cool_Element -> g_tParam 镜像
```

## 七、日志记录重复定义清理

原问题：

`EEPROM.c` 和 `LogRecord.c` 都存在事件记录相关实现：

- `EEPROM_ResetData_EventRecord_ToDefault`
- `ReadEEPROM_EventRecord_Parameters`

处理结果：

- 删除 `EEPROM.c` 末尾重复实现。
- 保留 `LogRecord.c` 作为唯一实现。

当前唯一实现位置：

```text
Code/Source/LogRecord.c
```

## 八、与 103 存储方案的差异

### 1. 已对齐部分

已对齐或补齐的能力：

- 存储区和 APP 区域隔离。
- 参数写入具备立即保存语义。
- 参数保存失败会返回错误，不再静默失败。
- `g_tParam` 不再作为独立持久化来源。
- 事件记录不再重复实现。

### 2. 保留差异

030 当前仍保留以下差异，这是稳定性优先下的有意选择：

| 项目 | 030 当前方案 | 103 参考方案 | 当前处理 |
|---|---|---|---|
| 存储介质抽象 | 内部 Flash snapshot + EEPROM 虚拟地址 | 更偏 legacy EEPROM 地址模型 | 保留 030 snapshot |
| RW 参数域 | protect/other/heat 统一即时保存 | 按 legacy 参数域写入 | 已补齐即时保存 |
| SOC 持久化 | 当前已有 SOC 相关改动路径 | 103 有自身 SOC/增强 SOC 地址 | 未强行替换 |
| Production ID | 保留现有写标志路径 | 103 地址类似 | 保持现状 |
| UpgradeParamPolicy | 103 有升级策略标志 | 030 当前没有完整需求 | 未引入 |
| AFE 309 存储项 | 103 有 SH367309 相关地址 | 030 不一定适用 | 未引入 |

## 九、稳定性边界

本次没有做以下高风险迁移：

1. 不把 SOC、日志、Production ID 全部拆成新 domain。
2. 不改变 EEPROM 虚拟地址含义。
3. 不改变上位机寄存器协议。
4. 不改变启动默认值判断策略。
5. 不引入 103 的 AFE/升级策略字段。

原因：

- 这些改动会扩大验证面。
- 当前需求是“简单、方便、稳定、功能即可”。
- 030 当前最大风险是参数落盘一致性和 Flash 空间隔离，已完成。

## 十、验证记录

已执行：

```bash
git diff --check
```

结果：

```text
通过
```

尝试执行：

```bash
arm-none-eabi-gcc -fsyntax-only ...
```

结果：

当前本机 `arm-none-eabi-gcc` 缺少 `stdint.h` 运行时头文件，检查被工具链环境阻断。

本机未找到：

```text
armcc
armclang
UV4
```

因此本次未能执行 Keil 原生完整编译。

## 十一、建议上板验证项

建议按以下顺序验证：

1. 烧录后首次启动，确认无 `ERROR_EEPROM_STORE`。
2. 通过上位机写入 protect 参数，立即断电重启，确认参数保持。
3. 通过上位机写入 balance/sys/sleep/SOC/system 参数，断电重启确认保持。
4. 恢复 protect 默认参数，断电重启确认默认值保持。
5. 恢复 OtherElement 默认参数，确认 `SeriesNum / CS_RES / SOC refresh / sleep reset` 行为正常。
6. 恢复 HeatCool 默认参数，断电重启确认保持。
7. 写入非法参数长度，确认仍返回负响应。
8. 连续多次写参数，确认 Flash slot sequence 正常切换。
9. 触发日志记录，确认 `LogRecord.c` 路径正常。
10. 确认 APP map 文件最终 load 地址仍小于 `0x0800D000`。

## 十二、结论

本次改造后，030 的存储策略已经达到当前目标：

- APP 不会覆盖持久化 Flash。
- RW 参数写入具备立即落盘能力。
- 保存失败具备回滚和负响应。
- `g_tParam` 不再形成第二套参数来源。
- 日志记录实现唯一。
- 与 103 的关键稳定性能力已对齐，但未引入不必要的大范围重构。

当前方案适合作为 030 后续长期维护的稳定基线。
