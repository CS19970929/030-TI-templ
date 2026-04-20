# LED逻辑梳理与需求实现说明（LedBar）

## 1. 目标
本文档用于梳理当前 `LedBar` 代码行为，并明确实现以下需求：

1. 灯板干簧管持续闭合 3 秒后，触发开机/关机。
   - 开机动画：跑马灯 `LED1 -> LED5` 逐一亮起。
   - 关机动画：先全亮，再 `LED5 -> LED1` 逐一熄灭。
2. 开机状态下，灯板干簧管短闭合（1 秒内，例如 0.3 秒）：
   - 电量显示 1 秒，熄灭 1 秒；
   - 循环 5 次后退出，不再显示电量。

## 2. 当前代码梳理（现状）

### 2.1 入口与时基
- 主入口：`APP_LedBar()`（`Code/Source/LedBar.c`）。
- 调度频率：仅在 `b1Sys100msFlag` 为 1 时执行，即 **100ms 周期**。
- 运行前置：`SystemStatus.bits.b1StartUpBMS` 为 1 时直接返回，不刷新灯效。

### 2.2 当前灯态命令
- `LEDBAR_COMMAND`：`LED_BAR_NORMAL / LED_BAR_CHG / LED_BAR_DSG / LED_BAR_FAULT`。
- 当前未实现专门的“开机动画/关机动画/短按显示流程状态机”。

### 2.3 当前输入信号
- `MCUI_SOC_KEY`（`LedBar.h`）：`PORT_IN_GPIOB->bit14`。
- `is_open_gan3()`（`gan_huang_guan_logi.c`）使用 `GPIOC13`，而 `PC13` 同时也是 `KEY1/DI1`。
- 现有开关机（3 秒长按）已在 `IO_Control.c -> App_DI1_Switch()` 中按 `PC13` 实现。

### 2.4 当前显示行为
- 正常态显示 `SOC` 常亮分段。
- 充电态会做“当前档位闪烁”。
- 放电态显示 `SOC` 分段。
- 故障态会覆盖为告警闪烁。
- 进水态会在 `APP_LedBar()` 中覆盖为整体翻转闪烁。

## 3. 与需求的差距
1. `LedBar` 内缺少“开机/关机动画状态机”。
2. `LedBar` 内缺少“短按触发电量显示（1s亮+1s灭，共5次）”流程。
3. `PB14` 与 `PC13` 在输入定义上存在语义混用风险，需要统一“灯板干簧管”的最终输入源。

## 4. 需求落地建议（按100ms Tick）

> 建议统一：**灯板干簧管输入使用 `PC13`（DI1/gan3）**，与开关机功能一致。

### 4.1 关键计数参数
- `T_LONG_PRESS_ONOFF = 30 tick`（3.0s）
- `T_SHORT_PRESS_MAX = 10 tick`（1.0s）
- `T_SHORT_SHOW_ON = 10 tick`（1.0s）
- `T_SHORT_SHOW_OFF = 10 tick`（1.0s）
- `N_SHORT_SHOW_CYCLE = 5`

### 4.2 建议新增状态
在 `LedBar.c` 增加独立显示状态机（示例）：
- `LED_UI_IDLE`
- `LED_UI_BOOT_ANIM`
- `LED_UI_SHUTDOWN_ANIM`
- `LED_UI_SHORT_SHOW_ON`
- `LED_UI_SHORT_SHOW_OFF`

### 4.3 建议输入事件
- `EV_KEY_PRESS`
- `EV_KEY_RELEASE`
- `EV_KEY_LONG_3S`
- `EV_KEY_SHORT_LT_1S`

### 4.4 行为定义

#### A. 长按3秒开/关机
- 条件：干簧管连续闭合达到 30 tick。
- 触发后必须“释放后再允许下次触发”（防止持续闭合重复触发）。
- 开机时：执行 `LED1->LED5` 逐亮。
- 关机时：先全亮，再 `LED5->LED1` 逐灭。

#### B. 开机状态短按显示电量
- 条件：按下后在 `< 10 tick` 释放。
- 进入 `短按电量显示序列`：
  - `1秒显示SOC` -> `1秒全灭`，计为1次；
  - 共5次后自动退出到常规显示。

## 5. 冲突优先级（建议）
从高到低：
1. 进水告警覆盖
2. 严重故障告警覆盖
3. 开/关机动画
4. 短按电量显示序列
5. 常规充/放/静置显示

说明：高优先级到来时可抢占低优先级；高优先级退出后恢复低优先级状态。

## 6. 参考实现框架（伪代码）
```c
// 100ms调用
void APP_LedBar(void) {
    if (!tick100ms) return;

    sample_key_pc13();                // 形成 press/release/short/long 事件
    process_power_toggle_event();     // 长按3s触发开/关机事件（并要求释放再重触发）

    if (is_water_in()) { show_water_alarm(); return; }
    if (is_fault_active()) { show_fault_alarm(); return; }

    switch (ui_state) {
      case LED_UI_BOOT_ANIM: run_boot_anim(); break;
      case LED_UI_SHUTDOWN_ANIM: run_shutdown_anim(); break;
      case LED_UI_SHORT_SHOW_ON:
      case LED_UI_SHORT_SHOW_OFF: run_short_show_1s_on_1s_off_5cycles(); break;
      default: run_normal_soc_or_chg_dsg(); break;
    }
}
```

## 7. 验收用例
1. 休眠/关机状态：连续闭合 3.0s，触发开机动画（LED1->LED5逐亮）。
2. 开机状态：连续闭合 3.0s，触发关机动画（全亮后 LED5->LED1逐灭）。
3. 开机状态：闭合 0.3s 后释放，进入“1秒亮+1秒灭”循环，累计5次后退出。
4. 长按不释放场景：触发一次后不应再次触发，必须释放后再长按才能触发下一次。
5. 充电/放电中短按：短按显示优先于常规SOC显示，5次结束后恢复原显示。
6. 进水/故障场景：应覆盖并中断普通显示与短按显示。

## 8. 与现有代码的对应关系
- 显示主循环：`Code/Source/LedBar.c`
- LED宏定义：`Code/Source/LedBar.h`
- 干簧管输入函数：`Code/Source/gan_huang_guan_logi.c`
- 开关机按键逻辑（DI1/PC13）：`Code/Source/IO_Control.c`
- 100ms系统时基：`Code/Source/System_Init.c` + `APP_LedBar()` 的 `b1Sys100msFlag`

## 9. 实施注意事项
1. 先统一“灯板干簧管”输入定义（推荐 `PC13`），避免 `PB14`/`PC13` 双源冲突。
2. 如果开/关机动作仍由 `IO_Control.c` 触发，则 `LedBar` 负责动画执行；两边通过状态标志同步。
3. 动画期间建议锁定普通SOC刷新，避免闪烁冲突。
4. 所有定时均基于 100ms tick，参数统一宏定义，便于标定。

## 10. 本次实现说明
- 实现文件：`Code/Source/LedBar.c`
- 输入信号：使用 `MCUI_SOC_KEY`（`PB14`，低电平表示闭合）
- 已实现：
  - 长按 3 秒触发 `Sleep_Mode.bits.b1ForceToSleep_L3 = 1`，并执行开/关机动画
  - 长按触发后必须释放，才允许下一次触发
  - 短按（<=1 秒）触发 `1秒显示SOC + 1秒熄灭`，循环 5 次后停止显示
  - 常态显示改为不持续点亮 SOC（由短按触发显示）
