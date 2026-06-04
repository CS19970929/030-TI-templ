# 休眠唤醒与灯板复位后处理重构说明

日期：2026-06-04

## 重构目标

本次重构将 STOP 唤醒后的业务判断从 `SleepDeal.c` 移到 MCU 复位后的启动路径：

1. STOP 被唤醒后，`SleepDeal.c` 只做最小 GPIO 采样。
2. 通过 RTC 备份域记录唤醒原因和 SOC 缓存。
3. 立即 `MCU_RESET()`。
4. 复位后由 `LedBar_StartUp()` 处理 SOC 预览、长按开机动画、进水报警或充电唤醒。

这样避免了“复位前先点灯，复位初始化期间灯又灭一段时间”的体验问题，也让休眠/唤醒/LED 的职责边界更清晰。

## 关键行为

- 休眠态 `GAN3/PC13` 闭合后可唤醒 MCU，不要求 `GAN1`、`GAN2` 闭合。
- `GAN3` 唤醒后先复位，再显示备份域缓存 SOC。
- 复位后如果 `GAN3` 未持续按满 3s，则 SOC 预览约 3s 后重新进入深度休眠。
- 只有 `GAN1 && GAN2 && GAN3` 同时满足，且 `GAN3` 持续约 3s，才允许开机。
- `GAN1` 或 `GAN2` 不满足时，`GAN3` 长按不会开启 BMS，也不会播放开机动画。
- 开机动画顺序为：`1`、`12`、`123`、`1234`、`12345`、全灭、显示 SOC。
- 休眠态检测到进水后，`GAN3` 单击不再走 SOC 预览，而是全灯闪烁报警。
- 进水报警状态下，约 2 分钟无操作后灭灯并重新休眠；`GAN2` 断开时立即灭灯并重新休眠。
- 充电器 PA0 唤醒仍要求 `GAN1` 短接，记录为充电唤醒后复位，后续充电逻辑在正常启动路径处理。

## 代码职责

- `SleepDeal.c`
  - STOP 返回后只判断 PA0、PC13、GAN1 和进水输入。
  - 写入 `WAKE_DISPLAY_MODE_SOC_PREVIEW`、`WAKE_DISPLAY_MODE_WATER_ALARM` 或 `WAKE_DISPLAY_MODE_CHARGER_WAKE`。
  - 记录后立即复位，不再调用 LED 预览函数。

- `Flash.c/.h`
  - 扩展 RTC 备份域唤醒显示模式。
  - 新增进水报警唤醒、充电唤醒记录接口。
  - 新增 `WakeDisplayMode_ClearKeepSoc()`，清除唤醒模式但保留 SOC 缓存，避免短按预览后下次 SOC 变成 0。

- `LedBar.c`
  - `LedBar_StartUp()` 成为复位后的唤醒显示分发点。
  - SOC 预览、长按开机判定、进水报警均在复位后执行。
  - 长按开机成立后，先阻塞播放一次开机跑马并保持 SOC 灯亮，后续主循环接管显示。

- `System_Init.c`
  - 补充 `SWT_AD` 输入配置。
  - `InitIO_ganhuangguan()` 中也配置最小水检测 IO，保证休眠唤醒阶段能可靠采样进水状态。

- `main.c`
  - `LedBar_StartUp()` 前移到 `InitIO()` 后，减少复位后到灯板响应的等待时间。

## 构建验证

Keil 目标：`Target 1`

构建命令：

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -b 'E:\TODO\030 + TI\CommomBQ769x0_16series_030C8T6_C.uvprojx' -t 'Target 1' -j0 -o 'E:\TODO\030 + TI\Target 1_build.log'
```

结果：

- `0 Error(s), 35 Warning(s)`
- Program Size：`Code=39656 RO-data=2668 RW-data=668 ZI-data=6356`
- warning 为工程既有的未使用变量、隐式声明、参数类型和驱动库扩展常量提示；本次重构相关文件完整参与 rebuild，未产生 error。

## 上板验证重点

1. 休眠后，`GAN1/GAN2` 均不闭合时，单击 `GAN3` 只预览 SOC 后回休眠；长按 3s 不开机、不跑开机动画。
2. 休眠后，`GAN1 && GAN2` 闭合时，长按 `GAN3` 约 3s 后播放 `1,12,123,1234,12345,全灭,SOC` 动画并正常开机。
3. 休眠后进水，单击 `GAN3` 进入全灯闪烁报警，不显示 SOC。
4. 进水报警中，`GAN2` 断开应立即灭灯并重新休眠。
5. 短按 SOC 预览后再次短按，SOC 显示应保持正确，不应变成 0 或错乱。
6. 充电器 PA0 唤醒在 `GAN1` 短接时应正常进入启动路径，且不受 `GAN2/GAN3` 影响。
