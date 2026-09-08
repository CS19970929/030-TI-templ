"""主机故障注入：编译实际硬件I2C传输层及CRC块函数；不替代实板电气验证。
运行 python tools/test_i2c_afe.py，需要Visual Studio C++。
生成文件仅保存在LOCALAPPDATA/CodexTemp。
"""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/030-TI/i2c-host-tests"


def extract(source, name):
    match = re.search(r"^(?:int|unsigned char) " + name + r"\([^;]*?\)\n\{", source, re.M)
    assert match, name
    return source[match.start():source.index("\n}", match.end()) + 2]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    driver = (ROOT / "Code/Source/I2C_AFE_Transport.c").read_text(encoding="utf-8").replace('#include "main.h"', "")
    header = (ROOT / "Code/Source/I2C_AFE_Transport.h").read_text(encoding="utf-8")
    protocol = (ROOT / "Code/Source/I2C_AFE1.c").read_bytes().decode("gbk").replace("\r\n", "\n")
    common = (ROOT / "Code/Source/PubFunc.c").read_bytes().decode("gbk").replace("\r\n", "\n")
    body = extract(common, "CRC8") + "\n" + "\n".join(extract(protocol, name) for name in ["I2CWriteBlockWithCRC", "I2CReadBlockWithCRC", "I2CReadRegisterByteWithCRC"])
    (OUT / "test.c").write_text(PRELUDE + header + driver + body + TESTS, encoding="utf-8")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")
    (OUT / "build.cmd").write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /W3 /Od test.c /Fe:test.exe\nexit /b %errorlevel%\n', encoding="utf-8")
    subprocess.run(["cmd", "/c", str(OUT / "build.cmd")], cwd=OUT, check=True)
    subprocess.run([str(OUT / "test.exe")], cwd=OUT, check=True)


PRELUDE = r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define I2C_SYSTEM
#define ENABLE 1
#define DISABLE 0
#define CRC_KEY 7
#define GPIO_Pin_10 (1u<<10)
#define GPIO_Pin_11 (1u<<11)
#define GPIO_PinSource10 10
#define GPIO_PinSource11 11
#define GPIO_AF_1 1
#define GPIO_OType_OD 1
#define GPIO_PuPd_NOPULL 0
#define GPIO_Speed_Level_1 1
#define RCC_AHBPeriph_GPIOB 1
#define RCC_APB1Periph_I2C2 1
#define I2C_Mode_I2C 0
#define I2C_AnalogFilter_Enable 0
#define I2C_Ack_Enable 0
#define I2C_AcknowledgedAddress_7bit 0
#define I2C_AutoEnd_Mode 1
#define I2C_Generate_Start_Read 1
#define I2C_Generate_Start_Write 0
#define I2C_ISR_BUSY (1u<<15)
#define I2C_ISR_TXIS (1u<<1)
#define I2C_ISR_RXNE (1u<<2)
#define I2C_ISR_NACKF (1u<<4)
#define I2C_ISR_STOPF (1u<<5)
#define I2C_ISR_BERR (1u<<8)
#define I2C_ISR_ARLO (1u<<9)
#define I2C_ISR_OVR (1u<<10)
#define I2C_ICR_STOPCF I2C_ISR_STOPF
#define I2C_ICR_NACKCF I2C_ISR_NACKF
#define I2C_ICR_BERRCF I2C_ISR_BERR
#define I2C_ICR_ARLOCF I2C_ISR_ARLO
#define I2C_ICR_OVRCF I2C_ISR_OVR
#define I2C_OAR1_OA1EN (1u<<15)
typedef enum { GPIO_Mode_AF, GPIO_Mode_OUT } GPIOMode_TypeDef;
typedef struct { unsigned GPIO_Pin; GPIOMode_TypeDef GPIO_Mode; unsigned GPIO_OType, GPIO_PuPd, GPIO_Speed; } GPIO_InitTypeDef;
typedef struct { unsigned I2C_Mode,I2C_AnalogFilter,I2C_DigitalFilter,I2C_OwnAddress1,I2C_Ack,I2C_AcknowledgedAddress,I2C_Timing; } I2C_InitTypeDef;
typedef struct { uint32_t PCLK_Frequency; } RCC_ClocksTypeDef;
typedef struct { uint32_t ISR,OAR1; int enabled; } I2C_TypeDef;
static I2C_TypeDef bus;
#define I2C_AFE (&bus)
#define GPIOB 0
static unsigned char startData_[80], BufferCRCC[30];
static struct { unsigned char u8ErrFlag_Com_AFE1; } System_ErrFlag;
#define ERROR_AFE1 1
static void System_ERROR_UserCallback(int error) { System_ErrFlag.u8ErrFlag_Com_AFE1++; }
static unsigned clockHz, primask, ipsr;
static int scenario, transfers, moved, expected, reading, stopRequests, resets;
static unsigned odr, forcedLow, pinMode, pulses, releaseAt, micros, tickRemainder;
static int frozenTick, addressRegister;
static uint32_t errorFlag;
static unsigned char wire[255], output[255];
enum { NORMAL, ADDRESS_NACK, LAST_NACK, RX_STALL, STOP_STALL, BUS_FAULT, EARLY_STOP, TX_STALL, PARTIAL_NACK };
void AFE_I2C_Tick1ms(void);
static void __delay_us(unsigned us) {
    micros+=us; tickRemainder+=us;
    while(tickRemainder>=1000) { tickRemainder-=1000; if(!frozenTick) AFE_I2C_Tick1ms(); }
}
static uint32_t __get_IPSR(void) { return ipsr; }
static uint32_t __get_PRIMASK(void) { return primask; }
static void RCC_AHBPeriphClockCmd(unsigned p,int e) {}
static void RCC_APB1PeriphClockCmd(unsigned p,int e) {}
static void RCC_GetClocksFreq(RCC_ClocksTypeDef *c) { c->PCLK_Frequency=clockHz; }
static void GPIO_StructInit(GPIO_InitTypeDef *g) { memset(g,0,sizeof(*g)); }
static void GPIO_Init(int port,GPIO_InitTypeDef *g) {
    assert(g->GPIO_OType==GPIO_OType_OD && g->GPIO_PuPd==GPIO_PuPd_NOPULL);
    if(g->GPIO_Mode==GPIO_Mode_OUT) assert(!bus.enabled);
    pinMode=g->GPIO_Mode;
}
static void GPIO_PinAFConfig(int port,unsigned pin,unsigned af) {}
static unsigned GPIO_ReadInputData(int port) { return odr & ~forcedLow; }
static int GPIO_ReadInputDataBit(int port,unsigned bit) { return !!(GPIO_ReadInputData(port)&bit); }
static void GPIO_ResetBits(int port,unsigned pins) { odr &= ~pins; }
static void GPIO_SetBits(int port,unsigned pins) {
    if(pinMode==GPIO_Mode_OUT && (pins&GPIO_Pin_10) && !(odr&GPIO_Pin_10)) {
        pulses++;
        if(releaseAt && pulses>=releaseAt) forcedLow &= ~GPIO_Pin_11;
    }
    odr |= pins;
}
static void I2C_Cmd(I2C_TypeDef *b,int enabled) { b->enabled=enabled; }
static void I2C_DeInit(I2C_TypeDef *b) { b->ISR=0;b->OAR1=0;b->enabled=0;resets++; }
static void I2C_StructInit(I2C_InitTypeDef *c) { memset(c,0,sizeof(*c)); }
static void I2C_Init(I2C_TypeDef *b,I2C_InitTypeDef *c) {
    assert(c->I2C_Timing==0x00901D2B || c->I2C_Timing==0x30E3363D);
    b->enabled=1;b->OAR1=I2C_OAR1_OA1EN;
}
static void I2C_ClearFlag(I2C_TypeDef *b,unsigned flags) { b->ISR &= ~flags; }
static void I2C_GenerateSTOP(I2C_TypeDef *b,int enabled) {
    stopRequests++;
    if(!forcedLow) b->ISR=(b->ISR & ~I2C_ISR_BUSY)|I2C_ISR_STOPF;
}
static void I2C_TransferHandling(I2C_TypeDef *b,uint16_t address,uint8_t count,unsigned end,unsigned direction) {
    assert(!(address&1)); assert(b->enabled);
    transfers++;moved=0;expected=count;reading=direction;addressRegister=address;
    b->ISR=I2C_ISR_BUSY | (reading?I2C_ISR_RXNE:I2C_ISR_TXIS);
    if(scenario==ADDRESS_NACK) b->ISR=I2C_ISR_NACKF|I2C_ISR_STOPF;
    if(scenario==RX_STALL && reading) b->ISR=I2C_ISR_BUSY;
    if(scenario==TX_STALL && !reading) b->ISR=I2C_ISR_BUSY;
    if(scenario==EARLY_STOP) b->ISR=I2C_ISR_STOPF;
    if(scenario==BUS_FAULT) b->ISR=errorFlag;
}
static void finishByte(I2C_TypeDef *b) {
    if(scenario==PARTIAL_NACK && moved==1) b->ISR=I2C_ISR_NACKF|I2C_ISR_STOPF;
    else if(moved==expected) {
        b->ISR=scenario==STOP_STALL ? I2C_ISR_BUSY : I2C_ISR_STOPF;
        if(scenario==LAST_NACK) b->ISR |= I2C_ISR_NACKF;
    }
}
static void I2C_SendData(I2C_TypeDef *b,unsigned char byte) { output[moved++]=byte;finishByte(b); }
static unsigned char I2C_ReceiveData(I2C_TypeDef *b) { unsigned char v=wire[moved++];finishByte(b);return v; }
'''

TESTS = r'''
static void resetTest(void) {
    memset(&bus,0,sizeof(bus));memset(&afeI2cDiag,0,sizeof(afeI2cDiag));
    afeI2cMs=0;micros=0;tickRemainder=0;frozenTick=0;clockHz=8000000;
    primask=ipsr=0;forcedLow=0;odr=GPIO_Pin_10|GPIO_Pin_11;
    pinMode=GPIO_Mode_AF;releaseAt=pulses=0;scenario=NORMAL;
    transfers=resets=stopRequests=0;System_ErrFlag.u8ErrFlag_Com_AFE1=0;
    assert(!AFE_I2C_Init());assert(!(bus.OAR1&I2C_OAR1_OA1EN));
}
int main(void) {
    unsigned char data[255], saved[40], crcInput[2];
    unsigned count,i; int n;
    resetTest();memset(data,0x5a,sizeof(data));
    for(n=1;n<=255;n++) {
        assert(!I2CSendBytes(8,data,n,&count) && count==(unsigned)n);
        assert(!memcmp(data,output,n));
        assert(!I2CReadBytes(8,data,n,&count) && count==(unsigned)n);
    }
    assert(!transfers || addressRegister==16);
    resetTest();assert(I2CSendBytes(8,data,256,&count) && !count && !transfers);
    assert(I2CSendBytes(8,0,1,&count));assert(I2CReadBytes(8,data,0,&count));
    assert(I2CReadBytes(128,data,1,&count));assert(I2CSendBytes(8,data,1,0));
    resetTest();primask=1;assert(I2CSendBytes(8,data,1,&count) && !transfers);primask=0;
    ipsr=1;assert(I2CReadBytes(8,data,1,&count) && !transfers);ipsr=0;
    resetTest();clockHz=12000000;assert(AFE_I2C_Init());assert(!bus.enabled);
    resetTest();clockHz=48000000;assert(!AFE_I2C_Init());
    resetTest();scenario=LAST_NACK;
    assert(I2CSendBytes(8,data,3,&count) && count==3);
    assert(afeI2cDiag.lastError==AFE_I2C_NACK && afeI2cDiag.lastStage==AFE_I2C_STAGE_STOP);
    assert(transfers==1);scenario=NORMAL;assert(!I2CSendBytes(8,data,3,&count));
    assert(afeI2cDiag.lastError==AFE_I2C_NACK);
    resetTest();scenario=PARTIAL_NACK;assert(I2CSendBytes(8,data,3,&count) && count==1);
    resetTest();scenario=ADDRESS_NACK;
    assert(I2CReadRegisterByteWithCRC(8,1,data));assert(transfers==1);
    resetTest();scenario=EARLY_STOP;assert(I2CSendBytes(8,data,3,&count));
    assert(afeI2cDiag.lastError==AFE_I2C_UNEXPECTED_STOP);
    for(i=0;i<3;i++) {
        resetTest();scenario=BUS_FAULT;errorFlag=i==0?I2C_ISR_BERR:i==1?I2C_ISR_ARLO:I2C_ISR_OVR;
        assert(I2CSendBytes(8,data,1,&count));assert(transfers==1);
        if(i==1) assert(!stopRequests && !pulses);
        scenario=NORMAL;assert(!I2CSendBytes(8,data,1,&count));
    }
    for(i=0;i<4;i++) {
        resetTest();scenario=i==0?RX_STALL:i==1?TX_STALL:STOP_STALL;frozenTick=i==3;
        if(i==0) assert(I2CReadBytes(8,data,3,&count));
        else assert(I2CSendBytes(8,data,3,&count));
        assert(micros>=25000 && micros<30000);
        assert(afeI2cDiag.lastError==AFE_I2C_TIMEOUT && !afeI2cDiag.recoveryError);
        scenario=NORMAL;assert(!I2CSendBytes(8,data,1,&count));
    }
    resetTest();afeI2cMs=0xfffffff5u;scenario=TX_STALL;
    assert(I2CSendBytes(8,data,1,&count) && micros<30000);
    resetTest();bus.ISR=I2C_ISR_BUSY;
    assert(!I2CSendBytes(8,data,1,&count));assert(afeI2cDiag.recoveryCount==1);
    resetTest();forcedLow=GPIO_Pin_11;releaseAt=3;bus.ISR=I2C_ISR_BUSY;
    assert(!I2CSendBytes(8,data,1,&count));assert(pulses>=3 && pulses<=10);
    assert(pinMode==GPIO_Mode_AF && !forcedLow);
    resetTest();forcedLow=GPIO_Pin_10;
    assert(I2CSendBytes(8,data,1,&count));assert(!transfers && !pulses);
    assert(afeI2cDiag.recoveryError==AFE_I2C_SCL_STUCK && pinMode==GPIO_Mode_AF);
    resetTest();forcedLow=GPIO_Pin_11;
    assert(I2CSendBytes(8,data,1,&count));assert(!transfers && pulses==10);
    assert(afeI2cDiag.recoveryError==AFE_I2C_SDA_STUCK && pinMode==GPIO_Mode_AF);
    forcedLow=0;assert(!I2CSendBytes(8,data,1,&count));
    /* CRC uses the actual production algorithm; invalid blocks do not publish any bytes. */
    resetTest();memset(data,0xa5,sizeof(data));memset(saved,0xa5,sizeof(saved));
    for(i=0;i<40;i++) { wire[2*i]=(unsigned char)i;wire[2*i+1]=CRC8(&wire[2*i],1,CRC_KEY); }
    crcInput[0]=17;crcInput[1]=wire[0];wire[1]=CRC8(crcInput,2,CRC_KEY);
    wire[79]^=1;assert(I2CReadBlockWithCRC(8,1,data,40));assert(!memcmp(data,saved,40));
    assert(afeI2cDiag.lastError==AFE_I2C_CRC);
    wire[79]^=1;assert(!I2CReadBlockWithCRC(8,1,data,40));
    for(i=0;i<40;i++) assert(data[i]==i);
    assert(I2CReadBlockWithCRC(8,1,data,41));assert(I2CReadBlockWithCRC(8,1,0,1));
    assert(I2CWriteBlockWithCRC(8,1,data,15));assert(I2CWriteBlockWithCRC(8,1,data,0));
    assert(!I2CWriteBlockWithCRC(8,1,data,14));assert(expected==29);
    puts("PASS: I2C errors, last-byte NACK, real/frozen tick timeouts, recovery, stuck wires, counts, bounds and atomic CRC publication");
    return 0;
}
'''

if __name__ == "__main__":
    main()
