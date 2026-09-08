"""真实 Modbus CRC、地址校验、数据序列化和应答边界回归。
中间文件全部置于 LOCALAPPDATA/CodexTemp，使用 MSVC /RTC1 与缓冲区哨兵。
"""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/030-TI/sci-protocol-tests'

def read(name):
    return (ROOT / name).read_bytes().decode('latin1').replace('\r\n', '\n')

def clean(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)

def function(source, name):
    m = re.search(r'^(?:static )?(?:void|UINT8|UINT16) ' + name + r'\([^;]*?\)\n\{', source, re.M)
    if not m: raise ValueError(name)
    return clean(source[m.start():source.index('\n}', m.end()) + 2])

def declaration(source, name):
    return re.search(r'(?:struct|enum) ' + name + r'\s*\{.*?\};', clean(source), re.S).group()

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sci = read('Code/Source/Sci_Upper.c')
    data = read('Code/Source/DataDeal.h')
    code = PRELUDE
    code += declaration(data, 'TempArray') + declaration(data, 'tagInfoForKBArray')
    code += '\n' + clean(read('Code/Source/Sci_Upper.h')) + '\n'
    code += declaration(data, 'OTHER_ELEMENT')
    code += declaration(read('Code/Source/Fault.h'), 'PRT_E2ROM_PARAS')
    code += declaration(read('Code/Source/Heat_Cool.h'), 'HEAT_COOL_ELEMENT')
    prod = clean(read('Code/Source/ProductionID.h'))
    code += re.search(r'typedef struct\s*\{.*?\}\s*PRODUCTION_ID_INFO;', prod, re.S).group()
    for file, names in [
        ('Code/Source/EEPROM.h', ['E2P_PARA_NUM_PROTECT','E2P_PARA_NUM_RTC','E2P_PARA_NUM_OTHER_ELEMENT1','E2P_PARA_NUM_HEAT_COOL']),
        ('Code/Source/LogRecord.h', ['EVENT_RECORD_LENGTH']),
        ('Code/Source/Fault.h', ['Record_len']),
        ('Code/Source/SOC.h', ['SOC_TABLE_SIZE']),
        ('Code/Source/DataDeal.h', ['CompensateNUM'])]:
        for name in names:
            code += '\n' + re.search(r'^#define\s+'+name+r'\s+[^\n]+', clean(read(file)), re.M).group() + '\n'
    code += GLOBALS
    code += function(read('Code/Source/PubFunc.c'), 'Sci_CRC16RTU')
    code += function(sci, 'Sci_IsValidWriteRegsByteCount')
    start = sci.index('#define SCI_EX_ILLEGAL_ADDRESS')
    end = sci.index('void CRC_verify(', start)
    code += '\n' + clean(sci[start:end]) + '\n'
    for name in ['CRC_verify','Sci_ValidateReadRequest','Sci_Deal_ReadRegs_0x03']:
        code += function(sci,name)
    code += function(read('Code/Source/LogRecord.c'), 'Sci_ACK_0x03_ReadRegs_EventRecord')
    for name in ['Sci_ACK_0x03_ReadRegs_LCD','Sci_ACK_0x03_ReadRegs_Data',
                 'Sci_ACK_0x03_RW_Data_Pro','Sci_ACK_0x03_RW_Data_Cali',
                 'Sci_ACK_0x03_RW_Data_Other','Sci_ACK_0x03_RW_Data_OtherCanAdd',
                 'Sci_ACK_0x03','Sci_ACK_0x06_0x10', 'Sci_WrRegs_0x10_SystemElement',
                 'Sci_WrRegs_0x10_SN_Version']:
        code += function(sci,name)
    code += re.search(r"typedef struct \{.*?\} SciPort;", sci, re.S).group()
    code += DISPATCH_STUBS
    code += function(sci, 'Sci_ProcessRequest')
    code += TESTS
    (OUT/'test.c').write_text(code,encoding='utf-8')
    vcvars = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat')
    (OUT/'build.cmd').write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /W3 /Od /RTC1 /Zi test.c /Fe:test.exe\nif errorlevel 1 exit /b 1\ntest.exe\nexit /b %errorlevel%\n',encoding='utf-8')
    env = os.environ.copy()
    subprocess.run(['cmd','/c',str(OUT/'build.cmd')],cwd=OUT,env=env,check=True)

PRELUDE = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int8_t INT8;
typedef int16_t INT16;
typedef struct { UINT32 dummy; } USART_TypeDef;
#define PRODUCT_ID_LENGTH_MAX 32
"""
GLOBALS = r"""
struct stCell_Info g_stCellInfoReport;
struct OTHER_ELEMENT OtherElement;
struct PRT_E2ROM_PARAS PRT_E2ROMParas;
struct HEAT_COOL_ELEMENT Heat_Cool_Element;
PRODUCTION_ID_INFO ProductionInfor;
struct { UINT8 u8ErrFlag_Com_AFE1; UINT8 rest[23]; } System_ErrFlag;
struct { UINT32 all; } SystemStatus, System_OnOFF_Func;
UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];
UINT16 SOC_Table_Set[SOC_TABLE_SIZE], SOC_Table_LiFePO[SOC_TABLE_SIZE], SocTable_TernaryLi[SOC_TABLE_SIZE];
UINT16 CopperLoss[CompensateNUM], CopperLoss_Num[CompensateNUM];
UINT16 Fault_record_Third[Record_len], RTC_Fault_record_Third[Record_len][6];
UINT16 Fault_record_First2[Record_len], Fault_record_Second2[Record_len], Fault_record_Third2[Record_len];
UINT8 FaultPoint_Third, FaultPoint_First2, FaultPoint_Second2, FaultPoint_Third2;
UINT8 BMS_LOG_POINT, BMS_LOG_RECORD[EVENT_RECORD_LENGTH][2];
struct { UINT32 before; UINT8 buf[SCI_TX_BUF_LEN]; UINT32 after; } scratch;
#define g_u8SCITxBuff scratch.buf
#define OPEN 0
#define SOC_TABLE_TEST 0
#define SOC_TABLE_LIFEPO 1
#define SOC_TABLE_TERNARYLI 2
#define SOC_TABLE_LIFEPO2 3
static UINT8 Sci_IsValidWriteRegsByteCount(struct RS485MSG *s);
UINT32 u32E2P_OtherElement1_WriteFlag, g_u32CS_Res_AFE;
UINT8 SeriesNum;
const UINT8 SeriesSelect_AFE1[16][16] = {0};
#define EE_FLAG_OTHER1_SYS_SERIES_NUM 1u
#define EE_FLAG_OTHER1_SYS_CS_RESIS 2u
#define EE_FLAG_OTHER1_SYS_CS_NUM 4u
#define EE_FLAG_OTHER1_SYS_PRECHG_TIME 8u
static void InitData_Drivers(void) {}
"""
DISPATCH_STUBS = r"""
static int activityCount, writeCount;
static void Sci_ResetFrameState(SciPort *port) { memset(port->msg,0,sizeof(*port->msg)); }
static void Sci_RS485ValidRequest(SciPort *port) { activityCount++; }
static UINT8 P12_VerifyFrame(struct RS485MSG *s) { return 0; }
static UINT8 P12_BuildResponse(struct RS485MSG *s) { return 0; }
static void Sci_Deal_WrReg_0x06(struct RS485MSG *s) { writeCount++; }
static void Sci_Deal_WrRegs_0x10(struct RS485MSG *s) { writeCount++; }
"""
TESTS = r"""
static struct { UINT32 before; struct RS485MSG msg; UINT32 after; } guarded;
static void request(UINT16 addr, UINT16 count) {
    struct RS485MSG *s=&guarded.msg;
    memset(&guarded,0,sizeof(guarded));
    guarded.before=guarded.after=scratch.before=scratch.after=0xabcdef12u;
    memset(scratch.buf,0xa5,sizeof(scratch.buf));
    s->ptr_no=8; s->enRs485CmdType=RS485_CMD_READ_REGS;
    s->u16Buffer[0]=1;s->u16Buffer[1]=3;
    s->u16Buffer[2]=(UINT8)(addr>>8);s->u16Buffer[3]=(UINT8)addr;
    s->u16Buffer[4]=(UINT8)(count>>8);s->u16Buffer[5]=(UINT8)count;
    UINT16 crc=Sci_CRC16RTU(s->u16Buffer,6);
    s->u16Buffer[6]=(UINT8)crc;s->u16Buffer[7]=(UINT8)(crc>>8);
}
static void response(void) {
    struct RS485MSG *s=&guarded.msg;
    Sci_ACK_0x03(s);
    assert(s->AckLenth>=5 && s->AckLenth<=RS485_MAX_BUFFER_SIZE);
    assert(Sci_CRC16RTU(s->u16Buffer,s->AckLenth)==0);
    assert(guarded.before==0xabcdef12u && guarded.after==0xabcdef12u);
    assert(scratch.before==0xabcdef12u && scratch.after==0xabcdef12u);
}
static void valid(UINT16 addr,UINT16 count) {
    request(addr,count); CRC_verify(&guarded.msg);assert(guarded.msg.AckType==RS485_ACK_POS);
    Sci_Deal_ReadRegs_0x03(&guarded.msg);assert(guarded.msg.AckType==RS485_ACK_POS);
    response();assert(guarded.msg.u16Buffer[1]==3 && guarded.msg.AckLenth==2*count+5);
}
static void invalid(UINT16 addr,UINT16 count,UINT8 error) {
    request(addr,count);Sci_Deal_ReadRegs_0x03(&guarded.msg);
    assert(guarded.msg.AckType==RS485_ACK_NEG && guarded.msg.ErrorType==error);
    response();assert(guarded.msg.AckLenth==5 && guarded.msg.u16Buffer[1]==0x83);
    valid(0xd000,1); /* A bad request must not poison the next transaction. */
}
int main(void) {
    struct RS485MSG *s=&guarded.msg;
    UINT32 a; UINT16 n;
    const UINT16 counts[]={0,1,2,63,123,124,256,65535};
    const UINT16 pages[][2]={{0x2000,2*KB_NUM},{0x2100,E2P_PARA_NUM_PROTECT},
        {0x2200,SCI_OTHER_REGS},{0x2300,SCI_EXTRA_REGS},{0xc000,5},{0xc001,70},
        {0xc002,48},{0xc008,100},{0xd000,63},{0xd100,33},{0xd200,1}};
    request(0,3);assert(s->u16Buffer[6]==5 && s->u16Buffer[7]==0xcb);
    Sci_Deal_ReadRegs_0x03(s);response();
    { const UINT8 expected[]={1,0x83,2,0xc0,0xf1};assert(!memcmp(s->u16Buffer,expected,5)); }
    invalid(0,3,2);invalid(0xffff,2,2);invalid(0xd03e,2,2);
    invalid(0xd000,0,3);invalid(0xd000,124,3);invalid(0x2000,65535,3);
    for(n=0;n<sizeof(pages)/sizeof(pages[0]);n++) {
        valid(pages[n][0],pages[n][1]);
        invalid(pages[n][0],pages[n][1]+1,2);
    }
    valid(0xd120,1);valid(0x205d,1);invalid(0x205e,1,2);
    g_stCellInfoReport.u16VCell[1]=0x1234;
    valid(0xd001,1);assert(s->u16Buffer[3]==0x12 && s->u16Buffer[4]==0x34);
    g_u16CalibCoefK[1]=0x2345;
    valid(0x2002,1);assert(s->u16Buffer[3]==0x23 && s->u16Buffer[4]==0x45);
    memset(ProductionInfor.BMS_SerialNumber,0x42,PRODUCT_ID_LENGTH_MAX);
    memset(ProductionInfor.BMS_HardWareVersion,0x43,PRODUCT_ID_LENGTH_MAX);
    memset(ProductionInfor.BMS_SoftWareVersion,0x44,PRODUCT_ID_LENGTH_MAX);
    valid(0xc002,48);assert(s->u16Buffer[3]==0x42 && s->u16Buffer[35]==0x43 && s->u16Buffer[67]==0x44);
    /* Every 16-bit address plus length boundaries, real serializers + CRC. */
    for(a=0;a<=65535u;a++) for(n=0;n<sizeof(counts)/sizeof(counts[0]);n++) {
        request((UINT16)a,counts[n]);Sci_Deal_ReadRegs_0x03(s);response();
    }
    /* Actual protocol dispatcher: bad CRC is dropped without side effects;
     * invalid addresses produce errors, and the next valid frame still works. */
    SciPort port={0,s,1,0,0,0};
    request(0,3);Sci_ProcessRequest(&port);
    assert(s->csr==RS485_STA_RX_OK && s->AckType==RS485_ACK_NEG && !activityCount);
    response();assert(s->u16Buffer[2]==2);
    request(0xd000,1);s->u16Buffer[7]^=1;Sci_ProcessRequest(&port);
    assert(s->csr==RS485_STA_IDLE && !activityCount && !writeCount);
    request(0xd000,1);Sci_ProcessRequest(&port);response();assert(activityCount==1);
    request(0x2000,2);s->u16Buffer[1]=16;s->enRs485CmdType=RS485_CMD_WRITE_REGS;
    s->u16Buffer[6]=4;s->ptr_no=13; /* stale/wrong CRC, never write parameters */
    Sci_ProcessRequest(&port);assert(!writeCount && s->csr==RS485_STA_IDLE);
    /* Response boundary must also reject a caller that skipped parsing. */
    request(0,3);response();assert(s->u16Buffer[1]==0x83 && s->u16Buffer[2]==2);
    /* Truncated/overlong/malformed frames cannot reach CRC out of bounds. */
    for(n=0;n<256;n++) {
        request(0xd000,1);s->ptr_no=(UINT8)n;CRC_verify(s);
        assert((s->AckType==RS485_ACK_POS)==(n==8));
    }
    request(0xd000,1);s->u16Buffer[7]^=1;CRC_verify(s);assert(s->AckType==RS485_ACK_NEG);
    for(n=0;n<256;n++) {
        request(0x2000,2);s->u16Buffer[1]=16;s->u16Buffer[6]=(UINT8)n;
        s->ptr_no=(UINT8)(n+9);assert(Sci_ModbusFrameValid(s)==(n==4));
    }
    request(0x2000,0);s->u16Buffer[1]=16;s->u16Buffer[6]=0;s->ptr_no=9;
    assert(!Sci_ModbusFrameValid(s));
    /* Reject divisor/index corruption before any parameter or EEPROM flag changes. */
    memset(&OtherElement,0x55,sizeof(OtherElement));struct OTHER_ELEMENT saved=OtherElement;
    request(0x231c,4);s->u16Buffer[7]=0;s->u16Buffer[8]=12;
    s->u16Buffer[9]=0;s->u16Buffer[10]=0;s->u16Buffer[11]=0;s->u16Buffer[12]=3;
    Sci_WrRegs_0x10_SystemElement(s);assert(s->AckType==RS485_ACK_NEG);
    assert(memcmp(&OtherElement,&saved,sizeof(saved))==0 && !u32E2P_OtherElement1_WriteFlag);
    s->AckType=0;s->u16Buffer[8]=17;s->u16Buffer[10]=2;Sci_WrRegs_0x10_SystemElement(s);
    assert(s->AckType==RS485_ACK_NEG && memcmp(&OtherElement,&saved,sizeof(saved))==0);
    request(0xfff0,17);Sci_WrRegs_0x10_SN_Version(0xfff0,s);
    assert(s->AckType==RS485_ACK_NEG && !ProductionInfor.BMS_SerialNumber_WriteFlag);
    puts("PASS: 524288 address/count cases, real CRC/read serializers/responses, malformed frames, recovery, write divisor/index/metadata guards (RTC1 + canaries)");
    return 0;
}
"""
if __name__=='__main__': main()
