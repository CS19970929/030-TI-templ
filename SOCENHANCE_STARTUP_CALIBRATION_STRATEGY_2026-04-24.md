# SocEnhance 启动校准策略说明

## 修改目标

本次修改把 SOC 启动恢复阶段的校准策略接入到实际生效的 `Code/Source/SocEnhance.c`。

目标是解决启动时直接使用 OCV 导致的 SOC 跳变问题，并支持多种启动校准策略。

## 当前策略

当前在 `SOC_Update_StartUp()` 中支持以下策略：

- `SOC_INIT_STRATEGY_NONE`
- `SOC_INIT_STRATEGY_OCV_DIRECT`
- `SOC_INIT_STRATEGY_OCV_REST_DOWN_STEP`

## 当前默认行为

### 1. 有历史 SOC 快照时

如果启动时成功恢复到历史快照或兼容旧布局快照：

- 优先恢复历史 SOC
- 如果当前满足静置 OCV 条件，则额外进行一次 OCV 校准
- 这次 OCV 校准只允许向下修正
- 每次启动最多只下降 `1%`
- 不允许直接跳到 OCV 目标值
- 如果 OCV 目标值大于等于当前 SOC，则忽略本次校准

这对应策略 `SOC_INIT_STRATEGY_OCV_REST_DOWN_STEP`。

### 2. 没有历史 SOC 时

如果没有可用历史快照：

- 允许直接使用 OCV 初始化 SOC
- 这对应策略 `SOC_INIT_STRATEGY_OCV_DIRECT`

这是因为没有可靠历史基线时，无法做“相对上一次只减 1”的限幅。

### 3. 手动设定 SOC 时

如果本次启动来源是手动设定 SOC：

- 不叠加 OCV 启动校准
- 保持手动值优先

## 代码结构

本次新增了以下入口：

- `SOC_LimitSocValue()`
- `SOC_SetRuntimeSoc()`
- `SOC_SelectStartupCaliStrategy()`
- `SOC_ApplyStartupCaliStrategy()`

这些函数用于把“策略选择”和“实际限幅”拆开，方便后续继续增加别的启动校准来源。

## 当前效果

以历史快照 `60%`、OCV 目标 `52%` 为例：

- 本次启动后 SOC 只会修正到 `59%`
- 不会直接从 `60%` 跳到 `52%`
- 下一次仍然满足静置校准条件时，才可能继续再下降 `1%`

## 构建验证

本次修改已在当前 Keil 目标 `Target 1` 上完成编译验证：

- 编译文件：`SocEnhance.c`
- 结果：`0 Error(s), 0 Warning(s)`

## 后续建议

如果后续还要继续扩展，可以直接在当前策略框架上增加：

- 更严格的静置判定策略
- 不同来源的校准策略选择
- 仅修正 `real` 或同时修正 `real/display` 的分层策略
