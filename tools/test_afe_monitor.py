"""编译实际AFE监控/故障休眠函数，验证状态转换和计时边界。产物仅写用户临时区。"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/030-TI/afe-monitor-tests"

PRELUDE = r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
struct { volatile UINT16 cnt_10ms; } sys_time;
struct { UINT8 u8ErrFlag_Com_AFE1; } System_ErrFlag;
struct { struct { UINT8 b1Status_AFE1; } bits; } SystemStatus;
enum { ERROR_AFE1, ERROR_REMOVE_AFE1, ERROR_STATUS_AFE1,
       ERROR_STATUS_AFE2, ERROR_STATUS_EEPROM_COM, ERROR_STATUS_EEPROM_STORE };
#define DEEP_MODE 1
static unsigned wakeCalls, initCalls, sleepCalls;
static UINT8 afe2, eepromCom, eepromStore;
static UINT8 System_ERROR_UserCallback(int code) {
    switch (code) {
    case ERROR_AFE1: ++System_ErrFlag.u8ErrFlag_Com_AFE1; break;
    case ERROR_REMOVE_AFE1: System_ErrFlag.u8ErrFlag_Com_AFE1=0; break;
    case ERROR_STATUS_AFE1: return System_ErrFlag.u8ErrFlag_Com_AFE1;
    case ERROR_STATUS_AFE2: return afe2;
    case ERROR_STATUS_EEPROM_COM: return eepromCom;
    case ERROR_STATUS_EEPROM_STORE: return eepromStore;
    }
    return 0;
}
static void App_WakeUpAFE(void) { ++wakeCalls; }
static void InitAFE1(void) { ++initCalls; }
static void entersleep(int mode) { assert(mode==DEEP_MODE); ++sleepCalls; }
'''

TESTS = r'''
static void reset(void) {
    memset(&afeMonitor,0,sizeof(afeMonitor));
    System_ErrFlag.u8ErrFlag_Com_AFE1=0;
    SystemStatus.bits.b1Status_AFE1=0;
    afe2=eepromCom=eepromStore=0;
    App_CommunicationFaultSleep(); /* 清除函数私有计时状态。 */
    sys_time.cnt_10ms=0;
    wakeCalls=initCalls=sleepCalls=0;
}
int main(void) {
    unsigned i;
    reset();
    MonitorAFE(0);MonitorAFE(0);assert(!SystemStatus.bits.b1Status_AFE1);
    MonitorAFE(0);assert(SystemStatus.bits.b1Status_AFE1);
    MonitorAFE(1);assert(System_ErrFlag.u8ErrFlag_Com_AFE1==1);
    for(i=0;i<1000;++i) MonitorAFE(1);
    assert(System_ErrFlag.u8ErrFlag_Com_AFE1==1 && !wakeCalls);
    sys_time.cnt_10ms=599;MonitorAFE(1);assert(!wakeCalls);
    sys_time.cnt_10ms=600;MonitorAFE(1);assert(wakeCalls==1 && initCalls==1);
    MonitorAFE(1);assert(wakeCalls==1);
    sys_time.cnt_10ms=1200;MonitorAFE(1);assert(wakeCalls==2);
    MonitorAFE(0);MonitorAFE(0);assert(System_ErrFlag.u8ErrFlag_Com_AFE1);
    MonitorAFE(1);MonitorAFE(0);MonitorAFE(0);assert(System_ErrFlag.u8ErrFlag_Com_AFE1);
    MonitorAFE(0);assert(!System_ErrFlag.u8ErrFlag_Com_AFE1 && !afeMonitor.recovering);
    /* 非采样寄存器访问报错，同样必须重新累计三次成功。 */
    System_ERROR_UserCallback(ERROR_AFE1);MonitorAFE(0);MonitorAFE(0);
    assert(System_ErrFlag.u8ErrFlag_Com_AFE1);MonitorAFE(0);
    assert(!System_ErrFlag.u8ErrFlag_Com_AFE1);
    reset();sys_time.cnt_10ms=65500;MonitorAFE(1);
    sys_time.cnt_10ms=(UINT16)(65500u+599u);MonitorAFE(1);assert(!wakeCalls);
    ++sys_time.cnt_10ms;MonitorAFE(1);assert(wakeCalls==1);
    /* 无需调用采样，EEPROM故障也能独立满5分钟休眠。 */
    reset();eepromCom=1;App_CommunicationFaultSleep();
    for(i=0;i<1000;++i) App_CommunicationFaultSleep();assert(!sleepCalls);
    sys_time.cnt_10ms=29999;App_CommunicationFaultSleep();assert(!sleepCalls);
    sys_time.cnt_10ms=30000;App_CommunicationFaultSleep();assert(sleepCalls==1);
    App_CommunicationFaultSleep();assert(sleepCalls==1);
    /* 故障源不能相互接力凑满持续时间，清除后必须重新计时。 */
    reset();afe2=1;App_CommunicationFaultSleep();
    sys_time.cnt_10ms=20000;afe2=0;eepromStore=1;App_CommunicationFaultSleep();
    sys_time.cnt_10ms=30000;App_CommunicationFaultSleep();assert(!sleepCalls);
    eepromStore=0;App_CommunicationFaultSleep();eepromStore=1;App_CommunicationFaultSleep();
    sys_time.cnt_10ms=50000;App_CommunicationFaultSleep();assert(!sleepCalls);
    sys_time.cnt_10ms=60000;App_CommunicationFaultSleep();assert(sleepCalls==1);
    reset();sys_time.cnt_10ms=60000;afe2=eepromCom=1;
    System_ErrFlag.u8ErrFlag_Com_AFE1=1;App_CommunicationFaultSleep();
    sys_time.cnt_10ms=(UINT16)(60000u+30000u);App_CommunicationFaultSleep();
    assert(sleepCalls==1); /* 回绕且多个故障同时到期，只休眠一次。 */
    puts("PASS: AFE retry/recovery, fault latching, independent sleep timers and tick wrap");
    return 0;
}
'''

def main():
    source = (ROOT / "Code/Source/DataDeal.c").read_text(encoding="gbk")
    monitor = source[source.index("#define AFE_RETRY_INTERVAL_10MS"):source.index("void test_Autocurrent_cycle")]
    source = (ROOT / "Code/Source/System_Monitor.c").read_text(encoding="gbk")
    sleep = source[source.index("#define SYSTEM_FAULT_SLEEP_10MS"):]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "test.c").write_text(PRELUDE + monitor + sleep + TESTS, encoding="utf-8")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")
    (OUT / "build.cmd").write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /W3 /Od test.c /Fe:test.exe\nexit /b %errorlevel%\n', encoding="utf-8")
    subprocess.run(["cmd", "/c", str(OUT / "build.cmd")], cwd=OUT, check=True)
    subprocess.run([str(OUT / "test.exe")], cwd=OUT, check=True)

if __name__ == "__main__":
    main()
