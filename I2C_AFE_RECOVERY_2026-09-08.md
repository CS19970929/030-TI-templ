# AFE硬件I²C驱动完善说明

## 范围

当前板使用STM32F030C8T6的I2C2，PB10=SCL、PB11=SDA，单主机通信。保留同步轮询和BQ769x0的地址、CRC格式及寄存器访问方式。EEPROM的软件I²C不在本次修改范围。

新增I2C_AFE_Transport.c/.h作为硬件传输层；I2C_AFE1.c负责CRC、AFE寄存器和采样业务。Keil工程已加入新源文件。删除未被调用的旧Init_I2C、InitAFE1_F6F7初始化入口，避免PB10/PB11与PF6/PF7配置混淆。

## 初始化与时序

- 使用GPIO_StructInit完整初始化GPIO结构，PB10/PB11设为AF1、开漏、无内部上下拉，明确依赖板上外部上拉电阻。
- 使用I2C_DeInit复位I2C2，I2C_StructInit填充配置，再调用I2C_Init；关闭本机从地址应答，作为专用主机使用。
- 通过RCC_GetClocksFreq取得PCLK。当前8MHz使用原TIMINGR=0x00901D2B；48MHz沿用原0x30E3363D。其他频率明确报AFE_I2C_CLOCK并禁用外设，不悄悄使用错误时序。
- 地址参数保持7位，传给标准库时仅左移一位，读写方向交给I2C_Generate_Start_Read/Write设置。
- 保留原“写寄存器地址并STOP，再START读取”的BQ访问序列，不将其擅自改成通用重复起始接口。

时序常量沿用项目原值，并非本次实测结果。外部上拉、上升时间及实际SCL频率必须上板确认；BQ769x0通信速率应满足[TI数据手册](https://www.ti.com/lit/gpn/BQ76940)的限制。

## 超时与失败处理

TIM17每个1ms节拍调用AFE_I2C_Tick1ms。等待使用无符号毫秒差计算，允许计数回绕。若节拍未运行或停止，每次轮询中的校准10us延时预算仍可保证退出；不再用未经校准的CPU空转次数作为超时单位。

| 等待项 | 限制 |
| --- | --- |
| 开始事务前BUSY | 10ms |
| 完整事务，包括所有字节和STOP | 25ms |
| STOP后BUSY释放 | 2ms |
| 总线恢复时每次SCL释放 | 2ms |

每次等待先检查ARLO/BERR/OVR/NACKF，再判断目标标志。最后一个字节NACK与STOPF同时出现时仍返回失败，不误报成功。发送计数表示已交给外设的字节数，不代表每个字节都被从机ACK；最终返回0才代表本笔完成。接收计数记录实际读出的字节数。

传输失败后统一恢复：

1. 如未失去仲裁且BUSY仍在，请求STOP并有界等待释放。
2. 禁用外设，检查SCL/SDA实际电平。如果都已为高，只需复位、重新配置外设。
3. 若线路未释放，临时切为GPIO开漏；最多发出9个恢复时钟，每次抬高SCL都读取实际电平，允许有限时钟拉伸。
4. 构造STOP。SCL一直低或SDA无法释放时，记录对应错误，不无限循环。
5. 无论成功失败均释放GPIO输出并恢复AF配置，重新配置I2C外设。保留恢复结果，下一次正常访问可再次尝试。

发生ARLO时不强制STOP、不输出GPIO时钟，避免干扰可能存在的其他主机；当前实现按本板单主机场景设计，不是通用多主机调度器。

仅在尚未发出START前允许“恢复后继续本次访问”。START之后失败一律返回失败，不自动重放写命令，因为从机可能已接受部分数据。后续正常周期访问是新的事务。

驱动仅允许主循环调用，拒绝ISR及PRIMASK关中断环境。InitAFE1之前需完成InitDelay、InitTimer。全局关中断、高优先级中断长期占用等仍可能影响实际墙钟耗时；本机制不能替代系统看门狗。

## 上层数据与错误传播

- 写寄存器地址失败后立即返回，不再继续读数据。
- CRC写块检查BufferCRCC容量，最多14个数据字节；CRC读块检查startData_容量，最多40个数据字节。当前业务的3/8字节写、40字节读均保留。
- 读块先验证所有CRC，全部通过后才复制到调用者缓冲，失败不发布半包。
- 单字节读取复用块读取，统一边界和错误行为。
- AFE配置写入或读回失败不再比较未初始化的数据；增益读取失败不继续计算。采样读取失败保留上一份数据，不继续计算或更新滤波。
- AFE关机序列任一步写失败立即返回失败，不再固定报告成功。
- 硬件错误置位保持锁存，不让8位旧错误计数因连续报错回绕；后续MonitorAFE简化为连续3次完整采样成功后清除AFE1通信错误，具体策略见[AFE监控简化说明](AFE_MONITOR_SIMPLIFICATION_2026-09-08.md)。

通过AFE_I2C_GetDiagnostics读取只读诊断记录：lastError、lastStage、recoveryError、failureCount、recoveryCount。lastError/lastStage保留最近一次失败，成功访问不会清掉证据；recoveryError记录最近一次恢复的结果。公共收发接口仍返回0成功、1失败，兼容原有调用代码。

## 官方依据

本次参考本机官方库：

`C:\Users\Administrator\Downloads\STM32F0xx_StdPeriph_Lib_V1.5.0\STM32F0xx_StdPeriph_Lib_V1.5.0\Libraries\STM32F0xx_StdPeriph_Driver`

- stm32f0xx_i2c.c：I2C_Init内部关闭PE、写配置、开启PE；I2C_DeInit使用APB外设复位；I2C_TransferHandling设置SADD/NBYTES/RD_WRN/AUTOEND；I2C_ClearFlag直接写ICR。
- stm32f0xx_gpio.c/.h：GPIO初始化、开漏设置、输入电平读取和BSRR/BRR操作。
- [ST RM0360](https://www.st.com/resource/en/reference_manual/DM00091010.pdf)：I2C2时钟、GPIO复用和I2C状态标志。

## 验证

运行`python tools/test_i2c_afe.py`，使用Visual Studio编译实际传输源文件、CRC块函数及项目CRC8算法。故障注入覆盖：

- 1～255字节收发及计数、非法地址/长度/空指针、错误调用上下文和不支持的时钟。
- 地址NACK、部分写入NACK、末字节NACK+STOP、提前STOP、BERR/ARLO/OVR。
- TXIS/RXNE/STOP不出现、毫秒计数回绕、节拍停止时的退出上限。
- BUSY残留、SDA通过时钟恢复、SCL永久低、SDA永久低、恢复后再次通信。
- CRC末尾错误时不修改目的缓冲、40字节读边界及14字节写边界。

主机模拟无法证明真实寄存器的电气时序；需要实板验证外部上拉、正常ACK、断开AFE、SDA/SCL短时拉低、解除故障后的再次通信，以及保护逻辑能否按预期退出通信故障状态。

所有临时生成文件位于用户级CodexTemp目录。构建隔离目录为`%LOCALAPPDATA%\CodexTemp\030-TI\i2c-recovery-20260908\build`，构建日志为同级build.log；主机测试文件位于`%LOCALAPPDATA%\CodexTemp\030-TI\i2c-host-tests`。

最终验证：主机故障注入测试通过；Keil Target 1（STM32F030C8，ARMCC V5.06 update 7 build 960）隔离全量编译为0个错误、44个警告，新增硬件传输文件无编译警告。Code=35724、RO-data=3292、RW-data=644、ZI-data=6428字节，链接统计Flash为39660字节、RAM为7072字节。AXF/HEX/BIN已生成在上述隔离目录的Objects中。该构建包含当时工作区的其他并行修改，仅用于集成验证，不据此计算本次I²C修改的空间增减。尚未烧录或实板验证。
