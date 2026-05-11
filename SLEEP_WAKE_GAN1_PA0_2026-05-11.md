# 休眠唤醒 GAN1 与 PA0 逻辑说明

## 目标

- 休眠关机状态下，`GPIO_GAN1` 短接后，PA0 充电信号上升沿可直接唤醒并进入正常启动流程。
- PA0 充电唤醒不再依赖 PC13 开关长按。
- 所有从休眠唤醒后准备进入正常模式的路径，都必须满足 `GPIO_GAN1` 短接；未短接时继续进入 STOP 休眠等待。

## 实现位置

- `Code/Source/SleepDeal.c`

## 关键逻辑

1. `InitWakeUp_Base()` 启用 PA0/EXTI0 上升沿唤醒。
2. `IsSleepWakeupValid()` 先检查 `is_open_gan1()`：
   - 未短接：本次唤醒无效，继续 STOP。
   - 已短接且 PA0 为高：认为是充电唤醒，清除预览状态并进入正常恢复流程。
   - 已短接且 PC13 闭合：保留原有电量预览/长按开机流程。
3. `IsSleepStartUp()` 去掉三个休眠分支中重复的 STOP 等待代码，统一走 `SleepStartup_WaitForWakeup()`。

## 验证建议

- GAN1 未短接时触发 PA0 上升沿，确认系统醒来后继续休眠，不进入正常模式。
- GAN1 已短接时触发 PA0 上升沿，确认无需 PC13 长按即可进入正常模式。
- GAN1 已短接时按 PC13，确认原电量预览和长按开机流程仍有效。
