# 当前 LED 灯板、休眠唤醒、干簧管逻辑梳理

日期：2026-06-04

本文按当前代码实现梳理，不描述历史旧逻辑。核心文件：

- `Code/Source/SleepDeal.c`
- `Code/Source/LedBar.c`
- `Code/Source/gan_huang_guan_logi.c`
- `Code/Source/Flash.c` / `Code/Source/Flash.h`
- `Code/Source/IO_Control.c`
- `Code/Source/main.c`

## 信号定义

| 信号 | 当前含义 | 有效电平/条件 | 主要使用位置 |
| --- | --- | --- | --- |
| `GAN1` | 浮动连接器/插头在位 | GPIO 读 0 为闭合 | 充电允许、放电允许、开机允许、断开休眠 |
| `GAN2` | 把手在位 | GPIO 读 0 为闭合 | 放电允许、开机允许、提手休眠、进水报警退出 |
| `GAN3` / `PC13` | 灯板开关/电池上盖键 | GPIO 读 0 为闭合 | 休眠唤醒、SOC 预览、长按开关机 |
| `PA0` | 充电器插入唤醒信号 | GPIO 读 1 为有效 | 休眠态充电唤醒；CHG MOS 打开后不能继续作为在线判断 |
| `SWT_AD` | 进水检测 | GPIO 读 0 为进水 | 运行态进水保护、休眠态进水报警 |
| `SWT_EN` | 进水检测使能 | `Bit_SET` 使能，`Bit_RESET` 关闭 | 水检测采样和进水保护 |

## 总体启动链路

`main.c` 当前启动顺序：

1. `InitDelay()`
2. `IsSleepStartUp()`
3. `InitIO()`
4. `LedBar_StartUp()`
5. `InitTimer()`
6. `InitSystemWakeUp()`
7. EEPROM、串口、ADC、SOC、参数、AFE、MOS 等完整初始化

`LedBar_StartUp()` 已前移到 `InitIO()` 后，这样休眠唤醒复位后可以尽早处理 SOC 预览、进水报警和开机动画，不等待 EEPROM、ADC、SOC、AFE 等后续初始化完成。

## 备份域状态

`Flash.c` 使用 RTC 备份寄存器保存两类状态：

- `BootFlag`：保存系统是否从休眠路径复位进入。
- `WakeDisplayState`：高 16 位保存唤醒显示模式，低 16 位保存 SOC 缓存。

当前唤醒显示模式：

| 模式 | 值 | 含义 |
| --- | --- | --- |
| `WAKE_DISPLAY_MODE_NONE` | `0x0000` | 无复位后灯板动作 |
| `WAKE_DISPLAY_MODE_SOC_PREVIEW` | `0x0001` | `GAN3` 唤醒后复位，复位后 SOC 预览/长按开机判定 |
| `WAKE_DISPLAY_MODE_BOOT_SEQUENCE` | `0x0002` | 兼容旧开机动画模式 |
| `WAKE_DISPLAY_MODE_WATER_ALARM` | `0x0003` | 休眠态进水，复位后全灯闪烁报警 |
| `WAKE_DISPLAY_MODE_CHARGER_WAKE` | `0x0004` | 充电器唤醒，复位后正常启动 |

`WakeDisplayMode_ClearKeepSoc()` 只清除模式，不清 SOC 缓存。短按预览后重新休眠会保留 SOC，避免下次预览电量变成 0。

## 进入休眠流程

运行态请求休眠时，通常调用 `entersleep(DEEP_MODE)`，它只置位 `Sleep_Mode.bits.b1ForceToSleep_L3`。

随后 `App_SleepDeal()` 在主循环中执行：

1. 若 `SystemStatus.bits.b1StartUpBMS` 仍为 1，则不进入休眠处理。
2. 若没有强制休眠标志，按 1s 节拍推进。
3. 当状态推进到 `SLEEP_HICCUP_CONTINUE` 时调用 `SleepDeal_Continue()`。
4. `SleepDeal_Continue()` 先写入当前 SOC 到备份域低 16 位。
5. 根据 `Sleep_Mode` 选择写 `FLASH_NORMAL_SLEEP_VALUE`、`FLASH_HICCUP_SLEEP_VALUE` 或 `FLASH_DEEP_SLEEP_VALUE`。
6. 写成功后调用 `App_AFEshutdown()`，随后 `MCU_RESET()`。

复位后 `IsSleepStartUp()` 读到休眠标志，清除 `BootFlag`，配置低功耗 IO 和唤醒源，然后进入 STOP 循环。

## STOP 休眠与唤醒流程

`SleepStartup_WaitForWakeup()` 当前是无限循环：

1. 调用 `Sys_StopMode()` 进入 STOP。
2. STOP 返回后调用 `IsSleepWakeupValid()`。
3. 有效唤醒会记录原因并立即 `MCU_RESET()`。
4. 无效唤醒返回 0，继续下一轮 STOP。

`Sys_StopMode()` 只执行 `PWR_EnterSTOPMode()`；有效唤醒后不在这里恢复 HSE/PLL，因为马上复位。

### STOP 返回后的判定优先级

`IsSleepWakeupValid()` 先做最小 GPIO 初始化：

- 初始化 `GAN1/GAN2/GAN3/GAN4`
- 使能并初始化 `SWT_EN`
- 初始化 `SWT_AD` 输入

然后按以下顺序判定：

1. 如果 `GAN3/PC13` 当前闭合：
   - 若 `is_water_in()` 为真，写 `WAKE_DISPLAY_MODE_WATER_ALARM`。
   - 否则写 `WAKE_DISPLAY_MODE_SOC_PREVIEW`。
   - 清 `EXTI_Line13`。
   - 立即 `MCU_RESET()`。

2. 如果 `PA0` 有效：
   - 只有 `GAN1` 闭合时才认为充电唤醒有效。
   - 写 `WAKE_DISPLAY_MODE_CHARGER_WAKE`。
   - 清 `EXTI_Line0`。
   - 立即 `MCU_RESET()`。
   - 如果 `GAN1` 未闭合，则认为无效唤醒，继续回 STOP。

3. 其他情况：无效唤醒，继续回 STOP。

注意：`GAN1`、`GAN2` 不配置为 STOP 唤醒 EXTI。休眠态主要由 `GAN3/PC13` 和 `PA0` 唤醒。

## 复位后的 LED 启动分发

STOP 有效唤醒后都会先 MCU reset。复位后的 `LedBar_StartUp()` 读取备份域模式并分发。

### `WAKE_DISPLAY_MODE_SOC_PREVIEW`

流程：

1. 立即按备份域 SOC 显示电量。
2. 每 10ms 检查一次 `GAN3`。
3. 如果 `GAN3` 未长按到约 3s，SOC 显示约 3s 后灭灯。
4. 灭灯后调用 `SleepDeal_ReenterDeepSleepFromWakePreview()`：
   - 清唤醒模式但保留 SOC。
   - 写 `FLASH_DEEP_SLEEP_VALUE`。
   - `MCU_RESET()`，重新进入休眠启动路径。
5. 如果 `GAN3` 持续约 3s：
   - 只有 `GAN1 && GAN2` 同时闭合，才允许开机。
   - 若条件满足，阻塞播放一次开机动画，然后继续正常启动。
   - 若 `GAN1` 或 `GAN2` 不满足，灭灯并重新休眠，不开 BMS、不播放开机动画。

开机动画顺序：

```text
1 -> 12 -> 123 -> 1234 -> 12345 -> 全灭 -> SOC
```

这里的阻塞动画使用备份域 SOC 显示最后电量；后续主循环启动后会由实时 SOC 接管显示。

### `WAKE_DISPLAY_MODE_WATER_ALARM`

流程：

1. 复位后立即全灯亮。
2. 每约 500ms 全灯亮/灭切换。
3. 如果 `GAN2` 断开，立即灭灯并重新进入深度休眠。
4. 如果约 2 分钟无操作，灭灯并重新进入深度休眠。
5. 报警期间 `GAN3` 按下或边沿变化会重置无操作计时。

这个模式下不执行 SOC 预览。

### `WAKE_DISPLAY_MODE_CHARGER_WAKE`

复位后 `LedBar_StartUp()` 只灭灯并清模式，随后继续正常系统初始化。充电是否保持、CHG MOS 是否打开、充电 LED 如何显示，由运行态 `gan_huang_guan_logi.c` 和驱动流程处理。

### `WAKE_DISPLAY_MODE_BOOT_SEQUENCE`

兼容旧模式：请求非阻塞开机动画。当前新的休眠 `GAN3` 长按开机路径主要走阻塞动画。

## 运行态干簧管业务

运行态 `ganhuangguan_Logi()` 由 `IO_Control.c` 的 `RefreshData_Drivers()` 调用，周期来自 `App_MOS_Relay_Ctrl()` 的 10ms 节拍。

`RefreshData_Drivers()` 中顺序是：

1. 默认 `DriverForceExt = FORCE_KEEP_MODE`。
2. 调用 `ganhuangguan_Logi()` 执行业务逻辑。
3. 再由保护条件覆盖驱动状态，例如系统保护、AFE 通讯错误、CBC 错误、温度断线、进水等会强制 `FORCE_CLOSE_MODE`。

### 运行态优先级

`ganhuangguan_Logi()` 当前优先级：

1. 如果已经请求休眠：保持驱动关闭，直接返回。
2. 如果进水：清充电锁存，执行进水逻辑，直接返回。
3. 未进水：清进水计时和报警。
4. 处理充电逻辑。
5. 处理 `GAN3` 顶盖键逻辑。
6. 如果处于充电路径，直接返回。
7. 处理放电逻辑。

### 进水逻辑

触发条件：`SWT_AD` 读 0。

动作：

- 关闭驱动输出。
- 关闭充电显示和放电显示。
- 开启 LED 全灯闪烁报警。
- `SWT_EN` 拉低。
- 若 `GAN1` 或 `GAN2` 断开，立即请求休眠。
- 若 `GAN1 && GAN2` 都闭合，进水持续约 2 分钟后请求休眠。

运行态进水请求休眠后，LED 报警会被关闭并进入休眠流程；休眠态再次按 `GAN3` 唤醒时，会走复位后的进水报警模式。

### 充电逻辑

进入充电锁存条件：

- `GAN1` 闭合，并且满足以下任一条件：
  - `PA0` 为高，即充电器插入唤醒信号有效。
  - 检测到充电电流。

充电锁存后：

- 忽略 `GAN2` 和 `GAN3` 对充电保持的影响。
- `GAN1` 断开时立即清充电锁存并请求休眠。
- 保持驱动输出。
- 关闭放电显示。

充电保持和断开判定：

- 若存在充电电流，或 SOC 已满，或最高单体电压达到 `OtherElement.u16Soc_V_100`，保持充电锁存并显示充电动画。
- 若充电电流消失、SOC 未满、最高单体电压未达到 `u16Soc_V_100`，持续约 1s 后认为充电器断开，请求休眠。

这个逻辑用于规避 CHG MOS 打开后 PA0 失效的问题：PA0 只作为插入触发源，不作为 CHG MOS 打开后的持续在线判断。

### 运行态 `GAN3` 顶盖键逻辑

充电中或关机动画进行中，`GAN3` 不触发 SOC 预览/关机。

非充电、非关机动画时：

- `GAN3` 闭合约 1s：请求临时 SOC 显示，持续约 3s。
- `GAN3` 闭合约 3s：进入长按处理，并等待释放，避免一直按住重复触发。
- 只有 `GAN1 && GAN2` 同时闭合时，长按 3s 才会触发关机动画。
- `GAN1` 或 `GAN2` 不满足时，长按不执行关机动作。

运行态长按关机动作：

1. 设置 `sleep_reason = 1`。
2. `LedBar_RequestShutdownAnimation()`。
3. 关机动画执行完后调用 `entersleep(DEEP_MODE)`。
4. 后续由 `App_SleepDeal()` 写休眠标志、关 AFE、复位并进入 STOP。

### 放电逻辑

非进水、非充电路径下：

- `GAN1` 断开：关闭放电显示，持续约 0.5s 后请求休眠。
- `GAN1` 闭合但 `GAN2` 断开：保持驱动，持续约 2s 后请求休眠。
- `GAN1 && GAN2` 都闭合：保持驱动，开启放电 SOC 显示。

## LED 运行态显示状态机

`APP_LedBar()` 每 100ms 执行一次，优先级如下：

1. 如果 `SystemStatus.bits.b1StartUpBMS` 为 1，直接返回。
2. 如果进水报警使能，执行全灯闪烁。
3. 如果处于 UI 模式：
   - `LED_UI_SOC_TEMP`：临时 SOC 显示，到时后灭灯。
   - `LED_UI_BOOT_ANIM_ON`：开机动画点亮段。
   - `LED_UI_BOOT_ANIM_OFF`：开机动画全灭帧后显示实时 SOC。
   - `LED_UI_SHUTDOWN_ANIM`：关机动画。
4. 如果 `sleep_reason == 1`，强制灭灯。
5. 如果充电显示使能或命令为 `LED_BAR_CHG`，显示充电动画。
6. 如果放电显示使能或命令为 `LED_BAR_NORMAL`，按放电显示逻辑处理。
7. 其他情况灭灯。

### SOC 显示

SOC 分段灯：

- 默认至少亮第 1 格。
- `SOC >= 20` 亮第 2 格。
- `SOC >= 40` 亮第 3 格。
- `SOC >= 60` 亮第 4 格。
- `SOC >= 80` 亮第 5 格。

### 充电显示

- `SOC >= 99`：5 灯常亮。
- `SOC < 99`：已完成段常亮，下一段以约 500ms 闪烁。

### 故障显示

普通放电显示前会检查故障：

- `g_stCellInfoReport.unMdlFault_Third.all & 0x2FFA`
- `ERROR_STATUS_TEMP_BREAK`
- `ERROR_STATUS_CBC_DSG`

存在故障时，`SOC_20` 灯翻转闪烁，其余灯灭。

### 开机动画

非阻塞开机动画：

```text
1 -> 12 -> 123 -> 1234 -> 12345 -> 全灭 -> 实时 SOC
```

每步约 100ms。

休眠唤醒长按开机路径使用阻塞版动画，顺序相同，最后先显示备份域 SOC，等系统主循环运行后再由实时 SOC 刷新。

### 关机动画

关机动画开始时 5 灯全亮。

随后每约 100ms 从高位到低位逐步熄灭：

```text
12345 -> 1234 -> 123 -> 12 -> 1 -> 全灭
```

动画结束后：

- 清放电显示。
- 清充电显示。
- 保持灭灯。
- 调用 `entersleep(DEEP_MODE)`。

## 时间参数汇总

| 参数 | 当前值 | 含义 |
| --- | --- | --- |
| `GAN1_OFF_SLEEP_TICKS_10MS` | `50` | 运行态 `GAN1` 断开约 0.5s 后休眠 |
| `GAN2_OFF_SLEEP_TICKS_10MS` | `200` | 运行态 `GAN2` 断开约 2s 后休眠 |
| `GAN3_SOC_TICKS_10MS` | `100` | 运行态 `GAN3` 按下约 1s 后临时 SOC 显示 |
| `GAN3_POWER_TICKS_10MS` | `300` | 运行态 `GAN3` 按下约 3s 后长按关机判定 |
| `WATER_SLEEP_TICKS_10MS` | `12000` | 运行态进水约 2 分钟后休眠 |
| `CHARGER_LOST_TICKS_10MS` | `100` | 充电电流消失且 SOC/电压未满约 1s 后判定充电器断开 |
| `LEDBAR_PREBOOT_POWERON_TICKS_10MS` | `300` | 休眠唤醒后 `GAN3` 长按约 3s 开机判定 |
| `LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS` | `300` | 休眠唤醒后 SOC 预览约 3s |
| `LEDBAR_WATER_PREBOOT_SLEEP_TICKS_10MS` | `12000` | 休眠唤醒后进水报警约 2 分钟无操作后休眠 |
| `LEDBAR_WATER_PREBOOT_BLINK_TICKS_10MS` | `50` | 休眠唤醒后进水报警约 500ms 翻转一次 |
| `LEDBAR_ANIM_STEP_TICKS_100MS` | `1` | 开关机动画每步约 100ms |
| `LEDBAR_CHG_BLINK_TICKS_100MS` | `5` | 充电闪烁约 500ms 翻转一次 |
| `LEDBAR_WATER_BLINK_TICKS_100MS` | `5` | 运行态进水报警约 500ms 翻转一次 |

## 当前代码注意点

1. `SleepStartup_WaitForWakeup()` 当前不会返回；因此 `IsSleepStartUp()` 中它后面的 `IORecover_*()` switch 是历史遗留的不可达代码，可以删除或改成注释说明。
2. `LedBar_HandleWakePreviewBeforeBoot()` 仍保留旧名字和外部声明，但 `SleepDeal.c` 已不再调用它；当前主要逻辑在 `LedBar_StartUp()` 内复位后执行。
3. `LedBar_StartUp()` 在 `InitTimer()`、`InitAFE1()`、`InitData_SOC()` 之前执行；休眠唤醒显示必须依赖备份域 SOC，不应依赖实时 SOC 初始化结果。
4. 充电锁存后不受 `GAN2/GAN3` 影响，这是为了满足 CHG MOS 打开后 PA0 失效且充电不能被把手/上盖状态打断的需求。
5. 运行态进水保护和休眠态进水报警是两条路径：运行态由 `ganhuangguan_Logi()` 控制输出和报警，休眠态由 `SleepDeal.c` 记录模式并在复位后由 `LedBar_StartUp()` 报警。
