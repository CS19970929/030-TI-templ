"""Run production reed-switch/LED C against shutdown scenarios; outputs stay in user TEMP."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/030-TI/minimal-five-tests"

PRELUDE = r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
enum { LED_BAR_STARTUP, LED_BAR_NORMAL, LED_BAR_CHG, LED_BAR_DSG, LED_BAR_FAULT };
typedef int LEDBAR_COMMAND;
enum { FORCE_KEEP_MODE, FORCE_CLOSE_MODE, DEEP_MODE, GPIO_CHG, GPIO_DSG };
static unsigned close_calls;
static void BQ769X0_DriverMos_Ctrl(int type, int on) { assert(!on); assert(type==GPIO_CHG || type==GPIO_DSG); ++close_calls; }
enum { GPIO_GAN1, GPIO_GAN2, GPIO_GAN3, GPIO_GAN4, GPIO_SWT_AD, GPIO_SWT_EN, GPIOA };
#define PIN_GAN1 0
#define PIN_GAN2 0
#define PIN_GAN3 0
#define PIN_GAN4 0
#define PIN_SWT_AD 0
#define PIN_SWT_EN 0
#define GPIO_Pin_0 0
#define Bit_RESET 0
#define Bit_SET 1
#define ERROR_STATUS_TEMP_BREAK 0
#define ERROR_STATUS_CBC_DSG 1
static unsigned pins[7] = {1, 0, 1, 1, 1, 1, 0};
static unsigned leds[5], sleep_calls;
#define MCUO_SOC_20 leds[0]
#define MCUO_SOC_40 leds[1]
#define MCUO_SOC_60 leds[2]
#define MCUO_SOC_80 leds[3]
#define MCUO_SOC_100 leds[4]
struct { volatile UINT16 cnt_10ms; unsigned cnt_enter_chg_open, cnt_enter_dsg_open; } sys_time;
struct { struct { UINT8 b1Sys100msFlag; } bits; } g_st_SysTimeFlag = {{1}};
struct { struct { UINT8 b1StartUpBMS, b1Status_MOS_CHG, b1Status_MOS_DSG; } bits; } SystemStatus;
struct { UINT16 u16Sleep_VirCur_Chg, u16Soc_V_100; } OtherElement = {2, 4200};
struct {
    UINT16 u16Ichg, u16VCellMax;
    struct { UINT16 u16Soc; } SocElement;
    struct { UINT16 all; } unMdlFault_Third;
} g_stCellInfoReport = {0, 3600, {50}, {0}};
struct { struct { struct { UINT8 b2_DriverOFF_Flag; } bits; } DriverForceExt; UINT8 u8_DriverCtrl_Right; struct { struct { UINT8 b1Status_MOS_CHG, b1Status_MOS_DSG; } bits; } MosRelay_Status; } Driver_Element;
static unsigned GPIO_ReadInputDataBit(int port, int pin) { (void)pin; return pins[port]; }
static void GPIO_WriteBit(int port, int pin, int value) { (void)pin; pins[port]=value; }
static int System_ERROR_UserCallback(int error) { (void)error; return 0; }
static void entersleep(int mode) { assert(mode==DEEP_MODE); ++sleep_calls; }
enum { WAKE_DISPLAY_MODE_NONE, WAKE_DISPLAY_MODE_SOC_PREVIEW,
       WAKE_DISPLAY_MODE_WATER_ALARM, WAKE_DISPLAY_MODE_BOOT_SEQUENCE,
       WAKE_DISPLAY_MODE_CHARGER_WAKE };
static UINT16 wake_mode;
UINT8 WakeDisplayState_Read(UINT16 *mode, UINT16 *soc) { *mode=wake_mode; *soc=50; return 1; }
static void WakeDisplayMode_ClearKeepSoc(void) { wake_mode=0; }
static void LedBar_InitOutputPins(void) {}
static UINT8 LedBar_HandleWakeSocPreviewAfterReset(UINT16 soc) { (void)soc; return 0; }
static void SleepDeal_ReenterDeepSleepFromWakePreview(void) { abort(); }
'''

TESTS = r'''
static void poll_at(unsigned tick) {
    sys_time.cnt_10ms=(UINT16)tick;
    ganhuangguan_Logi();
    APP_LedBar();
}
static void expect_shutdown(void) {
    unsigned i;
    assert(sleep_calls==1 && close_calls>=2);
    assert(Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag==FORCE_CLOSE_MODE);
    for(i=0;i<5;i++) assert(!leds[i]);
    LedBar_Command=LED_BAR_CHG;
    LedBar_RequestShutdownAnimation();
    LedBar_RequestSocTemporary(100,30);
    APP_LedBar();
    for(i=0;i<5;i++) assert(!leds[i]);
    assert(ganhuangguan_IsOutputBlocked());
    Driver_Element.u8_DriverCtrl_Right=1;
    Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG=1;
    Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG=1;
    SystemStatus.bits.b1Status_MOS_CHG=1;
    SystemStatus.bits.b1Status_MOS_DSG=1;
    Drivers_External_Ctrl();
    assert(!Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
    assert(!Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG);
}
int main(int argc, char **argv) {
    unsigned scenario,t,start=1000;
    assert(argc==2); scenario=(unsigned)atoi(argv[1]);
    assert(is_open_gan1());
    if(scenario==1 || scenario==6 || scenario==7 || scenario==8) {
        if(scenario==7) start=65500;
        pins[GPIO_GAN2]=1; poll_at(start);
        for(t=0;t<10000;t++) poll_at(start);
        assert(!sleep_calls);
        poll_at(start+199); assert(!sleep_calls);
        if(scenario==6) {
            pins[GPIO_GAN2]=0; poll_at(start+200);
            pins[GPIO_GAN2]=1; start+=201; poll_at(start);
            poll_at(start+199); assert(!sleep_calls);
        }
        poll_at(start+(scenario==8?250:200)); expect_shutdown();
    } else if(scenario==11) {
        pins[GPIO_GAN2]=1;
        pins[GPIOA]=1; g_stCellInfoReport.u16Ichg=10;
        for(t=1;t<15000;t++) { poll_at(t); assert(!sleep_calls); }
    } else {
        if(scenario==10) start=65000;
        if(scenario==3) {
            pins[GPIOA]=1; g_stCellInfoReport.u16Ichg=10; poll_at(start-1);
            assert(ganhuangguan_IsChargeLatched());
        }
        if(scenario==4 || scenario==5) {
            /* Water detected on either wake path reaches WATER_ALARM startup. */
            wake_mode=WAKE_DISPLAY_MODE_WATER_ALARM; LedBar_StartUp();
        } else pins[GPIO_SWT_AD]=0;
        poll_at(start); assert(close_calls>=2);
        pins[GPIO_SWT_AD]=1; /* Sensor output changes after power is disabled. */
        if(scenario==3) {
            pins[GPIO_GAN2]=1; poll_at(start+1); expect_shutdown();
        } else {
            for(t=1;t<12000;t+=17) {
                poll_at(start+t); assert(!sleep_calls);
                assert(ganhuangguan_IsOutputBlocked());
                if(scenario==9) {
                    LedBar_RequestShutdownAnimation();
                    assert(!LedBar_IsShutdownAnimationActive());
                }
            }
            poll_at(start+11999); assert(!sleep_calls);
            poll_at(start+12000); expect_shutdown();
        }
    }
    printf("PASS: scenario %u\n",scenario); return 0;
}
'''


def read_source(path, ref):
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT).decode("latin1")
    return (ROOT / path).read_text(encoding="latin1")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-ref", help="Run identical scenarios against an earlier git revision")
    args = parser.parse_args()
    led = read_source("Code/Source/LedBar.c", args.source_ref)
    gan = read_source("Code/Source/gan_huang_guan_logi.c", args.source_ref)
    # Keep production state, LED arbitration and shutdown functions; stub only boot preview/GPIO.
    led_parts = led[led.index("uint8_t sleep_reason"):led.index("static void LedBar_InitOutputPins")]
    led_parts += led[led.index("void LedBar_RequestSocTemporary"):led.index("static void LedBar_RunBootAnimationBlocking")]
    led_parts += led[led.index("void LedBar_StartUp(void)"):led.index("void test_water_in")]
    io = read_source("Code/Source/IO_Control.c", args.source_ref)
    io = io[io.index("void Drivers_External_Ctrl(void)"):io.index("void InitMosRelay_DOx(void)")]
    source = PRELUDE + led_parts + gan.replace('#include "main.h"', '') + io + TESTS
    if not args.source_ref:
        main_source = read_source("Code/Source/main.c", None)
        assert main_source.index('ganhuangguan_Logi();') < main_source.index('App_AFEGet();')
        assert 'ganhuangguan_Logi();' not in read_source("Code/Source/IO_Control.c", None)
    out = OUT / (args.source_ref or "working")
    out.mkdir(parents=True, exist_ok=True)
    (out / "test.c").write_text(source, encoding="utf-8")
    vcvars = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")
    (out / "build.cmd").write_text(
        f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /W3 /Od /RTC1 test.c /Fe:test.exe\nexit /b %errorlevel%\n',
        encoding="utf-8")
    subprocess.run(["cmd", "/c", str(out / "build.cmd")], cwd=out, check=True)
    failed = []
    for scenario in range(1, 12):
        result = subprocess.run([str(out / "test.exe"), str(scenario)], cwd=out, timeout=10)
        if result.returncode:
            failed.append(scenario)
    print(f"{11-len(failed)}/11 passed; failed scenarios: {failed}")
    raise SystemExit(bool(failed))


if __name__ == "__main__":
    main()
