# 串口架构重构与串口2 RS485方向控制

## 范围与结论

将两路重复的收帧、错误处理和主循环状态机合并为公共实现，保留现有业务处理、寄存器映射、CRC、P12校验与应答内容。串口1仍为19200/8N1、Modbus；串口2仍为115200/8N1、Modbus + P12。

GPIO_M_STB/PIN_M_STB（PB1）现在用于串口2 RS485方向控制：高电平发送，低电平接收。纳入工作区原有的 InitIO 低电平初始化修改，串口初始化时也设置为接收方向。

## 原架构与重复状态

原实现每路有独立消息缓冲、通信错误计数、发送使能、发送完成标志和接收超时计数。USART中断负责逐字节接收、识别帧长度、TXE发送和TC完成；主循环校验帧、执行命令、构造应答并恢复接收。

App_CommonUpperSCI1/2、错误处理和Modbus接收判断大部分重复。发送完成标志与消息的TX_COMPLETE状态重复，发送使能与TX_BUSY状态重复；错误计数只被判断是否非零，没有统计用途，还存在累加回绕的可能。

## 当前分层

| 层次 | 入口与职责 |
| --- | --- |
| 硬件配置 | InitSCI1_CommonUpper/InitSCI2_CommonUpper 保留时钟、引脚、波特率等硬件差异 |
| 端口上下文 | SciPort 绑定USART、消息缓冲、P12能力及接收错误/超时状态 |
| 中断适配 | 原两路IRQ调用无参数包装函数，避免消息缓冲与USART传错配对 |
| 接收传输 | Sci_RxISR_Deal 共用收帧、缓冲边界、完成后关闭接收逻辑 |
| 错误处理 | Sci_FaultChk 使用官方命名位，直接写ICR清除错误并记录rxFault |
| 调度 | Sci_Service 共用超时和收发状态机 |
| 协议业务 | Sci_ProcessRequest 显式按协议分发校验、命令执行 |
| 应答构造 | Sci_PrepareResponse 显式按协议选择应答；未知协议拒绝发送 |
| 发送传输 | Sci_StartTx启动，Sci_TxISR_Deal处理TXE/TC，Sci_SetTxDirection控制方向 |

删除 gu8_TxEnable_SCI1/2、gu8_TxFinishFlag_SCI1/2。gu16_CommuErrCnt_SCI1/2改为各端口的volatile UINT8 rxFault；超时计数归入上下文。ptr_no、csr作为中断和主循环共享字段加volatile。协议缓冲仍每路独立，原共享g_u8SCITxBuff仍用于主循环串行构造Modbus数据，不能简单删除，也不能搬到两路中断里并行使用。

## 状态及485时序

IDLE（含半帧接收）→ RX_COMPLETE → RX_OK → TX_BUSY → TX_COMPLETE → IDLE。

1. 完整接收后关闭RE/RXNEIE，主循环处理当前帧，缓冲不再被接收覆盖。
2. 应答长度有效时关闭接收及发送中断，设置TX_BUSY，将PB1拉高，清旧TC并开启TXEIE。
3. 首字节和后续字节均由同一TXE逻辑写TDR，消除启动阶段重复的发送逻辑。相较原实现增加首字节的一次TXE中断，不阻塞主循环。
4. 最后一字节写TDR后仅关闭TXEIE、开启TCIE并返回，PB1继续保持高。不在末字节写入后额外清TC，避免延迟抢占后误清已完成事件。
5. 后续TC中断确认完整发送结束，才将PB1拉低，发布TX_COMPLETE，并保留原有Flash升级标志交接。
6. 主循环复位帧并重新开启RE/RXNEIE。因此，TC后方向立即释放，但软件接收重新开放仍受主循环调度影响，保持原有单帧请求/响应方式。

接收超时仍为三个10ms调度节拍，并非严格从最后字节起计时30ms。超时判断和清帧使用保存/恢复PRIMASK的短临界区，避免判断超时后，接收中断恰好发布完整帧却被清除。校验、应答构造及业务执行不在该临界区内。

无效帧、超限帧及超时丢弃时同时清除该帧错误标志，避免错误残留导致下一帧被额外丢弃。这是明确的异常恢复改进；合法报文业务处理保持原逻辑。原有广播回复习惯、CRC错误应答习惯也保持不变，不借此重构改变协议兼容性。

## 后续维护

- 增加Modbus命令：在公共收帧分支补充长度规则，在Sci_ProcessRequest添加业务分发，在Sci_PrepareResponse选择应答构造。寄存器修改继续维护原Sci_Deal_*及Sci_ACK_*函数。
- 增加协议：在Sci_Upper.h增加协议标识，在Sci_RxISR_Deal增加首字节识别、长度与边界规则，再给Sci_ProcessRequest和Sci_PrepareResponse增加显式分支。当前supportsP12是两种协议的轻量端口配置；协议数量增加时可演进为能力掩码，不必提前引入函数指针框架。
- 修改串口硬件/波特率：在对应初始化函数修改；修改485方向引脚/极性：集中修改Sci_SetTxDirection及InitIO初始化电平。
- 中断只负责传输，不在中断内执行EEPROM、CRC遍历、应答构造等业务。不要根据ptr_no==AckLenth或TXE释放485方向。
- 当前printf/fputc仍为既有USART1阻塞调试路径，不能与USART1协议发送混用；若未来重定向到USART2，必须另行接入统一发送所有权与方向控制，不能只改debug_uart宏。

## 官方库依据

本次参考本机官方库：

`C:\Users\Administrator\Downloads\STM32F0xx_StdPeriph_Lib_V1.5.0\STM32F0xx_StdPeriph_Lib_V1.5.0\Libraries\STM32F0xx_StdPeriph_Driver`

- src/stm32f0xx_usart.c：USART_SendData通过TDR写入数据；USART_ClearFlag和USART_ClearITPendingBit直接写ICR；TXE为数据寄存器空，TC为发送完成。
- inc/stm32f0xx_usart.h：USART_FLAG_TC、ORE/NE/FE/PE及USART_IT_TC的定义。
- src/stm32f0xx_gpio.c：GPIO_SetBits/ResetBits通过BSRR/BRR设置/复位引脚。

中断保留基于官方位定义的直接寄存器访问，避免逐字节路径增加不必要的库函数开销。方向控制使用标准库GPIO接口。

## 验证与资源结果

工程：CommomBQ769x0_16series_030C8T6_C.uvprojx；目标：Target 1；器件：STM32F030C8；工具链：ARMCC V5.06 update 7 build 960。

| 项目（字节） | 修改前 | 最终版本 | 差值 |
| --- | ---: | ---: | ---: |
| Code | 39184 | 38580 | -604 |
| RO-data | 2668 | 2668 | 0 |
| RW-data | 676 | 692 | +16 |
| ZI-data | 6364 | 6364 | 0 |
| Flash逻辑合计（Code+RO+RW） | 42528 | 41940 | -588 |
| RAM合计（RW+ZI） | 7040 | 7056 | +16 |

减少重复代码和状态变量不等于RAM一定下降：这里用16字节额外RAM换取显式端口绑定和公共实现。Flash合计按链接器分项计算，不等同于压缩初始化数据后的BIN长度。未测量中断周期，不声称吞吐提升。

最终全量重编译：0错误、44警告，耗时4秒；基线0错误、44警告，耗时5秒。既有警告包括P12版本函数中未使用变量及param.c指针类型不匹配，没有新增警告。最终产物AXF 849032字节、HEX 116323字节、BIN 41340字节。工作期间检测到DataDeal.h的SNum由8改为12，已在最终构建副本中同步验证；该用户修改不包含在本次提交。

构建技能脚本因缺少tool_config模块不能启动，使用本机UV4.exe相同命令行完成基线和最终构建。隔离副本、构建日志和产物位于：

`%LOCALAPPDATA%\CodexTemp\030-TI\sci-refactor-20260908\`

其中baseline-build.log为基线，after-final-build.log为最终全量构建，after/Objects为最终产物。项目内没有新建临时目录。复现构建时请复制源码到用户临时区再运行Keil，工程原ListingPath指向工程根目录。

运行 `python tools/test_sci_transport.py`：编译实际C传输函数并模拟USART/GPIO，验证1/8/251字节发送、TXE后保持方向、TC后释放、0x03/0x06及0x10长度边界、P12范围、半帧超时、错误恢复、PRIMASK恢复、协议分发和两路交错收帧，全部通过。测试业务分发使用桩，不代表实际参数读写或P12内容完成端到端验证。

尚未上板验收：示波器同时观察USART2_TX和PB1，确认PB1上升早于首起始位，下降晚于末停止位；分别验证两路Modbus、串口2 P12、连续请求与两路并发、升级应答完整性。还需根据实际485收发器确认使能建立时间，不能由主机寄存器模拟证明电气时序。
