# 485 发送前等待与发送后保持

## 配置

在 Code/Source/Sci_Upper.h 修改以下宏后重新编译，也支持通过工程编译宏覆盖：

```c
#define RS485_TX_SETUP_US 0u
#define RS485_TX_HOLD_US 1000u
```

两者单位都是微秒，允许 0～65535；0 表示不等待，范围外编译报错。例如保持 2ms 使用 2000u。当前默认发送前不额外等待，检测到 TC 后再保持发送 1000µs，随后切回接收。

## 执行顺序

USART2：关闭 MCU 接收 → PB1 高 → SETUP 等待 → TXE 中断发送 → TC 确认 UART 停止位完成 → HOLD 保持 → 开启 MCU 接收、PB1 低。

SETUP 和 HOLD 期间保持 TX_BUSY，PB1 高，RE/RXNEIE 关闭。HOLD 期间 TXEIE/TCIE 也关闭，避免 TC 持续触发或重复装载保持时间。只有等待完成后才清理帧并恢复接收，同时完成原有升级应答完成标志处理。USART1 不应用这两个延时。

异常传输仍可由原有约 190～200ms 的发送看门狗中止；该看门狗只累计 ACTIVE 阶段，不截短配置的 SETUP/HOLD。TC 中断遗漏时，看门狗确认 TC 后仍启动 HOLD，而不是直接释放方向。TIM14 中断遗漏时，看门狗仅在硬件更新标志已经置位后补做阶段切换。

485 的 30 秒空闲断电逻辑仍检查 TX_BUSY，因此不会在 SETUP/HOLD 中切断供电。此功能不新增 STOP 唤醒。

## 计时实现及硬件依据

使用此前未占用的 TIM14，按 APB 时钟及其定时器倍频规则配置为 1MHz。ARR=等待微秒数-1；每次启动重新装载预分频器、清除更新标志并从 0 开始计数。到期中断停止定时器、清标志，推进发送阶段。无需引脚输出，不改变现有 TIM17 的 500µs 时基，不在中断中忙等。

启动计时器的短临界区保存并恢复 PRIMASK，防止软件生成更新事件时，被更新中断误认为延时已经结束。TIM14、USART 和 TIM17 使用相同中断优先级，阶段切换不会互相抢占。

参考本机官方库 C:/Users/Administrator/Downloads/STM32F0xx_StdPeriph_Lib_V1.5.0/STM32F0xx_StdPeriph_Lib_V1.5.0/Libraries 下的 RCC_ClocksTypeDef、TIM_TimeBaseInit、TIM_SetAutoreload、TIM_SetCounter、TIM_GenerateEvent、TIM_ClearITPendingBit 等定义与实现。中断入口绑定现有启动文件中的 TIM14_IRQHandler。

1000µs 是 TC 被软件处理后启动的定时保持时间；实际 TC 到 PB1 下降还包含软件设置与中断响应延迟，不能声称示波器上精确等于 1000µs。时间精度受 MCU 时钟精度影响；长时间关中断会延后切换，不会使软件主动提前释放方向。保持过长可能影响对端紧接着发送的数据，应结合收发器/隔离器与主机周转时间调试。

## 验证

- python tools/test_sci_transport.py：实际 C 状态机和计时器启动函数，模拟 USART/TIM/GPIO。通过 (SETUP,HOLD)=(0,1000)、(0,0)、(100,50000)、(65535,65535) 四组配置；覆盖边界前不切换、到期恢复接收、TC 重复不重启、TIM14 中断入口、看门狗补偿、收发方向、升级完成标志、双串口隔离和电源窗口。
- python tools/test_sci_protocol.py：524288 组 Modbus 地址/数量及真实 CRC/应答回归通过。
- build-keil 技能对应的本机 UV4 构建成功。工程 CommomBQ769x0_16series_030C8T6_C.uvprojx，Target 1，STM32F030C8，ARMCC 5.06 update 7。首次相关构建 0 错误、30 警告、6 秒；最终增量 0 错误、7 个既有未使用变量警告、2 秒。
- Code=37048，RO-data=3292，RW-data=664，ZI-data=6432；Flash 合计 41004 字节，RAM 合计 7096 字节。资源统计包含构建时工作区其他现有修改，不把所有大小变化归因于本功能。
- Objects 下同工程名 AXF=827632 字节、HEX=113766 字节、BIN=40432 字节。临时构建日志位于 LOCALAPPDATA/CodexTemp/030-TI/rs485-direction-delay-20260909，测试中间文件亦在用户临时区。

未连接 ST-Link、未烧录、未实测波形。后续调试应同时观察 USART2_TX、PB1、A/B，确认发送前建立和帧尾保持满足实际电路要求。
