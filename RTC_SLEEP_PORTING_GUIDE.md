# RTC 空闲低功耗休眠改造与移植说明

## 目标
在电池空闲且系统正常时，优先进入 RTC 周期低功耗休眠，降低静态功耗；并将策略抽象为可移植钩子。

## 本次改动文件
- `Code/Source/SleepDeal.c`
- `Code/Source/RTC.c`

## 1. 休眠策略改造（SleepDeal）

### 1.1 新增可移植策略钩子
在 `SleepDeal.c` 新增三个函数：
- `SleepDeal_HasBlockingFault()`：是否存在阻塞休眠的异常。
- `SleepDeal_IsIdleCurrent()`：是否满足“空闲电流”。
- `SleepDeal_IsRtcIdleNormal()`：是否满足“空闲且正常，可进入 RTC 周期休眠”。

移植到其他工程时，优先改这三个函数即可。

### 1.2 恢复 L1(RTC) 路径
在 `SleepDeal_Normal_Select()` 中，决策逻辑改为：
1. 空闲且低压（`VCellMin < Vlow`） -> `L3`
2. 空闲且正常（`SleepDeal_IsRtcIdleNormal()`） -> `L1(RTC)`
3. 其余空闲状态 -> `L2`

并在 `App_SleepDeal()` 的状态机中重新启用：
- `SLEEP_HICCUP_NORMAL_L1 -> SleepDeal_Normal_L1()`

## 2. RTC 周期唤醒改造（RTC）

### 2.1 去除固定 30 秒闹钟
`RTC_AlarmConfig()` 原来写死 `+30s`，现改为调用：
- `RTC_BuildAlarmBySleepInterval()`

### 2.2 按参数动态计算下一次闹钟
`RTC_BuildAlarmBySleepInterval()` 使用：
- `OtherElement.u16Sleep_RTC_WakeUpTime`（单位：分钟）

计算方式：
- 读取当前 RTC 时间
- 加上 `interval_min * 60`
- 对 24h 取模，写入 Alarm 的 `Hour/Minute/Second`

当 `u16Sleep_RTC_WakeUpTime == 0` 时，内部兜底为 `1` 分钟，避免无效闹钟。

## 3. 参数建议
- `u16Sleep_TimeRTC`：控制进入 RTC 休眠前的空闲持续时间（min）
- `u16Sleep_RTC_WakeUpTime`：RTC 周期唤醒间隔（min）
- `u16Sleep_VirCur_Chg / u16Sleep_VirCur_Dsg`：空闲电流阈值
- `u16Sleep_VNormal / u16Sleep_Vlow`：L1/L2/L3 电压分段阈值

## 4. 可移植点（最小改动）
迁移到新工程时建议按顺序处理：
1. 对接三类基础数据源：电流、电压、故障。
2. 替换 `SleepDeal_HasBlockingFault()` 内的故障条件。
3. 替换 `SleepDeal_IsIdleCurrent()` 的空闲判据。
4. 保持 `SleepDeal_IsRtcIdleNormal()` 的组合逻辑，按新项目需求微调。
5. 保留 `RTC_BuildAlarmBySleepInterval()` 的“当前时间 + 周期”计算框架。

## 5. 风险与注意
- `RTC` 依赖时钟源稳定（LSE/LSI），若时钟漂移大，周期唤醒可能偏差。
- 若上层频繁设置 `Sleep_Mode` 强制位，会抢占普通休眠路径。
- 现场调参优先顺序：先调电流阈值，再调 `Sleep_TimeRTC`，最后调 `RTC_WakeUpTime`。
