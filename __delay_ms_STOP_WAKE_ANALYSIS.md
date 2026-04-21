# __delay_ms 在 STOP 唤醒后的可用性分析（理论）

## 1. 问题背景
- `STOP mode` 唤醒后，STM32F0 硬件默认系统时钟回到 `HSI`。
- 本工程 `__delay_ms` 由 `SysTick` 轮询实现，系数由 `InitDelay()` 根据 `SystemCoreClock` 计算。

相关实现位置：
- `Code/Source/System_Init.c`：`InitDelay()`、`__delay_ms()`
- `Code/Source/SleepDeal.c`：`Sys_StopMode()`
- `Code/Drivers/stm32f0xx.h`：当前时钟宏选择

## 2. __delay_ms 的计时机理
`InitDelay()` 中：
- `SysTick->CTRL &= ~(1<<2)`，即 `CLKSOURCE=0`，SysTick 时钟使用 `HCLK/8`。
- 再根据 `SystemCoreClock` 设定 `fac_ms`：
  - 8MHz -> `fac_ms = 1000`
  - 12MHz -> `fac_ms = 1500`
  - 48MHz -> `fac_ms = 6000`

`__delay_ms(ms)` 中：
- `SysTick->LOAD = ms * fac_ms`
- 轮询 `COUNTFLAG` 到期

因此延时精度由两部分决定：
1) `fac_ms` 是否与当前 `HCLK` 一致
2) 当前时钟源本身的频率误差

## 3. STOP 唤醒后是否必然出问题
结论：不必然。

### 场景A：当前工程默认配置（_HSE_NOPLL_8M）
- 休眠前常用 HSE 8MHz，唤醒后变 HSI 8MHz。
- 频率名义值同为 8MHz，`fac_ms=1000` 仍匹配数量级。
- 所以**不会出现 6 倍这类严重失准**。

但会有精度差异：
- HSE（晶振）通常 ppm 级更稳定。
- HSI（内部 RC）误差更大，随温度/电压漂移明显。
- 所以 `__delay_ms` 会有**小到中等比例误差**（取决于芯片实测与环境）。

### 场景B：若工程是 48MHz PLL 运行
- 若 `STOP` 唤醒后停在 HSI 8MHz，但 `fac_ms` 仍按 48MHz（6000）未重算，
- 则 `__delay_ms` 会出现**大幅失准（约 6 倍级别）**。

## 4. 你当前代码路径判断
- 你的时钟宏为 `_HSE_NOPLL_8M`（`Code/Drivers/stm32f0xx.h`）。
- `Sys_StopMode()` 仅在 `_HSE_8M_PLL_48M/_HSE_12M_PLL_48M` 下有唤醒后切回 PLL 代码。
- 因此当前唤醒后保持 HSI 的判断成立。

对 `__delay_ms` 的影响：
- 当前配置下通常“可用”，但绝对精度较 HSE 时变差。

## 5. 风险分级
- 功能级风险（是否能工作）：低（当前 8MHz->8MHz）。
- 精度级风险（是否精确 1ms）：中（HSI 漂移导致误差）。
- 可移植风险（后续改时钟方案）：高（若改到 PLL 频率且不重算系数会明显出错）。

## 6. 建议
1. 若业务只需粗延时（去抖、等待若干 ms）：当前可接受。
2. 若业务依赖精确定时（严格协议窗口、精密采样节拍）：
   - 唤醒后显式恢复到目标系统时钟（如 HSE/PLL），并
   - 执行 `SystemCoreClockUpdate(); InitDelay();`
3. 为防后续移植踩坑，建议在 `Sys_StopMode()` 唤醒后统一调用一次时钟恢复与 delay 基准重建（即使当前是 8MHz 档也可保持一致性）。

## 7. 一句话结论
- 当前配置下，`__delay_ms` 一般**不会功能失效**，但会受 HSI 精度影响；
- 若未来切到 PLL 主频且不重建 delay 基准，`__delay_ms` 将出现显著误差。
