# RS485 按需供电与通信窗口

## 行为及配置

- USART2 对应 485；PA8（GPIO_M_CTR）高电平供电，PB1 控制发送方向，PB14（PIN_INT_WK_CMNT）上升沿唤醒。
- 默认 PA8 低，USART2 不接收；PB14 上升沿清理旧帧并立即拉高 PA8，开启通信窗口。
- `Code/Source/Sci_Upper.h` 的 `RS485_POWER_WINDOW_SECONDS` 默认 30 秒，支持编译宏覆盖，范围 1～86400 秒；修改后重新编译。不新增 EEPROM 参数或上位机寄存器。
- 窗口是滑动的无有效通信超时：例如唤醒后第 20 秒收到有效请求，则最早第 50 秒断电。
- 只有 USART2 上校验通过且业务处理成功的 Modbus 请求，或校验通过且可生成应答的 P12 请求，刷新窗口。串口1、噪声、半帧、错误地址、校验失败、业务拒绝及供电期间重复 PB14 边沿不续期。
- 10ms 硬件定时中断维护时间基准，主循环检查超时。使用无符号差值处理计数回绕；实际断电存在 10ms 量化与主循环调度延迟。
- 超时后清除半帧并关闭接收，PA8 拉低。已完成收帧的处理与正在发送的应答先完成；异常发送沿用原有约 200ms 看门狗。不会在正常应答的最后停止位之前切断电源。

## 启动及休眠衔接

普通启动和 STOP 前都配置 PB14/EXTI14 上升沿。STOP 唤醒后沿用原工程复位恢复时钟的流程，通过现有 BootFlag 正反码备份机制写入 0x1237；初始化 GPIO、USART 后消费标记，重新开启完整通信窗口。检查与 WFI 之间暂时屏蔽中断，避免唤醒已被处理后再次睡下。

通信窗口阻止普通空闲休眠提前关闭电路；过流、压差、CBC、过压、欠压、测试和显式强制休眠仍按原优先级执行。这里的 STOP 恢复没有增加 Flash 擦写，也不使用显示状态占用的 BKP3R/BKP4R。

## 底层依据

核对本机官方标准库：
`C:/Users/Administrator/Downloads/STM32F0xx_StdPeriph_Lib_V1.5.0/STM32F0xx_StdPeriph_Lib_V1.5.0/Libraries/`

- GPIO_SetBits / GPIO_ResetBits / GPIO_WriteBit：通过 BSRR/BRR 设置输出。
- EXTI_Init、EXTI_GetITStatus、EXTI_ClearITPendingBit：配置上升沿并写 PR 清除挂起位。
- USART_ReceiveData、USART_ClearFlag：读取 RDR，向 ICR 写错误标志。
- GPIO_StructInit 初始化配置结构，避免休眠输出配置使用未初始化的上下拉字段。

## 验证及构建

`python tools/test_sci_transport.py` 通过：编译实际 C 传输和电源控制函数，模拟 USART/GPIO，验证默认断电、唤醒、30 秒边界、重复边沿、串口隔离、有效/拒绝请求、P12、半帧清理、发送延后断电、PRIMASK 恢复、计数回绕。原有发送长度、TXE/TC、发送看门狗、接收边界与双串口测试继续通过。协议业务采用桩函数，不代表完整协议内容或板上电气验证。

工程 `CommomBQ769x0_16series_030C8T6_C.uvprojx`，目标 `Target 1`，STM32F030C8 / ARMCC：编译成功。首次构建 0 错误、32 警告；最终 SleepDeal 增量编译 0 错误、3 警告，用时 1 秒。最终警告为原有 BLE/SWT 数字与枚举混用及 IsDI1Pressed 未使用；增量警告数不代表全部工程无其他警告。

最终大小：Code=36228，RO-data=3292，RW-data=656，ZI-data=6432；Flash 合计 40176 字节，RAM 合计 7088 字节。

产物位于 Objects：同工程名 AXF 815968 字节、HEX 111455 字节、BIN 39608 字节。BIN 与 Flash 汇总口径不同，以链接器汇总评估资源占用。

build-keil 技能脚本缺少 tool_config 模块，使用本机 C:/Keil_v5/UV4/UV4.exe 完成等价构建。临时日志和主机测试中间文件仅在 LOCALAPPDATA/CodexTemp/030-TI 下。

## 板上验收

尚未烧录或实测功耗。建议同时观察 PA8、PB14、PB1、USART2_TX：验证冷启动低电平、唤醒供电、连续有效请求续期、停止请求约 30 秒断电、持续错误帧不续期以及 STOP 唤醒后通信。主机应先触发唤醒，再等待电源和固件就绪后发送请求；供电建立时间及 STOP 后复位启动时间需通过实测确定，不保证紧随唤醒边沿的首帧可接收。

工作区已有的其他参数、待办、BLE/SWT 休眠修改和生成文件保留；本次提交只纳入 485 相关代码、测试与说明。
