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
    names = ["Sci_ClearFrameState", "Sci_ResetFrameState", "Sci_SetTxDirection", "Sci_IsValidWriteRegsByteCount", "Sci_StartTx", "Sci_TxISR_Deal", "Sci_FaultChk", "Sci_RxISR_Deal", "Sci_ProcessRequest", "Sci_PrepareResponse", "Sci_Service"]
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
#define GPIO_M_STB 0
#define PIN_M_STB 2
#define SCI_RX_TIMEOUT_TICKS 3u
static int direction, requestCount;
static UINT8 u8FlashUpdateE2PROM, u8FlashUpdateFlag;
static UINT32 irqMask;
static struct { struct { UINT8 b1Sys10msFlag1; } bits; } g_st_SysTimeFlag;
static UINT32 __get_PRIMASK(void) { return irqMask; }
static void __disable_irq(void) { irqMask = 1; }
static void __set_PRIMASK(UINT32 mask) { irqMask = mask; }
static void GPIO_SetBits(int gpio, int pin) { direction = 1; }
static void GPIO_ResetBits(int gpio, int pin) { direction = 0; }
static void USART_ClearITPendingBit(USART_TypeDef *u, UINT32 bits) { u->ICR = bits; }
'''

STUBS = r'''
/* 主循环业务分发用桩记录调用；寄存器读写、P12内容本身不是本测试覆盖范围。 */
static UINT8 P12_VerifyFrame(struct RS485MSG *s) { return 1; }
static UINT8 P12_BuildResponse(struct RS485MSG *s) { s->AckLenth = 8; return 1; }
static void CRC_verify(struct RS485MSG *s) { s->AckType = RS485_ACK_POS; }
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
    direction = 0;
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
    assert(p->msg->csr == RS485_STA_TX_COMPLETE);
    assert(u8FlashUpdateFlag && !u8FlashUpdateE2PROM);
    Sci_Service(p);
    assert(p->msg->csr == RS485_STA_IDLE);
    assert(p->usart->CR1 & USART_CR1_RXNEIE);
}
int main(void) {
    struct RS485MSG m1,m2;
    SciPort p1={USART1,&m1,0,0,0}, p2={USART2,&m2,1,0,0};
    SciPort *p;
    UINT8 frame[251]={1,3,0,0,0,1,0,0};
    int port,cmd,n;
    for(port=0;port<2;port++) {
        p=port?&p2:&p1;
        tx(p,1); tx(p,8); tx(p,251);
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
    puts("PASS: TXE/TC direction, 1/8/251-byte TX, RX bounds, timeout, faults, dispatch, dual-port isolation");
    return 0;
}
'''

if __name__ == "__main__":
    main()
