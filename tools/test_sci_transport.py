"""编译真实 Sci_Upper.c 传输函数，使用模拟寄存器验证状态与边界。

Windows: python tools/test_sci_transport.py
需要 Visual Studio C++ 工具；中间文件仅写入 LOCALAPPDATA/CodexTemp。
这是主机逻辑测试，不替代板上485时序、电气和业务协议验收。
"""
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/030-TI/sci-transport-tests"


def function(source, name):
    match = re.search(r"^(?:static )?(?:void|UINT8) " + name + r"\([^;]*?\)\n\{", source, re.M)
    if not match:
        raise ValueError(name)
    return source[match.start():source.index("\n}", match.end()) + 2]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "Code/Source/Sci_Upper.c").read_bytes().decode("gbk").replace("\r\n", "\n")
    header = (ROOT / "Code/Source/Sci_Upper.h").read_bytes().decode("latin1").replace("\r\n", "\n")
    # 保留真实消息结构和常量；去掉注释以避免旧文件混合编码影响主机编译。
    header = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.S)
    constants = "\n".join(line for line in header.splitlines() if re.match(r"#define\s+(RS485_(?:STA_|ACK_|ERROR_|SLAVE_ADDR|BROADCAST_ADDR|MAX_BUFFER_SIZE)|SCI_TX_|P12_)", line))
    declarations = "\n".join(re.findall(r"enum (?:RS485_CMD_E|SCI_FRAME_PROTOCOL_E)\s*\{.*?\};|struct RS485MSG\s*\{.*?\};", header, re.S))
    port = re.search(r"typedef struct \{.*?\} SciPort;", source, re.S).group()
    names = ["Sci_ClearFrameState", "Sci_ResetFrameState", "Sci_SetTxDirection", "Sci_RS485PowerIsOn", "Sci_RS485WakeFromISR", "Sci_RS485ValidRequest", "Sci_RS485PowerService", "Sci_IsValidWriteRegsByteCount", "Sci_StartTx", "Sci_FinishTx", "Sci_AbortTx", "Sci_TxWatchdog", "Sci_Tick10ms", "Sci_TxISR_Deal", "Sci_FaultChk", "Sci_RxISR_Deal", "Sci_ProcessRequest", "Sci_PrepareResponse", "Sci_Service"]
    code = PRELUDE + constants + "\n" + declarations + "\n" + port + STUBS
    code += "\n".join(function(source, name) for name in names) + TESTS
    (OUT / "test.c").write_text(code, encoding="utf-8")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")
    if not vcvars.exists():
        raise SystemExit("未找到 Visual Studio vcvars64.bat")
    (OUT / "build.cmd").write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /W3 /Od test.c /Fe:test.exe\nexit /b %errorlevel%\n', encoding="utf-8")
    subprocess.run(["cmd", "/c", str(OUT / "build.cmd")], cwd=OUT, check=True)
    subprocess.run([str(OUT / "test.exe")], cwd=OUT, check=True)


PRELUDE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef struct { UINT32 CR1, ISR, ICR, TDR, RDR; } USART_TypeDef;
static USART_TypeDef uart1, uart2;
#define USART1 (&uart1)
#define USART2 (&uart2)
#define RESET 0
#define USART_CR1_UE 1u
#define ENABLE 1
#define DISABLE 0
#define USART_CR1_RE (1u << 2)
#define USART_CR1_TE (1u << 3)
#define USART_CR1_RXNEIE (1u << 5)
#define USART_CR1_TCIE (1u << 6)
#define USART_CR1_TXEIE (1u << 7)
#define USART_ISR_PE 1u
#define USART_ISR_FE 2u
#define USART_ISR_NE 4u
#define USART_ISR_ORE 8u
#define USART_ISR_TC (1u << 6)
#define USART_ISR_TXE (1u << 7)
#define USART_ICR_TCCF USART_ISR_TC
#define USART_IT_TC USART_ISR_TC
#define GPIO_M_CTR 1
#define PIN_M_CTR 256
#define RS485_POWER_WINDOW_SECONDS 30u
#define USART_FLAG_ORE USART_ISR_ORE
#define USART_FLAG_NE USART_ISR_NE
#define USART_FLAG_FE USART_ISR_FE
#define USART_FLAG_PE USART_ISR_PE
static UINT32 s_rs485Tick10ms, s_rs485LastActivity;
static UINT8 s_rs485PowerOn, s_rs485Ready;
static int power, rejectFrame;
static UINT16 USART_ReceiveData(USART_TypeDef *u) { return (UINT16)u->RDR; }
static void USART_ClearFlag(USART_TypeDef *u, UINT32 flags) { u->ICR=flags; }
#define GPIO_M_STB 0
#define PIN_M_STB 2
#define SCI_RX_TIMEOUT_TICKS 3u
#define SCI_TX_TIMEOUT_TICKS 20u
static int direction, requestCount, disableCount;
static UINT8 u8FlashUpdateE2PROM, u8FlashUpdateFlag;
static UINT32 irqMask;
static struct { struct { UINT8 b1Sys10msFlag1; } bits; } g_st_SysTimeFlag;
static UINT32 __get_PRIMASK(void) { return irqMask; }
static void __disable_irq(void) { irqMask = 1; }
static void __set_PRIMASK(UINT32 mask) { irqMask = mask; }
static void GPIO_SetBits(int gpio, int pin) { if (gpio == GPIO_M_CTR) power=1; else direction = 1; }
static void GPIO_ResetBits(int gpio, int pin) {
    if (gpio == GPIO_M_CTR) { power=0; return; }
    /* 正常释放时接收已开启；故障释放时UE和TE必须都已关闭。 */
    if (uart2.CR1 & USART_CR1_UE)
        assert((uart2.CR1 & (USART_CR1_RE|USART_CR1_RXNEIE)) == (USART_CR1_RE|USART_CR1_RXNEIE));
    else
        assert(!(uart2.CR1 & USART_CR1_TE));
    direction = 0;
}
static void USART_Cmd(USART_TypeDef *u, int enabled) {
    if (enabled) u->CR1 |= USART_CR1_UE;
    else {
        assert(!(u->CR1 & (USART_CR1_TXEIE|USART_CR1_TCIE|USART_CR1_RXNEIE)));
        u->CR1 &= ~USART_CR1_UE; u->ISR = USART_ISR_TXE|USART_ISR_TC;
        disableCount++;
    }
}
static void USART_ClearITPendingBit(USART_TypeDef *u, UINT32 bits) { u->ICR = bits; }
'''

STUBS = r'''
#define _COMMOM_UPPER_SCI1
#define _COMMOM_UPPER_SCI2
static struct RS485MSG globalMsg1, globalMsg2;
static SciPort sci1={USART1,&globalMsg1,0,0,0,0}, sci2={USART2,&globalMsg2,1,0,0,0};
/* 主循环业务分发用桩记录调用；寄存器读写、P12内容本身不是本测试覆盖范围。 */
static UINT8 P12_VerifyFrame(struct RS485MSG *s) { return !rejectFrame; }
static UINT8 P12_BuildResponse(struct RS485MSG *s) { s->AckLenth = 8; return 1; }
static void CRC_verify(struct RS485MSG *s) { s->AckType = rejectFrame ? RS485_ACK_NEG : RS485_ACK_POS; }
static void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s) { requestCount++; }
static void Sci_Deal_WrReg_0x06(struct RS485MSG *s) { requestCount++; }
static void Sci_Deal_WrRegs_0x10(struct RS485MSG *s) { requestCount++; }
static void Sci_ACK_0x03(struct RS485MSG *s) { s->AckLenth = 8; }
static void Sci_ACK_0x06_0x10(struct RS485MSG *s) { s->AckLenth = 8; }
'''

TESTS = r'''
static void init(SciPort *p) {
    memset(p->usart, 0, sizeof(*p->usart));
    memset(p->msg, 0, sizeof(*p->msg));
    Sci_ResetFrameState(p);
    p->usart->CR1 |= USART_CR1_UE;
    direction = 0;
    disableCount = 0;
    g_st_SysTimeFlag.bits.b1Sys10msFlag1 = 0;
}
static void feed(SciPort *p, const UINT8 *data, int len) {
    int i;
    for (i = 0; i < len; i++) {
        assert(p->usart->CR1 & USART_CR1_RXNEIE);
        p->usart->RDR = data[i];
        Sci_RxISR_Deal(p);
    }
}
static void tx(SciPort *p, int length) {
    int i;
    init(p);
    p->msg->AckLenth = (UINT8)length;
    for(i=0;i<length;i++) p->msg->u16Buffer[i]=(UINT8)(i^0x55);
    Sci_StartTx(p);
    assert(direction == (p->usart == USART2));
    assert(!(p->usart->CR1 & USART_CR1_RE));
    for(i=0;i<length;i++) {
        p->usart->ISR=USART_ISR_TXE;
        Sci_TxISR_Deal(p);
        assert(p->usart->TDR == (UINT8)(i^0x55));
        assert(p->msg->csr == RS485_STA_TX_BUSY);
        assert(direction == (p->usart == USART2));
    }
    assert(!(p->usart->CR1 & USART_CR1_TXEIE));
    assert(p->usart->CR1 & USART_CR1_TCIE);
    /* 最后字节已写入，TXE再次出现仍不得释放485方向。 */
    Sci_TxISR_Deal(p);
    assert(direction == (p->usart == USART2));
    assert(p->msg->csr == RS485_STA_TX_BUSY);
    u8FlashUpdateE2PROM=1;u8FlashUpdateFlag=0;
    p->usart->ISR=USART_ISR_TC;
    Sci_TxISR_Deal(p);
    assert(direction == 0);
    assert(p->msg->csr == RS485_STA_IDLE);
    assert(u8FlashUpdateFlag && !u8FlashUpdateE2PROM);
    assert(p->usart->CR1 & USART_CR1_RXNEIE);
    /* 不运行主循环，立即送入下一帧首字节；之后调度不得再次清帧。 */
    p->usart->RDR=1;Sci_RxISR_Deal(p);
    Sci_Service(p);assert(p->msg->ptr_no==1);
    assert(!disableCount);
}
static void watchdog(SciPort *p) {
    int i;
    init(p);p->msg->AckLenth=8;Sci_StartTx(p);
    /* 模拟TXE中断完全不来：主循环也不运行，定时看门狗仍能恢复。 */
    p->usart->ISR=0;u8FlashUpdateFlag=0;u8FlashUpdateE2PROM=0;
    for(i=0;i<SCI_TX_TIMEOUT_TICKS-1;i++) Sci_TxWatchdog(p);
    assert(p->msg->csr==RS485_STA_TX_BUSY && disableCount==0);
    Sci_TxWatchdog(p);
    assert(disableCount==1 && p->msg->csr==RS485_STA_IDLE);
    assert(!direction && !u8FlashUpdateFlag);
    assert((p->usart->CR1 & (USART_CR1_UE|USART_CR1_RE|USART_CR1_RXNEIE)) == (USART_CR1_UE|USART_CR1_RE|USART_CR1_RXNEIE));
    assert(!(p->usart->CR1 & (USART_CR1_TE|USART_CR1_TXEIE|USART_CR1_TCIE)));
    /* 恢复后的第一帧可正常发送。 */
    p->msg->AckLenth=1;Sci_StartTx(p);assert(p->usart->CR1 & USART_CR1_TE);
    p->usart->ISR=USART_ISR_TXE;Sci_TxISR_Deal(p);
    /* 末字节已写入但TC一直不来，仍按失败中止。 */
    for(i=0;i<SCI_TX_TIMEOUT_TICKS;i++) Sci_TxWatchdog(p);
    assert(disableCount==2 && p->msg->csr==RS485_STA_IDLE);
    init(p);p->msg->AckLenth=1;Sci_StartTx(p);
    p->usart->ISR=USART_ISR_TXE;Sci_TxISR_Deal(p);
    p->usart->ISR=USART_ISR_TC;u8FlashUpdateE2PROM=1;
    /* TC已发生但TC中断未执行，补做正常完成而不是复位USART。 */
    for(i=0;i<SCI_TX_TIMEOUT_TICKS;i++) Sci_TxWatchdog(p);
    assert(!disableCount && p->msg->csr==RS485_STA_IDLE && u8FlashUpdateFlag);
    /* 空闲不会累计超时或清掉下一帧。 */
    p->usart->RDR=1;Sci_RxISR_Deal(p);
    for(i=0;i<SCI_TX_TIMEOUT_TICKS+1;i++) Sci_TxWatchdog(p);
    assert(p->msg->ptr_no==1);
}
int main(void) {
    struct RS485MSG m1,m2;
    SciPort p1={USART1,&m1,0,0,0}, p2={USART2,&m2,1,0,0};
    SciPort *p;
    UINT8 frame[251]={1,3,0,0,0,1,0,0};
    int port,cmd,n;
    for(port=0;port<2;port++) {
        p=port?&p2:&p1;
        tx(p,1); tx(p,8); tx(p,251); watchdog(p);
        for(cmd=3;cmd<=6;cmd+=3) {
            init(p); frame[0]=1;frame[1]=(UINT8)cmd;
            feed(p,frame,8);
            assert(p->msg->csr==RS485_STA_RX_COMPLETE);
            assert(!(p->usart->CR1 & USART_CR1_RE));
            requestCount=0;Sci_Service(p);
            assert(requestCount==1 && p->msg->csr==RS485_STA_RX_OK);
            Sci_Service(p);assert(p->msg->csr==RS485_STA_TX_BUSY);
        }
        /* 0x10长度边界：最大242字节数据；0/不匹配/超限均丢弃。 */
        for(n=0;n<=244;n+=2) {
            init(p); memset(frame,0,sizeof(frame));frame[0]=1;frame[1]=16;
            frame[5]=(UINT8)(n/2);frame[6]=(UINT8)n;
            feed(p,frame,7);
            if(n==0 || n>242) assert(p->msg->ptr_no==0);
            else { feed(p,frame+7,n+2);assert(p->msg->csr==RS485_STA_RX_COMPLETE); }
        }
        init(p);frame[0]=1;frame[1]=16;frame[5]=2;frame[6]=2;
        feed(p,frame,7);assert(p->msg->ptr_no==0);
        init(p);frame[0]=1;frame[1]=3;feed(p,frame,2);
        p->usart->ISR=USART_ISR_ORE|USART_ISR_FE;Sci_FaultChk(p);
        assert(p->rxFault && p->usart->ICR==(USART_ISR_ORE|USART_ISR_FE));
        g_st_SysTimeFlag.bits.b1Sys10msFlag1=1;
        Sci_Service(p);Sci_Service(p);assert(p->msg->ptr_no==2);
        irqMask=1;Sci_Service(p);assert(irqMask==1);irqMask=0;
        assert(p->msg->ptr_no==0 && !p->rxFault);
        g_st_SysTimeFlag.bits.b1Sys10msFlag1=0;
        feed(p,frame,8);p->rxFault=1;requestCount=0;Sci_Service(p);
        assert(requestCount==0 && p->msg->csr==RS485_STA_IDLE);
        p->msg->AckLenth=0;Sci_StartTx(p);assert(!direction && p->msg->csr==RS485_STA_IDLE);
        p->msg->AckLenth=252;Sci_StartTx(p);assert(!direction && p->msg->csr==RS485_STA_IDLE);
    }
    /* P12仅串口2启用；最短及最大帧、错误帧头、超长数据。 */
    init(&p1);frame[0]=0x21;feed(&p1,frame,1);assert(m1.ptr_no==0);
    for(n=0;n<=243;n+=243) {
        init(&p2);memset(frame,0,sizeof(frame));frame[0]=0x21;frame[1]=0xaa;
        frame[2]=1;frame[3]=3;frame[4]=0x80;frame[5]=(UINT8)n;
        feed(&p2,frame,n+8);assert(m2.csr==RS485_STA_RX_COMPLETE);
        assert(m2.u8FrameProtocol==SCI_FRAME_PROTOCOL_P12 && m2.u16FrameAddress==0x0301);
        Sci_Service(&p2);assert(m2.csr==RS485_STA_RX_OK);
    }
    init(&p2);frame[5]=244;feed(&p2,frame,6);assert(m2.ptr_no==0);
    init(&p2);frame[1]=0;feed(&p2,frame,2);assert(m2.ptr_no==0);
    /* 两路交错收帧，互不覆盖。 */
    init(&p1);init(&p2);memset(frame,0,sizeof(frame));frame[0]=1;frame[1]=3;
    feed(&p1,frame,4);feed(&p2,frame,8);feed(&p1,frame+4,4);
    assert(m1.csr==RS485_STA_RX_COMPLETE && m2.csr==RS485_STA_RX_COMPLETE);
    /* 验证公开定时入口确实服务两路，且USART2未超时时不被USART1影响。 */
    init(&sci1);init(&sci2);sci1.msg->AckLenth=8;sci2.msg->AckLenth=8;
    Sci_StartTx(&sci1);Sci_StartTx(&sci2);uart1.ISR=0;uart2.ISR=0;
    sci1.txTimeoutTick=SCI_TX_TIMEOUT_TICKS-1;
    Sci_Tick10ms();
    assert(sci1.msg->csr==RS485_STA_IDLE && sci2.msg->csr==RS485_STA_TX_BUSY && direction);
    for(n=1;n<SCI_TX_TIMEOUT_TICKS;n++) Sci_Tick10ms();
    assert(sci2.msg->csr==RS485_STA_IDLE && !direction);
    /* Real power functions: boundary, repeated edges, UART isolation, CRC
     * rejection, P12 success, wraparound, partial frame and active TX. */
    init(&sci1); init(&sci2); s_rs485Ready=1;
    assert(!Sci_RS485PowerIsOn() && !power);
    s_rs485Tick10ms=100; Sci_RS485WakeFromISR();
    assert(power && s_rs485LastActivity==100);
    s_rs485Tick10ms=3099; Sci_RS485WakeFromISR();
    assert(s_rs485LastActivity==100);
    Sci_RS485PowerService(); assert(power);
    Sci_RS485ValidRequest(&sci1); assert(s_rs485LastActivity==100);
    s_rs485Tick10ms=3100; sci2.msg->ptr_no=2;
    irqMask=1; Sci_RS485PowerService(); assert(irqMask==1); irqMask=0;
    assert(!power && !s_rs485PowerOn && sci2.msg->ptr_no==0);
    assert(!(uart2.CR1 & USART_CR1_RXNEIE));
    Sci_RS485WakeFromISR(); assert(power && (uart2.CR1 & USART_CR1_RXNEIE));
    s_rs485Tick10ms=4000; rejectFrame=1;
    sci2.msg->u8FrameProtocol=SCI_FRAME_PROTOCOL_MODBUS;
    Sci_ProcessRequest(&sci2); assert(s_rs485LastActivity==3100);
    rejectFrame=0; Sci_ProcessRequest(&sci2); assert(s_rs485LastActivity==4000);
    s_rs485Tick10ms=5000; sci2.msg->u8FrameProtocol=SCI_FRAME_PROTOCOL_P12;
    rejectFrame=1; Sci_ProcessRequest(&sci2); assert(s_rs485LastActivity==4000);
    rejectFrame=0; sci2.msg->u8FrameProtocol=SCI_FRAME_PROTOCOL_P12;
    Sci_ProcessRequest(&sci2); assert(s_rs485LastActivity==5000);
    s_rs485Tick10ms=8000; sci2.msg->csr=RS485_STA_TX_BUSY;
    Sci_RS485PowerService(); assert(power);
    Sci_ResetFrameState(&sci2); Sci_RS485PowerService(); assert(!power);
    s_rs485Tick10ms=0xfffffff0u; Sci_RS485WakeFromISR();
    s_rs485Tick10ms=0xfffffff0u+2999u; Sci_RS485PowerService(); assert(power);
    s_rs485Tick10ms++; Sci_RS485PowerService(); assert(!power);
    puts("PASS: RS485 power window, valid activity, rejection, repeat edges, partial RX, TX deferral, wraparound");
    puts("PASS: immediate RX, TX watchdog abort/recovery, TXE/TC direction, 1/8/251-byte TX, RX bounds, timeout, faults, dispatch, dual-port isolation");
    return 0;
}
'''

if __name__ == "__main__":
    main()
