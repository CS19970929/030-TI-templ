# 当前 LED、干簧管、休眠唤醒逻辑梳理

日期：2026-06-09

本文只描述当前源码与 Keil 工程实际包含的逻辑，不沿用历史文档里的预期行为。当前工程文件 `CommomBQ769x0_16series_030C8T6_C.uvprojx` 包含 `SleepDeal.c`、`LedBar.c`、`gan_huang_guan_logi.c`、`IO_Control.c`、`System_Init.c`、`Flash.c` 等主路径文件；`Code/Source/dev/rtc_sleep.c` 与 `rtc_sleep copy.c` 不在当前工程文件列表内，本文不把它们作为运行逻辑。

## 关键结论

1. 运行态干簧管业务由 `App_MOS_Relay_Ctrl()` 每 10ms 间接调用 `ganhuangguan_Logi()`。
2. 运行态 `GAN3` 长按关机当前不走 `gan_huang_guan_logi.c` 里的 `Gan_ProcessTopKey()`，而是走 `TIM17_IRQHandler()` 中的 `key_can()`。
3. 休眠进入不是 `entersleep()` 直接 STOP，而是先置 `Sleep_Mode` 强制休眠位，再由 `App_SleepDeal()` 写 BootFlag、保存 SOC、关闭 AFE、复位，复位后进入 STOP 等待唤醒。
4. STOP 唤醒后 `IsSleepWakeupValid()` 当前会在记录唤醒显示模式后立即 `MCU_RESET()`，正式的 SOC 预览、长按开机判定和水报警显示都在复位后的 `LedBar_StartUp()`/主循环中处理。
5. 当前代码与历史需求有明显差异：休眠态 `GAN3` 长按开机阈值是 2s，不是 3s；LED 动画步进是 100ms，不是 200ms；开机动画没有逐灯熄灭阶段。

## 主要文件分工

| 文件 | 当前职责 |
| --- | --- |
| `Code/Source/main.c` | 初始化顺序、主循环调度；`APP_LedBar()` 在 `App_SleepDeal()` 后执行。 |
| `Code/Source/System_Init.c` | GPIO 初始化、系统节拍、`key_can()` 运行态长按关机扫描、TIM17 中断。 |
| `Code/Source/IO_Control.c` | 10ms 驱动控制入口；调用 `ganhuangguan_Logi()`，再用保护条件覆盖驱动输出。 |
| `Code/Source/gan_huang_guan_logi.c` | `GAN1/GAN2`、PA0 充电锁存、进水、放电保持、业务休眠请求。 |
| `Code/Source/LedBar.c` | LED GPIO 输出、SOC 显示、充电动画、进水闪烁、开机/关机动画、复位后唤醒显示分发。 |
| `Code/Source/SleepDeal.c` | 运行态休眠状态机、BootFlag 写入、STOP 入口、STOP 返回后的唤醒源判定。 |
| `Code/Source/Flash.c` | RTC 备份寄存器封装：BootFlag 与 WakeDisplayState。 |
| `Code/Drivers/stm32f0xx_it.c` | EXTI0/EXTI13 等中断只做清标志和少量标志置位；真正业务在 STOP 返回后轮询 GPIO。 |

## 信号定义

| 信号 | 引脚 | 有效条件 | 当前用途 |
| --- | --- | --- | --- |
| `GAN1` | `GPIOA/PIN4` | GPIO 读 0 | 浮动连接器/插头在位；充电锁存前提、放电前提、开机前提、断开 0.5s 休眠。 |
| `GAN2` | `GPIOA/PIN5` | GPIO 读 0 | 把手在位；放电前提、开机前提、断开 2s 休眠。 |
| `GAN3` | `GPIOC/PIN13` | GPIO 读 0 | 休眠态唤醒 EXTI13、复位后 SOC 预览/长按开机判定、运行态长按关机扫描。 |
| `GAN4` | `GPIOA/PIN6` | GPIO 读 0 | 当前只提供 `is_open_gan4()` 读取接口，主业务未使用。 |
| `PA0` | `GPIOA/PIN0` | GPIO 读 1 | 充电器插入/唤醒信号；STOP 唤醒 EXTI0；运行态充电锁存触发源之一。 |
| `SWT_AD` | `GPIOB/PIN9` | GPIO 读 0 | 进水检测。 |
| `SWT_EN` | `GPIOF/PIN6` | `Bit_SET` 使能，`Bit_RESET` 关闭 | 水检测使能；LED 水报警开启时会拉低。 |

## 调度与周期

启动顺序：

1. `main()` 调 `InitDevice()`。
2. `InitDevice()` 中先 `InitDelay()`，再 `IsSleepStartUp()`。
3. 如果没有休眠 BootFlag，继续 `InitIO()`。
4. `__FUNC__LED__` 已打开，因此调用 `LedBar_StartUp()`，然后置 `sys_time.key_irq_scan_enable = true`。
5. `InitTimer()` 启动 TIM17。
6. 后续初始化唤醒 AFE、EEPROM、串口、ADC、SOC、参数、AFE、MOS 等。
7. `InitDevice()` 返回后 `InitVar()` 初始化系统开关与启动标志。

主循环顺序中，与本文相关的调用关系如下：

```text
App_SysTime()
App_MOS_Relay_Ctrl()
  -> RefreshData_Drivers()
       -> ganhuangguan_Logi()
App_SleepDeal()
APP_LedBar()
App_ChargerLoad_Det()
App_LogRecord()
```

周期关系：

- `App_MOS_Relay_Ctrl()` 依赖 `b1Sys10msFlag1`，干簧管业务约 10ms 跑一次。
- `APP_LedBar()` 依赖 `b1Sys100msFlag`，LED 运行态状态机约 100ms 跑一次。
- `key_can()` 在 TIM17 中断里约 10ms 跑一次，用于运行态 `GAN3` 长按关机扫描。
- `App_SleepDeal()` 普通休眠按 1s 节拍推进；强制休眠位存在时绕过 1s 节拍限制。

## RTC 备份域状态

`Flash.c` 当前使用 RTC 备份寄存器保存两组状态：

| 状态 | 寄存器 | 校验 | 用途 |
| --- | --- | --- | --- |
| `BootFlag` | `BKP1R/BKP2R` | 值 + 16 位反码 | 标记复位后是否进入休眠启动路径。 |
| `WakeDisplayState` | `BKP3R/BKP4R` | 32 位值 + 反码 | 高 16 位为唤醒显示模式，低 16 位为休眠前缓存 SOC。 |

`BootFlag` 当前取值：

| 宏 | 值 | 含义 |
| --- | --- | --- |
| `FLASH_NORMAL_SLEEP_VALUE` | `0x1234` | 普通低功耗睡眠路径。 |
| `FLASH_DEEP_SLEEP_VALUE` | `0x1235` | 深度休眠路径。 |
| `FLASH_HICCUP_SLEEP_VALUE` | `0x1236` | RTC/打嗝休眠路径。 |
| `FLASH_SLEEP_RESET_VALUE` | `0xFFFF` | 非休眠启动或已清除。 |

`WakeDisplayState` 当前模式：

| 宏 | 值 | 当前来源/用途 |
| --- | --- | --- |
| `WAKE_DISPLAY_MODE_NONE` | `0x0000` | 无复位后 LED 特殊动作；低 16 位仍可保留 SOC。 |
| `WAKE_DISPLAY_MODE_SOC_PREVIEW` | `0x0001` | STOP 返回后未判定为水报警或有效充电唤醒时写入；复位后做 SOC 预览和长按开机判定。 |
| `WAKE_DISPLAY_MODE_BOOT_SEQUENCE` | `0x0002` | 兼容入口；当前源码没有实际调用 `WakeDisplay_RequestBootSequence()`。 |
| `WAKE_DISPLAY_MODE_WATER_ALARM` | `0x0003` | STOP 返回后检测到进水时写入；复位后 LED 进入水报警闪烁。 |
| `WAKE_DISPLAY_MODE_CHARGER_WAKE` | `0x0004` | STOP 返回后 `PA0` 有效且 `GAN1` 闭合时写入；复位后清模式并正常启动。 |

`WakeDisplaySocCache_Write()` 在进入休眠前写 `NONE + 当前 SOC`。后续 `WakeDisplay_RequestSocPreview()`、`WakeDisplay_RequestWaterAlarm()`、`WakeDisplay_RequestChargerWake()` 会保留低 16 位 SOC，只改高 16 位模式。`WakeDisplayMode_ClearKeepSoc()` 也只清模式，不清 SOC。

## LED 灯板逻辑

### GPIO 与 SOC 映射

`LedBar_SetByMask()` 使用 5 位 mask 控制 5 个 SOC LED：

| mask 位 | LED 宏 | 引脚 |
| --- | --- | --- |
| bit0 | `MCUO_SOC_20` | `PB8` |
| bit1 | `MCUO_SOC_40` | `PB7` |
| bit2 | `MCUO_SOC_60` | `PB13` |
| bit3 | `MCUO_SOC_80` | `PB12` |
| bit4 | `MCUO_SOC_100` | `PB5` |

SOC 显示规则：

- SOC 被钳位到 0..100。
- 即使 SOC 为 0，也会点亮 bit0，也就是至少亮第一格。
- `>=20` 亮第二格，`>=40` 亮第三格，`>=60` 亮第四格，`>=80` 亮第五格。

### LED UI 模式

`s_led_ui_mode` 当前状态：

| 状态 | 含义 | 进入方式 | 退出方式 |
| --- | --- | --- | --- |
| `LED_UI_NORMAL` | 常态显示分发 | 初始化、动画结束 | 由请求函数切换到其他模式。 |
| `LED_UI_SOC_TEMP` | 临时 SOC 显示 | `LedBar_RequestSocTemporary()` | 约 3s 后灭灯回常态。当前运行路径没有有效调用者。 |
| `LED_UI_BOOT_ANIM_ON` | 非阻塞开机逐灯点亮 | `LedBar_RequestBootAnimation()` | 5 步后转 `LED_UI_BOOT_ANIM_OFF`。 |
| `LED_UI_BOOT_ANIM_OFF` | 开机动画收尾 | `LED_UI_BOOT_ANIM_ON` 结束 | 当前不逐灯熄灭，只切回常态并显示实时 SOC。 |
| `LED_UI_SHUTDOWN_ANIM` | 关机动画 | `LedBar_RequestShutdownAnimation()` | 逐灯熄灭后调用 `entersleep(DEEP_MODE)`。 |

### 时间常量

| 宏 | 当前值 | 实际时间 |
| --- | --- | --- |
| `LEDBAR_SOC_TEMP_TICKS_100MS` | `30` | 3s |
| `LEDBAR_CHG_BLINK_TICKS_100MS` | `5` | 500ms |
| `LEDBAR_WATER_BLINK_TICKS_100MS` | `2` | 200ms |
| `LEDBAR_ANIM_STEP_TICKS_100MS` | `1` | 100ms |
| `LEDBAR_PREBOOT_POWERON_TICKS_10MS` | `200` | 2s |
| `LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS` | `300` | 3s |

注意：历史需求里常见的“3s 长按开机”和“0.2s 跑马步进”与当前代码不一致。当前复位后长按开机阈值为 2s，LED 动画步进为 100ms。

### 常态显示分发

`APP_LedBar()` 的执行顺序：

1. 非 100ms 节拍直接返回。
2. `SystemStatus.bits.b1StartUpBMS` 为 1 时直接返回，启动未完成期间不跑运行态 LED。
3. 如果 `s_water_alarm_enable` 为 1 且不是关机动画，则全灯按 200ms 闪烁并返回。
4. 优先处理 `s_led_ui_mode` 中的临时 SOC、开机动画、关机动画。
5. 若 `sleep_reason == 1`，直接灭灯返回。
6. 若充电显示使能或 `LedBar_Command == LED_BAR_CHG`，显示充电动画。
7. 若放电显示使能或 `LedBar_Command == LED_BAR_NORMAL`：
   - `s_discharge_display_enable == 1` 时显示 SOC/故障；
   - 否则灭灯。
8. 兜底灭灯。

### 充电显示

`LedBar_ShowCharge()` 当前行为：

- SOC `>=99` 时 5 灯常亮。
- SOC `<99` 时，已完成的 20% 档位常亮，下一档按 500ms 闪烁。
- 例如 SOC 35：第一格常亮，第二格闪烁。

### 故障显示

`LedBar_Show_Normal()` 中如果满足以下任一条件，则进入 `LedBar_Show_Fault()`：

- `g_stCellInfoReport.unMdlFault_Third.all & 0x2FFA` 非 0。
- `ERROR_STATUS_TEMP_BREAK`。
- `ERROR_STATUS_CBC_DSG`。

故障显示当前只翻转第一格，其他 4 格关闭。由于 `APP_LedBar()` 100ms 运行一次，第一格闪烁周期约 200ms。

### 关机动画

运行态长按关机或测试入口会调用 `LedBar_RequestShutdownAnimation()`：

1. 立即点亮 5 灯。
2. 每 100ms 右移一次 mask：`11111 -> 01111 -> 00111 -> 00011 -> 00001 -> 00000`。
3. 清动画状态、关闭充放电显示。
4. 调用 `entersleep(DEEP_MODE)`。

这里实际效果是从最高位 LED 开始逐个熄灭，约 0.5s 熄完，随后进入休眠请求流程。

### 开机动画

当前有两套开机动画入口：

1. `LedBar_RequestBootAnimation()`：非阻塞入口，仅在 `WAKE_DISPLAY_MODE_BOOT_SEQUENCE` 分支使用，但当前没有实际写入该模式的调用者。
2. `LedBar_RunBootAnimationBlocking()`：复位后 SOC 预览长按开机成功时使用。

当前实际休眠长按开机走阻塞动画：

```text
1 -> 12 -> 123 -> 1234 -> 12345 -> 全灭 500ms -> 缓存 SOC
```

当前没有 `LED5 -> LED1` 逐灯熄灭阶段，也没有动画后再显示 3s 再灭灯的阶段；动画后会保持 SOC，后续由正常运行态逻辑接管。

## 运行态干簧管逻辑

运行态入口是：

```text
App_MOS_Relay_Ctrl()
  -> RefreshData_Drivers()
       -> ganhuangguan_Logi()
```

`RefreshData_Drivers()` 的顺序很重要：

1. 先把 `Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag` 置为 `FORCE_KEEP_MODE`。
2. 调用 `ganhuangguan_Logi()`。
3. 再检查保护条件，若存在保护、AFE/EEPROM 通讯错误、CBC 错误、温度断线、进水等，强制覆盖为 `FORCE_CLOSE_MODE`。

因此，干簧管业务可以请求打开/保持驱动，但最终保护条件有更高优先级。

### `ganhuangguan_Logi()` 当前优先级

1. 若 `s_sleep_requested` 已置位，只保持驱动关闭并返回。
2. 若 `is_water_in()` 为真：
   - 清充电锁存；
   - 执行进水逻辑；
   - 直接返回。
3. 当前 `Gan_ClearWater()` 被注释，没有在未进水时清水报警或清水计时。
4. 执行充电逻辑 `Gan_ProcessCharge()`。
5. 当前 `Gan_ProcessTopKey()` 被注释，运行态 `GAN3` 业务不在这里处理。
6. 若充电路径已激活，直接返回。
7. 否则执行放电逻辑 `Gan_ProcessDischarge()`。

### 进水逻辑

触发条件：`SWT_AD` 读 0。

动作：

- 强制关闭驱动。
- 关闭充电显示和放电显示。
- 开启 LED 水报警闪烁。
- 如果 `GAN1` 或 `GAN2` 任一断开，立即请求休眠。
- 如果 `GAN1 && GAN2` 都闭合，进水持续约 120s 后请求休眠。

当前注意点：

- `Gan_ClearWater()` 没有被调用，因此一旦 `LedBar_SetWaterAlarm(1)` 打开水报警，后续水信号恢复正常时，运行态不会在 `ganhuangguan_Logi()` 中主动清除水报警、恢复 `SWT_EN` 或清 `s_water_ticks`。
- 保护覆盖中仍会在 `is_water_in()` 为真时强制关闭驱动。

### 充电逻辑

充电锁存进入条件：

```text
GAN1 闭合 && (PA0 为高 || 充电电流有效)
```

充电电流有效条件：

```text
g_stCellInfoReport.u16Ichg > max(OtherElement.u16Sleep_VirCur_Chg, 2)
```

锁存后行为：

- `GAN1` 断开：清充电锁存、关闭充电显示、请求深度休眠。
- `GAN1` 保持闭合：保持驱动输出，关闭放电显示。
- 有充电电流、SOC 已满、或最高单体电压达到 `OtherElement.u16Soc_V_100`：保持充电锁存并显示充电动画。
- 充电电流消失、SOC 未满、最高单体电压未达到满电阈值：持续约 1s 后认为充电器丢失，清锁存并请求休眠。

这个策略意味着 PA0 只作为插入触发/唤醒依据，不作为 CHG MOS 打开后的长期在线依据。

### 放电逻辑

非进水、非充电时走 `Gan_ProcessDischarge()`：

- 先置 `LedBar_Command = LED_BAR_NORMAL` 并关闭充电显示。
- `GAN1` 断开：
  - 关闭放电显示；
  - 计时 50 个 10ms，约 0.5s 后请求休眠。
- `GAN1` 闭合、`GAN2` 断开：
  - 保持驱动输出；
  - 计时 200 个 10ms，约 2s 后关闭放电显示并请求休眠。
- `GAN1 && GAN2` 都闭合：
  - 清断开计时；
  - 保持驱动输出；
  - 开启放电显示，LED 常态显示 SOC/故障。

### 运行态 `GAN3`

`gan_huang_guan_logi.c` 中保留了 `Gan_ProcessTopKey()`，其设计行为是：

- 1s 显示临时 SOC。
- 3s 且 `GAN1 && GAN2` 闭合时请求关机动画。
- 充电中和关机动画中不响应。

但当前 `ganhuangguan_Logi()` 中调用被注释：

```c
// Gan_ProcessTopKey(gan1_on, gan2_on, charge_path_active);
```

因此，以上设计行为当前不生效。当前运行态 `GAN3` 长按关机实际由 `System_Init.c` 的 `key_can()` 处理：

1. `LedBar_StartUp()` 后置 `sys_time.key_irq_scan_enable = true`。
2. TIM17 中断每约 10ms 调 `key_can()`。
3. `key_can()` 要求启动后先检测到一次 `GAN3` 松开，才允许后续长按计数。
4. 松开门控成立后，`GAN3` 闭合累计 300 个 10ms，约 3s：
   - 设置 `sleep_reason = 1`；
   - 调用 `LedBar_RequestShutdownAnimation()`。

`key_can()` 不检查 `GAN1/GAN2`，也不检查充电锁存；这些条件只存在于当前未调用的 `Gan_ProcessTopKey()` 中。

## 运行态休眠进入流程

`entersleep(mode)` 当前只置位 `Sleep_Mode`，不直接进入 STOP：

| 参数 | 当前置位 |
| --- | --- |
| `HICCUP_MODE` | `b1ForceToSleep_L1 = 1` |
| `NORMAL_MODE` | `b1ForceToSleep_L3 = 1` |
| `DEEP_MODE` | `b1ForceToSleep_L3 = 1` |
| `NO_SLEEP` | 清 `Sleep_Mode`，状态回普通选择 |

运行态请求休眠后的主流程：

1. `entersleep(DEEP_MODE)` 或直接置 `Sleep_Mode.bits.b1ForceToSleep_L3`。
2. `App_SleepDeal()` 看到强制休眠位后推进状态机。
3. 状态到 `SLEEP_HICCUP_CONTINUE` 时，先置 `Sleep_Mode.bits.b1_ToSleepFlag = 1`。
4. 下一轮 `App_SleepDeal()` 看到 `b1_ToSleepFlag` 后置 `LogRecord_Flag.bits.Log_Sleep = 1` 并返回。
5. `App_LogRecord()` 记录睡眠事件后清 `b1_ToSleepFlag`。
6. 再下一轮 `App_SleepDeal()` 执行 `SleepDeal_Continue()`：
   - `WakeDisplaySocCache_Write(g_stCellInfoReport.SocElement.u16Soc)`；
   - 按 `Sleep_Mode` 写 BootFlag；
   - `App_AFEshutdown()`；
   - `MCU_RESET()`。

`App_SleepDeal()` 在 `SystemStatus.bits.b1StartUpBMS == 1` 时直接返回，因此系统启动未完成时不会执行运行态休眠状态机。

当前普通空闲休眠选择：

- `SleepDeal_Normal_Select()` 在空闲电流条件满足时：
  - 最低单体电压 `< OtherElement.u16Sleep_Vlow`：进入 L3 深度休眠计时；
  - 否则进入 L2 普通休眠计时。
- L1/RTC 普通休眠路径在 `App_SleepDeal()` 的 switch 中当前被注释，普通空闲不会进入 `SleepDeal_Normal_L1()`。

当前还有一个低压强制保护：

- `App_SleepDeal()` 中如果 `g_stCellInfoReport.u16VCellMin < 2650` 持续约 60 分钟，会调用 `entersleep(DEEP_MODE)`。

## 复位后休眠启动路径

复位后 `InitDevice()` 很早调用 `IsSleepStartUp()`：

1. 读取 `BootFlag_Read()`。
2. 如果是 `FLASH_SLEEP_RESET_VALUE`，说明不是休眠启动，直接返回。
3. 如果是休眠标志：
   - 先 `BootFlag_Clear()`；
   - 按类型配置低功耗 IO 和唤醒源；
   - 进入 `SleepStartup_WaitForWakeup()` 无限循环。

不同 BootFlag 的初始化差异：

| BootFlag | 当前动作 |
| --- | --- |
| `FLASH_HICCUP_SLEEP_VALUE` | `Init_RTC()`，`IOstatus_RTCMode()`，`InitWakeUp_RTCMode()`。 |
| `FLASH_NORMAL_SLEEP_VALUE` | `IOstatus_NormalMode()`，`InitWakeUp_NormalMode()`。 |
| `FLASH_DEEP_SLEEP_VALUE` | `IOstatus_DeepMode()`，`InitWakeUp_DeepMode()`。 |

`IOstatus_Base()` 会把 GPIOA/B/C/F 配成模拟态并关闭 ADC，用于降低功耗。随后 `InitWakeUp_Base()` 配置唤醒源：

- PA0 -> EXTI0，上升沿。
- PC13/GAN3 -> EXTI13，下降沿。
- `GAN1/GAN2` 不作为 EXTI 唤醒源，只在 STOP 返回后轮询判定。

`SleepStartup_WaitForWakeup()` 当前是：

```text
while (1) {
    Sys_StopMode();
    IsSleepWakeupValid();
}
```

`Sys_StopMode()` 只调用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`。STOP 返回后不恢复 HSE/PLL，因为有效唤醒路径会立即复位。

## STOP 返回后的唤醒判定

`IsSleepWakeupValid()` 当前流程：

1. 调 `InitIO_ganhuangguan()` 初始化 `GAN1/GAN2/GAN3/GAN4/SWT_EN/SWT_AD`。
2. 延时 100ms。
3. 最多再扫描约 1000ms 的进水状态。
4. 若检测到进水：
   - 写 `WAKE_DISPLAY_MODE_WATER_ALARM`；
   - `MCU_RESET()`。
5. 若 `PA0` 为高且 `GAN1` 闭合：
   - 写 `WAKE_DISPLAY_MODE_CHARGER_WAKE`；
   - 清 `EXTI_Line0`；
   - `MCU_RESET()`。
6. 其他所有情况：
   - 写 `WAKE_DISPLAY_MODE_SOC_PREVIEW`；
   - `MCU_RESET()`。

当前注意点：

- `IsDI1Pressed()` 存在但未参与当前判定。
- 旧逻辑中“无效唤醒返回 0 继续 STOP”的分支当前实际上不存在；除非在前面复位失败，否则函数末尾会无条件写 SOC 预览并复位。
- PA0 有效但 `GAN1` 未闭合时，不会走充电唤醒，但会落入 SOC 预览复位。
- RTC 闹钟唤醒如果没有水、没有有效 PA0+GAN1，也会落入 SOC 预览复位。

## 复位后的 LED 分发

STOP 返回后写入 WakeDisplayState 并复位。复位后的 `LedBar_StartUp()` 读取模式：

### `WAKE_DISPLAY_MODE_SOC_PREVIEW`

当前行为：

1. 立即按缓存 SOC 点亮电量。
2. 每 10ms 检查一次 `GAN3`。
3. 如果 `GAN3` 持续闭合达到 200 个 10ms，约 2s：
   - 若 `GAN1 && GAN2` 同时闭合，播放阻塞开机动画并继续正常启动；
   - 否则灭灯并返回失败。
4. 如果没有达到长按阈值，显示累计到 300 个 10ms，约 3s 后灭灯并返回失败。
5. 返回失败时，`LedBar_StartUp()` 调 `SleepDeal_ReenterDeepSleepFromWakePreview()`：
   - `WakeDisplayMode_ClearKeepSoc()`；
   - 写 `FLASH_DEEP_SLEEP_VALUE`；
   - `MCU_RESET()`，重新进入休眠启动路径。

### `WAKE_DISPLAY_MODE_WATER_ALARM`

当前行为：

1. `LedBar_StartUp()` 调 `LedBar_SetWaterAlarm(1)` 后返回，不清 WakeDisplayState。
2. 后续 `InitVar()` 中 `LedBar_IsWakePreviewPending()` 会继续看到水报警模式，从而把 `System_OnOFF_Func.bits.b1OnOFF_MOS_Relay` 与启动记录中的 MOS/Relay 功能关掉。
3. 主循环中 `APP_LedBar()` 在启动完成后按 200ms 闪烁全灯。
4. 运行态 `ganhuangguan_Logi()` 若仍检测到进水，会执行进水保护和 120s 休眠计时。

当前没有使用 `LedBar_HandleWakeWaterAlarmAfterReset()`；`LEDBAR_WATER_PREBOOT_SLEEP_TICKS_10MS`、`LEDBAR_WATER_PREBOOT_BLINK_TICKS_10MS` 也未参与实际流程。

### `WAKE_DISPLAY_MODE_CHARGER_WAKE`

当前行为：

1. `LedBar_StartUp()` 灭灯。
2. `WakeDisplayMode_ClearKeepSoc()` 清模式但保留 SOC。
3. 继续正常系统启动。
4. 充电锁存与充电 LED 后续由 `ganhuangguan_Logi()` 的充电逻辑接管。

### `WAKE_DISPLAY_MODE_BOOT_SEQUENCE`

当前行为：

1. 调 `LedBar_RequestBootAnimation(wake_soc)`。
2. 清模式但保留 SOC。
3. 后续由 `APP_LedBar()` 非阻塞播放点亮动画。

但当前源码没有实际调用 `WakeDisplay_RequestBootSequence()`，因此该分支主要是兼容保留。

### `WAKE_DISPLAY_MODE_NONE` 或读取失败

当前行为：

- 灭灯。
- 清模式但保留 SOC。
- 正常启动。

## 当前未生效或容易误判的逻辑

| 代码/宏 | 当前状态 | 影响 |
| --- | --- | --- |
| `Gan_ProcessTopKey()` | 定义存在，调用被注释 | 运行态 `GAN3` 的 1s 临时 SOC 和带 `GAN1/GAN2/充电` 条件的 3s 关机逻辑不生效。 |
| `Gan_ClearWater()` | 定义存在，调用被注释 | 水报警、水计时、`SWT_EN` 恢复不会在水消失时主动清理。 |
| `WakeDisplay_RequestBootSequence()` | 只有定义，无实际调用 | `WAKE_DISPLAY_MODE_BOOT_SEQUENCE` 分支基本是兼容保留。 |
| `LedBar_HandleWakePreviewBeforeBoot()` | 头文件声明存在，C 文件实现被注释 | 当前没有这个入口。 |
| `LEDBAR_PREBOOT_RELEASE_TICKS_10MS` | 未使用 | “长按后必须松手”的复位后门控当前不靠该宏实现。 |
| `LEDBAR_WATER_PREBOOT_*` | 未使用 | 复位后水报警时长/闪烁不走这组宏。 |
| `SleepDeal_Normal_L1()` | 函数存在，主状态机调用被注释 | 普通空闲不会进入 RTC L1 周期休眠。 |
| `IsDI1Pressed()` | 函数存在，判定被注释 | STOP 返回后不再显式判断 PC13 是否仍按下。 |

## 当前行为核对清单

建议上板按以下现状验证，而不是按旧文档预期验证：

1. 正常运行、`GAN1 && GAN2` 闭合：应保持驱动输出，LED 常态显示 SOC；故障时第一格闪烁。
2. 正常运行、`GAN1` 断开：约 0.5s 后请求深度休眠。
3. 正常运行、`GAN2` 断开：约 2s 后请求深度休眠。
4. 正常运行、`GAN3` 长按：必须先经历一次松开门控；之后闭合约 3s 触发关机动画。该路径当前不检查 `GAN1/GAN2`。
5. 充电路径：`GAN1` 闭合且 PA0 有效或充电电流有效后进入充电锁存；`GAN1` 断开立即请求休眠。
6. 充电保持：充电电流有效、SOC 满、或最高单体达到满电电压时保持充电显示；否则满足丢失条件约 1s 后休眠。
7. 运行态进水：关闭输出并全灯闪烁；`GAN1/GAN2` 任一断开立即休眠，否则约 120s 后休眠。
8. 休眠后 PA0 唤醒且 `GAN1` 闭合：写充电唤醒模式并复位，随后正常启动。
9. 休眠后 `GAN3`/其他非水非有效充电唤醒：复位后显示缓存 SOC 约 3s；如果 `GAN3` 连续闭合约 2s 且 `GAN1 && GAN2` 闭合，则开机。
10. 休眠后进水：写水报警模式并复位；启动后水报警模式不清 WakeDisplayState，需要关注后续水消失时是否能退出。

