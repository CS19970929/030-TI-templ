# AFE通信监控简化说明

## 修改目的与边界

MonitorAFE原先混合了两路AFE状态、失败积分、唤醒次数、缓存清零以及AFE/EEPROM故障休眠。本次按当前工程只有AFE1采样调用的实际情况简化，保留已有错误标志、通信协议和保护模块接口。底层I2C事务、总线解锁及CRC实现不变。

## 结构与变量

- MonitorAFE变为DataDeal.c私有函数，仅接收本次采样结果，删除无实际调用的AFE2分支及num参数。
- 删除u8IICFaultcnt1/2、u8WakeCnt1/2四个全局变量，使用私有AFE_MONITOR_STATE：retryTime、successCount、recovering。三者分别表示上次重试/故障起点、连续成功采样次数、是否处于恢复确认阶段。
- 删除Init_Registers及其故障时清零40字节采样缓存的逻辑。失败保留上一份有效数据，依靠通信故障标志表达失效，不把伪造的零值作为新采样。
- 删除App_AFEGet中未使用的ts_u8TempSel和过时的分段采样调用注释。
- 三段故障休眠流程合并为System_Monitor.c的App_CommunicationFaultSleep，由主循环独立调用。三个起始时间加一个有效位掩码，分别监督AFE1、AFE2、EEPROM，保持各故障源的持续时间互不串用。AFE2错误枚举、日志等全局兼容接口保留。

## 明确的行为变化

| 项目 | 旧行为 | 新行为 |
| --- | --- | --- |
| 故障确认 | 传输层锁存故障，上层失败积分循环 | 保留故障锁存；每次失败清除连续成功计数 |
| 恢复确认 | 失败积分、唤醒次数均减至零，时间依赖历史 | 连续3次完整采样成功后恢复；中间失败重新计数 |
| 首次正常启动 | 首次成功即可置通信正常 | 同样经过3次成功确认，不额外设置通信故障标志 |
| 重初始化 | 积分达到30时执行，唤醒次数最多实际21次 | 故障期间，每隔至少6秒、在失败采样处理时尝试一次；不再用次数永久禁止后续恢复 |
| 初始化完成 | 混在失败计数流程中 | 唤醒/初始化不直接判定恢复，仍需后续采样成功 |
| 休眠 | 6000次采样监控，200ms周期下约20分钟 | 独立运行节拍计满5分钟；不依赖采样次数 |
| 多故障同时到期 | 可能连续多次调用休眠 | 一轮只请求一次休眠；若函数返回且故障仍在，重新开始计时 |

其他AFE寄存器访问置位通信故障后，MonitorAFE也会重新开始成功确认。当前错误接口只是锁存标志，不能区分恢复阶段内每一笔非采样事务；本次“连续成功”明确指完整采样结果，不扩展为所有AFE事务的全局成功序列。

6秒由AFE_RETRY_INTERVAL_10MS配置，3次由AFE_RECOVERY_SAMPLES配置，5分钟由SYSTEM_FAULT_SLEEP_10MS配置。无需修改流程即可调整这些策略。业务重初始化会发送AFE配置命令；底层失败事务仍不自动重放。

## 时基与调度

复用TIM17维护的sys_time.cnt_10ms，将该成员声明为volatile，未新增中断计数器或修改寄存器。使用UINT16差值处理回绕（完整周期655.36秒）；主循环正常运行时，6秒与300秒阈值均在回绕周期内。该计时是MCU运行节拍，不是包含停钟休眠时间的RTC墙钟；长期阻塞主循环或停止中断仍会影响执行时刻。

采样因EEPROM写入、启动保护等原因暂停时，故障休眠检查仍执行；但唤醒重初始化仍在采样路径中进行，避免绕过原来的访问互斥条件。

## 验证与构建记录

运行python tools/test_afe_monitor.py，Visual Studio编译从实际生产文件提取的监控和休眠函数，测试通过：

- 首次启动、连续成功、间歇失败、外部访问报错后的重新确认。
- 6秒重试边界、重复调用不加速重试、故障锁存不回绕。
- 无AFE采样时EEPROM独立休眠、5分钟边界、故障清除重新计时。
- 不同故障源不能接力累加、多个故障同时到期只请求一次、16位节拍回绕。

Keil隔离全量构建成功：

| 项目 | 结果 |
| --- | --- |
| 工程/目标 | CommomBQ769x0_16series_030C8T6_C.uvprojx / Target 1 |
| target_mcu / toolchain | STM32F030C8T6 / ARMCC V5.06 update 7 build 960 |
| 错误/警告/耗时 | 0 / 43 / 5秒 |
| Code / RO-data / RW-data / ZI-data | 35676 / 3292 / 648 / 6432字节 |
| Flash / RAM链接统计 | 39616 / 7080字节 |
| AXF / HEX / BIN文件大小 | 846528 / 109896 / 39056字节 |
| artifact_kind | AXF（首选），另有HEX、BIN |
| artifact_path | C:\Users\Administrator\AppData\Local\CodexTemp\030-TI\afe-monitor-20260908\build\Objects\CommomBQ769x0_16series_030C8T6_C.axf |
| 构建日志 | C:\Users\Administrator\AppData\Local\CodexTemp\030-TI\afe-monitor-20260908\build.log |

build-keil技能脚本先前已确认缺失tool_config模块，本次沿用UV4隐藏窗口CLI进行隔离全量构建。所有中间产物均位于用户临时目录；主机测试目录为同级afe-monitor-tests。构建包含当前工作区已有修改，因此不以不同任务的构建统计宣称本次RAM/Flash净节省。

尚未烧录或实板测试。上板应验证断开AFE后重试、恢复后的三次确认、EEPROM故障不再随采样暂停，以及持续故障5分钟后的实际休眠行为。
