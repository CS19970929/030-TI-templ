# ST-Link 通信 HardFault 现场记录

## 结论

通过 ST-Link V2 / OpenOCD 附加当前程序，连接时目标已经处于 HardFault。USART2 收到合法 CRC 的请求 `01 03 00 00 00 03 05 CB`，读取 0x0000 起的 3 个寄存器。Sci_Deal_ReadRegs_0x03 未拒绝该地址，Sci_ACK_0x03 的正常应答只处理地址 >= 0x2000；低地址分支没有初始化局部长度 i，就调用 Sci_CRC16RTU，导致越界读取并进入 HardFault 永久循环。

本次仅观察、保存现场，没有修改固件、烧录或复位。捕获后曾保留调试暂停；随后已按用户要求 detach 并退出 GDB/OpenOCD，释放 ST-Link。

## 硬件与符号核对

- STLINK V2J37S7，目标电压 3.283789V，SWD 950kHz，Cortex-M0。
- AXF：Objects/CommomBQ769x0_16series_030C8T6_C.axf。
- SHA256：c43892cde814cd01c8b719b03972c21919485d1b38c4eecbefb826589f43a737。
- GDB compare-sections ER_IROM1：0x08001C00～0x0800B5D8 全部匹配。
- 源文件时间戳提示比可执行文件更新，因此关键结论同时以板上匹配的机器码和原始异常栈确认。

## 原始证据

- 当前 PC=0x08004954，HardFault_Handler 自循环。
- EXC_RETURN/LR=0xFFFFFFF9，MSP=0x20001B48，xPSR=0x81000003。
- 异常栈保存的 PC=0x08008358，LR=0x08007D05。
- 故障指令：Sci_CRC16RTU+12，`ldrb r2, [r5, r3]`。
- R5=0x20000A08（USART2 消息缓冲），R3=0x15F8；读取地址恰为 0x20002000，超出本器件 8KB SRAM。
- CRC 循环比较 R3 与 R1；R1=0x20000174，是未初始化局部量遗留值。GDB 把 UINT8 参数显示为 116，不能据此认为真实循环仅执行 116 次，实际机器指令比较完整 R1。
- Sci_ACK_0x03+76 的低地址分支直接跳到 +30，未给代表 i 的 R5 赋值；+32 把 R5 传给 CRC 的 R1。这直接解释了本次异常。
- 消息状态：csr=2 (RX_OK)，AckType=0 (POS)，起始地址=0，数据字节数=6，ptr_no=8；原请求尚在缓冲区。
- 调用链：main → App_Sci → App_CommonUpper → Sci_Service → Sci_PrepareResponse → Sci_ACK_0x03 → Sci_CRC16RTU → HardFault_Handler。

CRC 独立核算为 0xCB05，与请求中的低字节 05、高字节 CB 相符。请求已完整到达，不是 485 供电窗口关闭导致无响应。

## 当时的修复建议

应在 0x03 解析阶段验证起始地址、寄存器数量以及地址范围末端，非法请求返回协议异常；应答构造保证所有分支都确定长度并检查缓冲边界。仅把 i 初始化成 0 不能替代合法地址与长度检查。现有传输测试的读寄存器业务和应答使用桩，未覆盖这条真实业务分支，需要增加原请求回归用例。

现场完整 GDB 记录和 8KB SRAM 快照保存在用户临时区：
C:/Users/Administrator/AppData/Local/CodexTemp/030-TI/stlink-monitor-20260908/。
OpenOCD PID=14372，GDB 交互会话=48601；服务端口 3333/4444/6666。调试技能脚本缺少 tool_config，直接使用已安装的 OpenOCD 与 ARM GDB 完成附加。

## 后续处理

已完成本地代码修复及回归验证，见 MODBUS_REQUEST_BOUNDS_FIX_2026-09-08.md；本记录中的地址和 AXF 指纹指向修复前现场。未重新连接或烧录板子。
