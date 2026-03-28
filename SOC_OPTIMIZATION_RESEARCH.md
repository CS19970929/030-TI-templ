# SOC 模块调研与优化规划

## 1. 目标

- 提升 SOC 精度，减少长周期漂移、满电不满/空电不空、静置后跳变等问题。
- 提升用户体验，避免开机乱跳、末端突变、长期卡 100%/0%、休眠恢复不可信。
- 简化当前 SOC 初始化、状态机、EEPROM 存储逻辑，降低维护成本和联动风险。

## 2. 当前实现概况

当前项目的 SOC 方案本质上是三段式混合算法：

- OCV 查表：按最低单体电压 `u16_VCellMin` 查表得到 SOC 基准。
- 安时积分：按 200ms 时基对充放电电流做积分。
- 末端强制校准：在接近满电/空电阈值时，用经验规则把 SOC 往 100% 或 0% 拉。

代码入口与关键点：

- 调度入口：[Code/Source/SOC.c](Code/Source/SOC.c)
- 算法主体：[Code/Source/SocEnhance.c](Code/Source/SocEnhance.c)

关键逻辑：

- `RefreshData_SOC()` 将最低单体电压、最高单体电压、充放电电流喂给 SOC 模块。
- `Get_OpenCircuit_Value()` 直接按 `u16_VCellMin` 查 OCV 表。
- `CorrectionTerminal_CV()` 通过多级门限对末端 SOC 做强拉。
- `SOC_State_Transfer()` 在充电、放电、静置间切换。
- `SOC_Update_StartUp()` 依据 EEPROM 标志决定开机 SOC。
- `SOC_EEPROM_Deal_Monitor()` 周期性写 SOC、循环计数等数据。

## 3. 当前方案的主要问题

### 3.1 精度问题

- OCV 基准直接使用 `VCellMin`，更接近“最差单体保护视角”，不等于整包可用容量视角。
- 没有严格的“静置判定 + OCV 有效性判定”，静置校准条件偏弱。
- 安时积分缺少系统零漂、板载静态功耗、自耗电补偿，长期一定会漂。
- 温度、倍率、老化对容量的影响没有进入主模型，只是零散靠规则修补。
- `CorrectionTerminal_CV()` 直接修改 `SOC_Now` 和 `CapNow`，本质是“显示值修正”和“物理容量估计”混在一起。

### 3.2 用户体验问题

- 首次上电默认给 `60%`，不是基于真实状态，风险很高。
- 末端靠强拉，容易出现“长时间不动，突然跳 1%”或“快满时爬升过快”。
- 开机、休眠恢复、参数刷新共用一套复杂分支，某些路径下显示可信度不一致。
- `SOC_Fixed` / `SOC_Zero` 这种展示覆盖逻辑和算法输出耦合在同一路径，不利于后续维护。

### 3.3 架构问题

- 初始化、运行态估算、展示输出、容量学习、EEPROM 持久化都堆在同一模块里。
- EEPROM 用多槽轮转保存 `SOC`、`DSG_SOC_Int`、循环次数，但没有清晰版本化、校验、原子提交概念。
- `SeriousFaultFlag` 既像状态标志又像流程入口，语义不清。
- 状态机只覆盖“充/放/静”，没有覆盖“未校准、可信、待校准、学习中、异常回退”等业务状态。

## 4. 主流 BMS / Fuel Gauge 做法

行业主流不是单独依赖某一种方法，而是组合式估算：

- 短期用库仑计积分保证动态响应。
- 静置或弱负载时用 OCV/模型估算修正积分漂移。
- 结合温度、倍率、老化、内阻变化修正可用容量。
- 将“内部真实 SOC”和“对外显示 SOC”分层，显示层增加防跳变策略。
- 用学习机制更新有效满充容量、等效内阻、循环寿命参数。

参考资料：

- TI 电量计产品综述：强调其算法会面向不同电池体系提供更高精度估算，并有 Dynamic Z-Track / Impedance Track 等面向动态负载的方案  
  <https://www.ti.com/product-category/battery-management-ics/battery-fuel-gauges/overview.html>
- ADI DS2786 OCV 方案：明确指出 OCV 估算要基于静置后的开路电压，静置后可立即给出容量基准  
  <https://www.analog.com/en/resources/technical-articles/interpreting-the-opencircuitvoltage-ocv-fuel-gauge-of-the-ds2786.html>
- ADI Fuel Gauge 总结：纯电压法通常难以优于 25% 精度，实际高精度方案需要库仑计和持续学习  
  <https://www.analog.com/en/resources/technical-articles/2022/07/16/09/50/battery-fuel-gauges-accurately-measuring-charge-level.html>
- ADI ModelGauge m5：将库仑计短期精度和电压法长期稳定性结合，同时补偿温度、老化、放电倍率  
  <https://www.analog.com/en/resources/technical-articles/get-enhanced-safety-accurate-stateofcharge-and-longer-runtime-for-your-portable-device-battery.html>
- ADI 电芯标定建议：要在不同温度和不同负载下做电芯表征，才能得到可靠 SOC 参考模型  
  <https://www.analog.com/en/resources/design-notes/2022/07/16/10/58/cell-characterization-procedure-for-a-modelgauge8482-m3-fuel-gauge.html>
- Renesas 多串燃料计：集成高精度 ADC、库仑计和剩余容量估算，说明量产方案普遍将测量、估算、保护联合设计  
  <https://www.renesas.com/en/products/power-management/battery-management/battery-fuel-gauges/raj240091gnp-3-7-series-li-ion-battery-fuel-gauge-ic>

## 5. 对当前项目的建议方向

### 5.1 建议一并优化初始化和存储逻辑

建议，不要只改积分和 OCV 部分。

原因：

- 当前 SOC 精度问题和用户体验问题，很多不是估算公式本身导致，而是“错误初始化 + 不清晰恢复 + 过度写存储 + 末端强拉”共同造成的。
- 如果只调参数，不重构初始化和存储，后续会继续出现“实验室看起来行，现场长期跑不稳”的问题。
- 初始化、恢复、持久化属于 SOC 可信度的基础设施，不先理顺，后续再引入学习容量或温度补偿会更乱。

但实施上建议分阶段，不要一次性推翻。

## 6. 推荐的目标架构

### 6.1 模块拆分

建议拆成 4 层：

1. `soc_measure`
- 输入电压、电流、温度、休眠/唤醒状态、时间基准。
- 做滤波、零电流判定、静置判定、有效测量窗口判定。

2. `soc_core`
- 保存真实 SOC 状态。
- 负责 OCV 初始化、库仑积分、静置校准、容量学习、SOH 更新。

3. `soc_store`
- 负责 EEPROM/NV 读写、CRC、版本号、双缓冲/序号管理。
- 存的是“可信状态快照”，不是一堆散变量。

4. `soc_display`
- 负责对外显示 SOC 的平滑、防回跳、快满/快空策略。
- 不直接污染核心容量估算。

### 6.2 状态机重构

建议核心状态至少分为：

- `UNINITIALIZED`
- `INIT_FROM_OCV`
- `INIT_FROM_STORE`
- `TRACK_CHARGE`
- `TRACK_DISCHARGE`
- `RELAX`
- `RECALIBRATE`
- `FAULT_DEGRADED`

展示层再单独区分：

- `DISPLAY_NORMAL`
- `DISPLAY_HOLD`
- `DISPLAY_RAMP_UP`
- `DISPLAY_RAMP_DOWN`

## 7. 具体优化策略

### 7.1 初始化优化

现状问题：

- 首次上电固定 `60%` 风险极高。
- 掉电恢复和休眠恢复路径重复。
- 参数刷新和正常启动复用了太多逻辑。

建议：

- 首次启动改为“受限 OCV 初始化”：
  - 如果满足近静置条件，用 OCV 表给初值。
  - 若不满足静置条件，则给“低可信初值”，并在后续静置窗口做一次正式校准。
- 将初始化可信度分级：
  - `HIGH`：来自有效静置 OCV 或可信存储。
  - `MEDIUM`：来自普通存储恢复。
  - `LOW`：首次启动或异常回退。
- 将 `PowerOff`、`SleepWake`、`ParamChanged`、`FactoryReset` 四类启动原因独立处理，不再复用一个标志位跳转。

### 7.2 存储逻辑优化

现状问题：

- 分散字段多，状态含义不清。
- 缺少统一快照、版本、校验、原子性。
- 1% 变化就写，策略比较粗。

建议：

- 改为结构化快照存储：
  - `soc_percent`
  - `remaining_as`
  - `full_cap_as`
  - `cycle_count_x100`
  - `init_confidence`
  - `reason`
  - `timestamp_or_seq`
  - `crc`
- 采用双槽或三槽日志式提交：
  - 写新记录
  - 校验通过
  - 以序号最新为准
- 写入触发改为组合条件：
  - SOC 变化超过阈值
  - 充放电状态切换
  - 即将休眠/关机
  - 满充/空放校准完成
  - 周期性保底落盘

### 7.3 SOC 核心算法优化

建议保留“OCV + 积分”的主框架，但改为更清晰的融合模型：

- `SOC_real`
  - 内部真实估计值，高分辨率，允许小数。
- `SOC_display`
  - 对外显示值，整数，带节流和平滑。

核心规则：

- 动态阶段：以库仑积分为主。
- 静置阶段：仅在满足静置条件时，用 OCV 对 `SOC_real` 做有限幅度校正。
- 末端阶段：不再直接“硬加 1%”，改为基于端点条件的“校准钳位”。

推荐静置判定至少包含：

- 电流绝对值低于阈值，持续一段时间。
- 单体电压变化率 `dV/dt` 足够小。
- 当前无明显充电器接入/负载扰动。
- 温度在有效区间内。

### 7.4 满电/空电端点校准

当前 `CorrectionTerminal_CV()` 更像强制修饰显示。

建议改为端点事件：

- `FULL_EVENT`
  - 满足 CV 尾流小于阈值并持续一定时间，且单体达到满电电压窗口。
  - 触发后将 `SOC_real` 钳位至接近 100%，同时更新 `full_cap_as` 候选值。
- `EMPTY_EVENT`
  - 满足最低单体达到空电阈值且持续，且放电电流满足有效条件。
  - 触发后将 `SOC_real` 钳位至接近 0%。

不要在普通阶段持续强拉。

### 7.5 容量学习 / SOH

当前 `Correction_CapacityFull()` 为空，建议补起来，但第二阶段再做。

建议方法：

- 记录完整充满到接近放空的有效放电量。
- 在满足温度和倍率条件时更新 `u32CapFull`。
- 不直接一次跳变，采用缓慢收敛。
- SOH 由 `CapFull / CapFactory` 计算，且区分“估计 SOH”和“对外显示 SOH”。

### 7.6 用户体验优化

建议将“体验优化”明确放在显示层，而不是污染核心算法：

- 开机显示可快速恢复到上次可信值，但标记为待校准。
- 显示值避免短时间回跳。
- 充电时允许缓慢单调上升。
- 放电时允许缓慢单调下降。
- 若核心值和显示值偏差过大，分多步追赶，不瞬跳。

## 8. 建议实施顺序

### 第一阶段：低风险重构，优先落地

- 去掉首次启动固定 `60%`，改成受限 OCV 初始化。
- 把 `SOC_real` 和 `SOC_display` 分离。
- 重构 EEPROM 为统一快照结构。
- 将 `PowerOff` / `SleepWake` / `ParamChanged` 分开处理。
- 保留现有 OCV 表和积分主流程，先不引入复杂学习。

预期收益：

- 体验明显改善。
- 开机可信度提升。
- 后续改算法不会继续被存储链路拖累。

### 第二阶段：精度提升

- 增加静置判定和有限幅度 OCV 回正。
- 引入零电流偏置补偿、静态功耗补偿。
- 末端校准由“强拉计数器”改为“事件式钳位”。

预期收益：

- 长期漂移下降。
- 末端表现更自然。
- 实际可用容量和显示更接近。

### 第三阶段：学习与标定

- 增加有效满充容量学习。
- 按温度和倍率修正有效容量。
- 补充台架标定流程，建立不同化学体系表。

预期收益：

- 精度进一步提升。
- 长周期衰减后的表现明显改善。

## 9. 对当前项目的结论

结论是：

- 当前 SOC 模块值得优化，而且应当把初始化和存储逻辑一并优化。
- 不建议继续在现有 `CorrectionTerminal_CV()` 和 `SOC_Update_StartUp()` 上堆补丁。
- 最合适的路径是“保留 OCV + 库仑积分主框架，重构初始化/存储/显示分层，再补静置校准和容量学习”。

如果后续进入实改，建议先做第一阶段。第一阶段不追求理论最优，但能最快把“精度下限”和“用户体验下限”抬上来。
