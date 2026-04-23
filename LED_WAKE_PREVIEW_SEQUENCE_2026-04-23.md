# 灯板唤醒预览与开关机时序调整

## 变更目的

本次修改针对灯板 `LED1~LED5` 的显示时序，满足以下需求：

1. 开机状态短按干簧管，只显示当前电量 `5s`，然后全部熄灭。
2. 开机状态长按干簧管 `3s` 关机，关机动画改为：
   先全亮，再按 `LED5 -> LED1` 每 `0.2s` 逐个熄灭，`1s` 跑完。
3. 关机状态下，干簧管闭合达到电量显示阈值 `0.3s` 后：
   先显示电量 `3s`，然后熄灭。
4. 如果上述 `3s` 电量显示期间干簧管一直保持闭合，继续进入开机流程：
   `LED1 -> LED5` 每 `0.2s` 逐个点亮，`1s` 全亮；
   再按 `LED5 -> LED1` 每 `0.2s` 逐个熄灭，`1s` 全灭；
   最后再显示电量 `3s`，然后全部熄灭。

## 关键实现

### 1. 开机态短按显示

- `LedBar.c`
- 常态下 LED 默认熄灭。
- 仅在开机状态、短按释放后进入 `LED_UI_SHORT_SHOW`。
- 按实时 `SOC` 显示 `5s`，超时后熄灭。

### 2. 关机态先预览电量，再决定是否开机

- `SleepDeal.c`
- 睡眠态下 `DI1/PC13` 闭合 `0.3s` 就视为有效唤醒，不再直接要求先按满 `3s`。
- 到达 `0.3s` 时写入“电量预览唤醒”标志。

- `LedBar.c`
- 启动时如果检测到“电量预览唤醒”标志，则先进入 `LED_UI_WAKE_PREVIEW_SHOW`。
- 预览阶段显示缓存 `SOC` 共 `3s`。
- 预览结束时：
  - 若干簧管仍闭合，进入开机跑马灯。
  - 若已释放，则请求重新进入睡眠。

### 3. 开机态长按关机动画

- `LedBar.c`
- 仅在已经开机且 UI 处于 `LED_UI_NORMAL` 时，长按 `3s` 触发关机动画。
- 动画顺序：
  - 初始全亮；
  - 再按 `LED5 -> LED1` 每 `0.2s` 熄灭一个；
  - `1s` 完成后请求进入睡眠。

### 4. 开机动画与开机后再显示电量

- `LedBar.c`
- 预览阶段持续闭合时，进入两段式开机动画：
  - `LED_UI_BOOT_ANIM_ON`：`LED1 -> LED5` 每 `0.2s` 点亮；
  - `LED_UI_BOOT_ANIM_OFF`：`LED5 -> LED1` 每 `0.2s` 熄灭。
- 动画结束后进入 `LED_UI_BOOT_POST_SHOW`，再显示缓存 `SOC` `3s`，随后熄灭。

### 5. 休眠唤醒后 SOC 不可靠的问题

用户特别指出：`IsSleepStartUp()` 场景下 MCU 已经过复位并退出 `STOP`，此时不能依赖实时 `SOC` 重新计算结果。

为解决这个问题，本次改动增加了休眠前 `SOC` 缓存机制：

- `SleepDeal.c`
  - 进入休眠流程前，把当前 `g_stCellInfoReport.SocElement.u16Soc` 写入备份寄存器。
- `Flash.c/.h`
  - 新增唤醒显示状态接口。
  - 使用 `RTC->BKP3R/BKP4R` 保存：
    - 唤醒显示模式
    - 休眠前缓存 `SOC`
  - 通过正反码校验保证数据有效性。
- `LedBar.c`
  - 预览显示、开机动画后的 `3s` 电量显示，均优先使用缓存 `SOC`。

## 影响文件

- `Code/Source/Flash.c`
- `Code/Source/Flash.h`
- `Code/Source/LedBar.c`
- `Code/Source/LedBar.h`
- `Code/Source/SleepDeal.c`
- `Code/Source/main.c`

## 验证结果

- 2026-04-23 使用 `Keil 5 / UV4.exe` 对工程 `CommomBQ769x0_16series_030C8T6_C.uvprojx`
  的 `Target 1` 进行了重新编译。
- 结果：`0 Error(s), 47 Warning(s)`。
- 产物已更新：
  - `Objects/CommomBQ769x0_16series_030C8T6_C.axf`
  - `Objects/CommomBQ769x0_16series_030C8T6_C.hex`
  - `Objects/CommomBQ769x0_16series_030C8T6_C.bin`

## 二次修正

- 2026-04-23 二次修正了两个实际联调问题：
  - 休眠唤醒后的预览状态在后续初始化阶段可能被 `RTC` 备份域处理影响，因此在 `IsSleepStartUp()` 唤醒返回后，先把唤醒显示状态抓到 RAM 阴影变量，再由 `LedBar_StartUp()` 读取。
  - 长按开机成功后，如果用户一直不松手，原逻辑会把同一次长按继续累计成关机长按。现已改成进入开机序列后必须先松手，后续再次长按才允许进入关机/休眠逻辑。
