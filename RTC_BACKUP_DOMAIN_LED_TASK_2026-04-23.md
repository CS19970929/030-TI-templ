# 备份域寄存器与 LED 任务说明

## 目的

本文档说明本项目中 `RTC` 备份域寄存器的实际用法，并结合本次灯板 `LED` 唤醒预览/开机动画任务，回答以下问题：

1. 修改备份域寄存器是否会影响 `RTC` 模块。
2. 当前芯片一共有几个备份域寄存器。
3. 本项目现在占用了哪些寄存器。
4. 本次 `LED` 任务为什么必须使用备份域。
5. 后续在本项目里还能如何继续使用备份域。

## 结论

- 单纯读写备份域寄存器，不会直接影响 `RTC` 走时、日期时间和闹钟。
- 真正会影响 `RTC` 的，不是 `BKP` 寄存器本身，而是对备份域整体的复位、时钟源切换或重新初始化。
- 当前芯片在本项目里可见的备份域寄存器一共是 `5` 个：`BKP0R ~ BKP4R`。
- 本项目现在已经基本用满：
  - `BKP0` 给 `RTC` 模块自己使用；
  - `BKP1/BKP2` 给休眠启动标志使用；
  - `BKP3/BKP4` 给本次 `LED` 唤醒显示模式和缓存 `SOC` 使用。
- 如果后续还想继续扩展，建议优先做“状态打包重构”，而不是继续零散占寄存器。

## 芯片备份域寄存器数量

从当前工程头文件可见：

- [stm32f0xx.h](/E:/TODO/030%20+%20TI/Code/Drivers/stm32f0xx.h:910)
- [stm32f0xx_rtc.h](/E:/TODO/030%20+%20TI/Code/STM32F0xx_StdPeriph_Driver/inc/stm32f0xx_rtc.h:626)

当前芯片暴露的 `RTC` 备份寄存器为：

- `BKP0R`
- `BKP1R`
- `BKP2R`
- `BKP3R`
- `BKP4R`

也就是总共 `5` 个 `32-bit` 备份寄存器。

## 修改备份域寄存器是否会影响 RTC

### 不会直接影响的情况

像下面这种普通读写：

- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:71) `RTC->BKP3R = value;`
- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:95) `RTC->BKP1R = flag;`
- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:140) `RTC_ReadBackupRegister(RTC_BKP_DR0)`

这类操作本质只是访问备份域中的存储单元，不会直接改 `RTC` 的计数器、时间日期寄存器和闹钟寄存器。

### 会间接影响的情况

下面这些动作会影响整个备份域，进而可能影响 `RTC` 或让 `BKP` 里的内容丢失：

- 切换 `RTC` 时钟源
- 对备份域做复位
- 重新初始化 `RTC`
- 在 `LSE/LSI` 切换过程中重写 `BDCR`

本项目中最典型的位置在：

- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:39) `RCC->BDCR = 0x10000`
- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:41) `RCC->BDCR = 0x8200`

这就是为什么本次 `LED` 任务里，必须在 `Init_RTC()` 一开始先执行：

- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:181) `WakeDisplayState_CaptureForBoot()`

先把备份域中的 `LED` 唤醒显示状态抓到 RAM 阴影区，再继续后面的 `RTC` 初始化。

## 本项目当前占用情况

### BKP0

`RTC` 模块自用。

位置：

- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:140)
- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:144)

用途：

- 记录 `RTC` 是否已经做过初始化；
- 避免每次启动都重新配置时间和闹钟基准。

### BKP1 和 BKP2

休眠启动标志使用。

位置：

- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:92) `BootFlag_Write`
- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:99) `BootFlag_Read`

用途：

- 记录当前是从哪种休眠模式恢复；
- 在复位后让 [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:966) 的 `IsSleepStartUp()` 判断是否走休眠恢复路径；
- `BKP1` 存主值，`BKP2` 存反码，做基本一致性校验。

### BKP3 和 BKP4

本次 `LED` 唤醒显示任务使用。

位置：

- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:63) `WakeDisplayState_Write`
- [Flash.c](/E:/TODO/030%20+%20TI/Code/Source/Flash.c:76) `WakeDisplayState_ReadRaw`

用途：

- 保存 `LED` 唤醒显示模式：
  - `WAKE_DISPLAY_MODE_NONE`
  - `WAKE_DISPLAY_MODE_SOC_PREVIEW`
  - `WAKE_DISPLAY_MODE_BOOT_SEQUENCE`
- 保存休眠前缓存 `SOC`
- `BKP3` 存合成后的状态值，`BKP4` 存按位取反值，用于简单校验。

## 本次 LED 任务为什么必须使用备份域

### 问题背景

本次 `LED` 任务要求：

1. 休眠态按下干簧管后，要尽快显示 `SOC`
2. 如果长按满 `3s`，再进入开机动画
3. 开机动画后还要再显示一次缓存 `SOC`

但在当前项目中，休眠恢复路径并不是“原地继续跑”，而是会再次走复位流程：

- [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:277) `IORecover_RTCMode`
- [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:282) `IORecover_NormalMode`
- [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:288) `IORecover_DeepMode`

这些路径最终都会 `MCU_RESET()`。

所以只靠普通 RAM 变量不够，因为：

- 一复位 RAM 上下文就丢了；
- 重新启动时实时 `SOC` 不一定已经恢复可靠；
- 用户按键动作已经发生过，后续启动流程需要知道“刚才是短按预览还是长按开机”。

### 备份域在 LED 任务中的角色

本次方案里，备份域承担的是“跨复位的小状态存储”：

- 在进入休眠前：
  - [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:388) `WakeDisplaySocCache_Write(g_stCellInfoReport.SocElement.u16Soc);`
  - 先把休眠前 `SOC` 缓存起来

- 在按键唤醒到预览阈值后：
  - [SleepDeal.c](/E:/TODO/030%20+%20TI/Code/Source/SleepDeal.c:54) `WakeDisplay_RequestSocPreview();`
  - 记录“这次是电量预览唤醒”

- 在预览阶段长按满 `3s` 后：
  - [LedBar.c](/E:/TODO/030%20+%20TI/Code/Source/LedBar.c:334) `WakeDisplay_RequestBootSequence();`
  - 记录“这次应进入开机动画序列”

- 在正式启动早期：
  - [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:181) `WakeDisplayState_CaptureForBoot();`
  - 先把备份域内容抓到 RAM 阴影区，避免被 `RTC` 初始化覆盖

- 在 `LED` 模块启动时：
  - [LedBar.c](/E:/TODO/030%20+%20TI/Code/Source/LedBar.c:490) `WakeDisplayState_Read(&wake_mode, &wake_soc)`
  - 决定这次要不要走开机动画，以及动画后显示哪一个 `SOC`

## 本次 LED 任务中备份域的优点

- 能跨软件复位保留极少量关键状态
- 比写内部 Flash 轻量，不需要频繁擦写
- 访问速度快，适合启动早期读取
- 很适合保存这种“一次性启动上下文”

## 本次 LED 任务中备份域的限制

- 当前芯片只有 `5` 个备份寄存器，可用空间很小
- 现在项目里已经基本没有空闲寄存器
- 一旦 `RTC` 时钟源切换或备份域被复位，内容就可能丢失
- 如果板子没有独立 `VBAT` 维持，整机彻底掉电后不保证还能保留

## 常见使用场景

备份域寄存器最常见的用途通常是：

- 启动来源标志
- 休眠来源或唤醒来源记录
- 一次性启动任务标志
- 看门狗复位前的故障面包屑
- Bootloader / App 握手标志
- 少量运行态缓存值，如 `SOC`、故障摘要、UI 状态

## 本项目后续还能用在哪些地方

从“值不值得占用备份域”的角度，后续最适合的是这些方向：

- 记录“上次休眠原因”
  - 用户主动关机
  - 低功耗自动休眠
  - 故障保护休眠

- 记录“上次唤醒原因”
  - `RTC`
  - 干簧管按键
  - 充电唤醒

- 记录“看门狗复位前最后一步”
  - 便于现场定位死机点

- 记录“IAP/App 一次性握手标志”
  - 某些短期启动标记适合放在备份域，而不是单独写 Flash

## 建议

### 1. 不要再零散新增寄存器用途

当前 `BKP0 ~ BKP4` 已经基本用满，继续零散扩展会让后续维护越来越难。

### 2. 优先保住 BKP0 给 RTC 模块

`BKP0` 已经被 `RTC` 初始化逻辑占用，不建议拿来和业务状态混用。

### 3. 后续若继续扩展，建议做状态打包

比较建议的方向是：

- 保留 `BKP0` 给 `RTC`
- 把 `BKP1/BKP2/BKP3/BKP4` 重构成：
  - 一个统一状态字
  - 一个反码字
  - 一个扩展数据字
  - 一个扩展数据反码字

这样可以把现在分散的：

- `BootFlag`
- `WakeDisplay mode`
- `Wake SOC`
- 未来的 `WakeReason` / `SleepReason`

统一放到一个结构里，减少后续互相覆盖的风险。

### 4. 对关键状态保持“主值 + 反码”或“CRC”思路

本次 `LED` 任务已经采用了主值+反码校验，这个思路是对的，后续如果继续扩展，也建议保持。

## 结合本次 LED 任务的最终建议

针对当前项目现状，最实用的策略是：

- 短期内继续沿用当前方案：
  - `BKP0` 给 `RTC`
  - `BKP1/BKP2` 给 `BootFlag`
  - `BKP3/BKP4` 给 `LED` 唤醒显示状态

- 中期如果还有新的跨复位状态需求：
  - 不要再直接加新寄存器用途
  - 先做一次“备份域状态整合重构”

- 对所有依赖备份域的业务逻辑，都遵守一个原则：
  - 在可能触发备份域重配的初始化之前，先抓取到 RAM 阴影变量

本次 `LED` 任务里新增的：

- [RTC.c](/E:/TODO/030%20+%20TI/Code/Source/RTC.c:181)

就是这个原则的直接体现。
