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

IDLE（含半帧接收）→ RX_COMPLETE → RX_OK → TX_BUSY → IDLE。TC中断直接收尾，已删除TX_COMPLETE中间状态。

1. 完整接收后关闭RE/RXNEIE，主循环处理当前帧，缓冲不再被接收覆盖。
2. 应答长度有效时关闭接收及发送中断，设置TX_BUSY，将PB1拉高，清旧TC并开启TXEIE。
3. 首字节和后续字节均由同一TXE逻辑写TDR，消除启动阶段重复的发送逻辑。相较原实现增加首字节的一次TXE中断，不阻塞主循环。
4. 最后一字节写TDR后仅关闭TXEIE、开启TCIE并返回，PB1继续保持高。不在末字节写入后额外清TC，避免延迟抢占后误清已完成事件。
5. 后续TC中断确认完整发送结束，执行Sci_FinishTx：保留原有Flash升级标志交接，关闭发送中断，清理帧和超时状态，重新开启RE/RXNEIE，然后将PB1拉低。
6. TC中断返回前消息已处于IDLE且软件接收已开放，不再等待主循环清帧；下一帧首字节即使先于主循环到来，也不会被二次复位丢弃。

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

## 后续修复：TC立即恢复接收与发送超时（2026-09-08）

本节记录针对问题3、4的增量修复，上述原始资源表保留作为前一次提交的验证记录。

### 发送超时

TIM17每个真实10ms节拍调用公开接口Sci_Tick10ms，各端口独立累计txTimeoutTick，仅在TX_BUSY时计数。SCI_TX_TIMEOUT_TICKS=20，对应正常定时调度下约190～200ms；当前最长251字节、19200波特率、8N1的线速发送时间约131ms，115200时约22ms，均有余量。改变最低波特率、帧长度或发送间隔后必须重新计算阈值。

到期时分为两种情况：

- 发送位置已达到非零应答长度，且硬件TC确实置位：调用正常完成入口，补做遗漏的TC中断处理。
- 其他情况：关闭发送/接收中断，USART_Cmd(DISABLE)清UE停止硬件操作，再清TE；清理软件帧状态，保持UE关闭时将485切回接收，最后使能USART接收。下次Sci_StartTx重新打开TE。故障帧被明确中止，不当作完整应答，不主动触发升级完成标志。

没有轮询等待TC，也没有在TC未到时仅拉低方向脚。超时故障中止可能截断故障帧，这是释放卡死总线的预期行为，正常发送仍必须等待TC。

依据本机官方库stm32f0xx_usart.c的USART_Cmd实现及[ST RM0360中UE定义](https://www.st.com/resource/en/reference_manual/dm00091010.pdf)：清UE立即停止输出、丢弃当前操作，配置保留；因此不能仅清TE便认定硬件已停止发送。

### 并发约束与范围

TIM17、USART1、USART2目前均为NVIC优先级0，彼此不抢占。超时处理是有界的寄存器操作和帧头清理，不执行协议业务。若以后修改这些中断的相对优先级，必须重新审核状态发布与清帧并发。全局长时间关中断或同级ISR长时间阻塞仍会推迟超时处理，本机制不能替代系统看门狗。

修复保留现有接收半帧超时、波特率、协议业务和ONEBIT配置。审核中的问题1（全局升级标志跨端口归属）及问题2（采样策略）未在本次范围内修复；全局升级标志仍有原有并发风险。

### 验证

扩展tools/test_sci_transport.py，直接编译实际C函数；验证TC之后不运行主循环即可接收下一字节、后续主循环不清掉新帧；模拟TXE中断缺失、末字节之后TC缺失、TC已置位但中断缺失，以及中止后重新发送。GPIO桩检查正常释放前RE/RXNEIE已开启，故障释放前UE/TE均已关闭；公开Sci_Tick10ms入口同时验证两路计数相互独立。原有长度边界和协议分发测试继续通过。

本次隔离构建目录：%LOCALAPPDATA%\CodexTemp\030-TI\sci-recovery-20260908\build；日志位于同级build.log。工程与工具链仍为Target 1 / STM32F030C8 / ARMCC 5.06u7。全量构建0错误、44警告；Code=38656、RO-data=2668、RW-data=692、ZI-data=6364，Flash分项合计42016字节，RAM合计7056字节。当前工作区另有用户同步修改，构建结果是构建时工作区快照的结果，不将全部资源变化归因于本次修复。

最终全量构建耗时5秒，AXF为854548字节、HEX为116540字节、BIN为41416字节，均位于上述build/Objects目录。

尚未连接实板：需观察PB1与TX末停止位、紧接应答的下一帧，以及屏蔽发送中断后的超时释放与再次通信。
