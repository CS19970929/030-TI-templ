# RTC SOC 校准策略说明

## 本次修改

本次在 `Code/Source/dev/rtc_sleep.c` 中补充了 RTC 静置场景下的 SOC 校准策略框架，核心目标是避免 OCV 校准导致的 SOC 跳变。

当前增加了 3 类策略标识：

- `RTC_SOC_CALI_STRATEGY_NONE`
- `RTC_SOC_CALI_STRATEGY_OCV_DIRECT`
- `RTC_SOC_CALI_STRATEGY_OCV_REST_DOWN_STEP`

当前默认启用的是 `RTC_SOC_CALI_STRATEGY_OCV_REST_DOWN_STEP`。

## 当前默认策略

`RTC_SOC_CALI_STRATEGY_OCV_REST_DOWN_STEP` 的行为如下：

- 校准来源是 OCV 结果
- 静置校准只允许向下修正
- 如果 OCV 结果高于或等于当前显示 SOC，则忽略本次校准
- 如果 OCV 结果低于当前显示 SOC，则每次最多只下降 `1%`
- 不允许直接跳到 OCV 目标值

示例：

- 当前显示 `60`，OCV 结果 `63`，本次不校准，仍然保持 `60`
- 当前显示 `60`，OCV 结果 `59`，本次校准到 `59`
- 当前显示 `60`，OCV 结果 `52`，本次校准到 `59`

## 代码结构

新增了以下几个公共入口，便于后续继续扩展别的静置校准来源：

- `rtc_soc_limit()`
- `rtc_soc_cali_strategy_name()`
- `rtc_soc_select_strategy()`
- `rtc_soc_apply_strategy()`
- `rtc_soc_commit_result()`

这样后续如果要接入别的静置校准来源，只需要：

1. 产出候选 `target_soc`
2. 选择一个策略
3. 复用统一的限幅和提交逻辑

## 当前限制

当前 Keil 工程 `CommomBQ769x0_16series_030C8T6_C.uvprojx` 没有把 `Code/Source/dev/rtc_sleep.c` 编进目标，因此这次修改已经落库，但还没有进入当前固件产物。

本次本地构建验证通过，验证的是当前目标未被这次改动破坏，不代表这套 RTC 校准策略已经进入现网固件。

## 下一步建议

如果要让这套策略真正生效，下一步需要二选一：

- 把 `rtc_sleep.c` 所在 RTC 静置流程正式接入当前工程和实际调用链
- 或者把同样的策略迁移到当前真实生效的休眠/SOC 启动恢复路径
