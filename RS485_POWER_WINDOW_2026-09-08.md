# RS485 按需供电与通信窗口

## 行为及配置

- USART2 对应 485；PA8（GPIO_M_CTR）高电平供电，PB1 控制发送方向，PB14（PIN_INT_WK_CMNT）上升沿唤醒。
- 默认 PA8 低，USART2 不接收；PB14 上升沿清理旧帧并立即拉高 PA8，开启通信窗口。
- `Code/Source/Sci_Upper.h` 的 `RS485_POWER_WINDOW_SECONDS` 默认 30 秒，支持编译宏覆盖，范围 1～86400 秒；修改后重新编译。不新增 EEPROM 参数或上位机寄存器。
- 窗口是滑动的无有效通信超时：例如唤醒后第 20 秒收到有效请求，则最早第 50 秒断电。
- 只有 USART2 上校验通过且业务处理成功的 Modbus 请求，或校验通过且可生成应答的 P12 请求，刷新窗口。串口1、噪声、半帧、错误地址、校验失败、业务拒绝及供电期间重复 PB14 边沿不续期。
- 10ms 硬件定时中断维护时间基准，主循环检查超时。使用无符号差值处理计数回绕；实际断电存在 10ms 量化与主循环调度延迟。
- 超时后清除半帧并关闭接收，PA8 拉低。已完成收帧的处理与正在发送的应答先完成；异常发送沿用原有约 200ms 看门狗。不会在正常应答的最后停止位之前切断电源。

## 使用范围：仅激活运行中的 485 通信

PB14/EXTI14 仅在正常 USART2 初始化完成后配置，用于开启 485 电路供电。串口尚未初始化时，激活回调直接返回。

STOP 启动路径不配置 PB14 唤醒；已删除上次增加的 485 BootFlag 0x1237、唤醒后的复位处理、跨复位窗口恢复，以及 STOP 等待循环中的 485 条件判断。休眠流程也不再因通信窗口而延迟，按原有休眠策略运行。保留休眠时 PA8 拉低的省电输出配置。

当前工程进入 STOP 前先复位，再走 IsSleepStartUp，早于正常 InitUSART_CommonUpper；因此 STOP 路径不会启用 EXTI14。PB14 不能唤醒 STOP 中的 MCU，须由原有其他唤醒源恢复运行后再激活 485。

## 底层依据

核对本机官方标准库：
`C:/Users/Administrator/Downloads/STM32F0xx_StdPeriph_Lib_V1.5.0/STM32F0xx_StdPeriph_Lib_V1.5.0/Libraries/`

- GPIO_SetBits / GPIO_ResetBits / GPIO_WriteBit：通过 BSRR/BRR 设置输出。
- EXTI_Init、EXTI_GetITStatus、EXTI_ClearITPendingBit：配置上升沿并写 PR 清除挂起位。
- USART_ReceiveData、USART_ClearFlag：读取 RDR，向 ICR 写错误标志。
- GPIO_StructInit 初始化配置结构，避免休眠输出配置使用未初始化的上下拉字段。

## 验证及构建

`python tools/test_sci_transport.py` 通过：编译实际 C 传输和电源控制函数，模拟 USART/GPIO，验证初始化前激活无效、默认断电、运行时激活、30 秒边界、重复边沿、串口隔离、有效/拒绝请求、P12、半帧清理、发送延后断电、PRIMASK 恢复、计数回绕。原有发送长度、TXE/TC、发送看门狗、接收边界与双串口测试继续通过。协议业务采用桩函数，不代表完整协议内容或板上电气验证。

工程 `CommomBQ769x0_16series_030C8T6_C.uvprojx`，目标 `Target 1`，STM32F030C8 / ARMCC：本次编译成功，0 错误、30 警告，用时 5 秒；工程现有警告未在本次统一清理。

最终大小：Code=36100，RO-data=3292，RW-data=656，ZI-data=6432；Flash 合计 40048 字节，RAM 合计 7088 字节。

产物位于 Objects：同工程名 AXF 815112 字节、HEX 111095 字节、BIN 39480 字节。BIN 与 Flash 汇总口径不同，以链接器汇总评估资源占用。

build-keil 技能脚本缺少 tool_config 模块，使用本机 C:/Keil_v5/UV4/UV4.exe 完成等价构建。临时日志和主机测试中间文件仅在 LOCALAPPDATA/CodexTemp/030-TI 下。

## 板上验收

尚未烧录或实测功耗。建议同时观察 PA8、PB14、PB1、USART2_TX：验证冷启动低电平、唤醒供电、连续有效请求续期、停止请求约 30 秒断电、持续错误帧不续期以及 STOP 中 PB14 上升沿不会唤醒 MCU。主机应先触发唤醒，再等待电源和固件就绪后发送请求；供电建立时间需通过实测确定，不保证紧随唤醒边沿的首帧可接收。

工作区已有的其他参数、待办、BLE/SWT 休眠修改和生成文件保留；本次提交只纳入 485 相关代码、测试与说明。

## 2026-09-09 方向时序更新

USART2 新增可调发送前等待和 TC 后保持，默认 SETUP=0µs、HOLD=1000µs。HOLD 结束才切回接收；电源超时不会截断这一阶段。参数、TIM14 计时及最新构建结果见 RS485_DIRECTION_DELAY_2026-09-09.md。
