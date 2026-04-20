# LedBar 按键/动画/休眠联动说明（2026-04-20）

## 本次目标
- 解决开机动画过快问题。
- 解决开机后误触发短按 SOC 循环显示问题。
- 解决关机动画偶现不显示或过快问题（多入口抢占）。

## 改动文件
- `Code/Source/LedBar.c`
- `Code/Source/IO_Control.c`

## 关键策略
1. 按键开关机入口统一到 `LedBar`。
2. `IO_Control` 内 DI1 长按强制休眠入口在 `__FUNC__LED__` 使能时关闭，避免与 `LedBar` 同时处理。
3. 长按触发后先播动画，关机动画结束再写 `Sleep_Mode.bits.b1ForceToSleep_L3 = 1`。

## LedBar 参数（100ms 基准）
- `LEDBAR_LONG_PRESS_TICKS_100MS = 30`（长按 3s）
- `LEDBAR_ANIM_STEP_TICKS_100MS = 3`（动画每步 300ms）
- `LEDBAR_SHORT_SHOW_PHASE_TICKS_100MS = 10`（短按显示相位 1s）
- `LEDBAR_SHORT_SHOW_CYCLE_COUNT = 5`（亮灭 5 轮）
- `LEDBAR_SHORT_BLOCK_AFTER_BOOT_TICKS_100MS = 20`（开机/关机后短按抑制 2s）

## 状态机行为
### 开机动画
- 进入 `LED_UI_BOOT_ANIM` 后，按 `LED1 -> LED5` 逐步点亮。
- 每步间隔 300ms。
- 结束后回 `LED_UI_NORMAL`。

### 关机动画
- 进入 `LED_UI_SHUTDOWN_ANIM` 后，先全亮，再按 `LED5 -> LED1` 逐步熄灭。
- 每步间隔 300ms。
- 动画结束后再触发 `ForceToSleep_L3`。

### 短按 SOC 显示
- 仅在 `LED_UI_NORMAL` 且系统开机状态下触发。
- 1s 亮 + 1s 灭，循环 5 次后退出。
- 长按释放后的那次抬手会被忽略，不会误触发短按。
- 开机/关机动画后有 2s 短按抑制窗口。

## 便于移植的要点
- 只需提供：
  - 开关输入：`MCUI_ENI_DI1`（低有效）
  - 灯输出：`MCUO_SOC_20/40/60/80/100`、`MCUO_SOC_RUN`
  - 开机状态：`System_OnOFF_Func.bits.b1OnOFF_MOS_Relay`
- 若目标工程也有其它 DI 长按关机逻辑，建议同样只保留单一入口，避免动画被提前打断。
