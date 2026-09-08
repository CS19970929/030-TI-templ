"""编译真实的新旧 Fault.c，逐字节比较每拍状态、滤波计数和事件顺序。

运行：python tools/test_warn_ctrl.py [--report artifacts/warn-refactor/test-report.json]
依赖：Git、Python 3、Visual Studio 2022 C++、Keil ARMCC。不连接硬件。
基准固定为重构前提交；比较对外状态及全部独立计数，不绑定内部函数架构。
全部生成的 C、EXE、OBJ 和轨迹位于 LOCALAPPDATA/CodexTemp。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "9a04ed8d1aa8649422b4ba8d91407adfdf356c12"


def clean(data):
    text = data.decode("latin1").replace("\r\n", "\n")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def function(text, name):
    match = re.search(r"(?:void|UINT8) " + name + r"\([^;]*?\)\n\{", text)
    if not match:
        raise ValueError(name)
    return text[match.start():text.index("\n}", match.end()) + 2]


def declaration(text, pattern):
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(pattern)
    return match.group() + "\n"


def header():
    fault = clean((ROOT / "Code/Source/Fault.h").read_bytes())
    fault = re.sub(r'^#include[^\n]*', '', fault, flags=re.M)
    sci = clean((ROOT / "Code/Source/Sci_Upper.h").read_bytes())
    pub = clean((ROOT / "Code/Source/PubFunc.h").read_bytes())
    sys = clean((ROOT / "Code/Source/System_Init.h").read_bytes())
    data = clean((ROOT / "Code/Source/DataDeal.h").read_bytes())
    text = PRELUDE + fault + "\n"
    text += declaration(data, r"enum TempArray\s*\{.*?\};")
    for name in ("SOC_CAL_ELEMENT_UPPER", "MDLCHGFAULT_BITS"):
        text += declaration(sci, r"struct " + name + r"\s*\{.*?\};")
    text += declaration(sci, r"union\s+MDLCHGFAULT_REG\s*\{.*?\};")
    text += declaration(sci, r"struct stCell_Info\s*\{.*?\};")
    text += declaration(sys, r"union SYS_TIME\s*\{.*?\n\};")
    text += declaration(pub, r"typedef\s+struct\s*\{.*?\}SPUBOPUPCHK;")
    text += "\nstruct stCell_Info g_stCellInfoReport;\nvolatile union SYS_TIME g_st_SysTimeFlag;\n"
    text += "struct { UINT16 cnt_10ms1, cnt_10ms2, cnt_10ms3, cnt_10ms4, cnt_10ms5; } sys_time;\n"
    text += "UINT8 App_PubOPUPChk(SPUBOPUPCHK *check);\nUINT8 System_ERROR_UserCallback(int code);\n"
    return text


def rules(old):
    order = re.findall(r"(App_\w+Check)\(\);", function(old, "App_WarnCtrl"))
    result = []
    for name in order:
        body = function(old, name)
        fields = dict(re.findall(r"t_sPubOPUPChk\.(\w+) = ([^;]+);", body))
        level, bit = re.search(r"unMdlFault_(\w+)\.bits\.(\w+)", body).groups()
        result.append(dict(name=name, fields=fields, level=level, bit=bit))
    assert len(result) == 26
    return result


def driver(old):
    entries = rules(old)
    init = []
    for field in re.findall(r"UINT16\s+(u16\w+)\s*;", clean((ROOT / "Code/Source/Fault.h").read_bytes())):
        # Valid high/low bounds for both high-active and low-active protections.
        low_active = any(s in field for s in ("Uvp", "UTp", "SocUp"))
        suffix = field.rsplit("_", 1)[-1]
        values = dict(First=200 if low_active else 100,
                      Second=100 if low_active else 200,
                      Third=80 if low_active else 220,
                      Rcv=180 if low_active else 120, Filter=3)
        init.append(f"    PRT_E2ROMParas.{field} = {values[suffix]};")
    config = "\nstatic void init_params(void) {\n" + "\n".join(init) + "\n}\n"
    config += "static void set_rule(int rule, int mode, UINT16 filter) {\n switch(rule) {\n"
    for i, row in enumerate(entries):
        f = row['fields']
        high, low = f['u16OPValB'], f['u16OPValS']
        sample = f['u16ChkVal']
        config += f" case {i}:\n  {f['u16TimeCntB']} = filter;\n"
        config += f"  switch(mode) {{\n"
        for mode, value in enumerate((f"{high}-1", high, f"{high}+1", f"{low}-1", low, f"{low}+1")):
            config += f"   case {mode}: {sample} = (UINT16)({value}); break;\n"
        config += f"   case 6: {high} = 0; {low} = 65535; break;\n"
        config += f"   case 7: {high} = {low} = 100; {sample} = 100; break;\n"
        config += "  }\n  break;\n"
    config += " }\n}\n"
    # Explicit function pointers exist only in the host test harness.
    config += "static void (*checks[26])(void) = {" + ",".join(r['name'] for r in entries) + "};\n"
    config += "static int logic[26] = {" + ",".join(r['fields']['u8FlagLogic'] for r in entries) + "};\n"
    return config + DRIVER


PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
#define ERROR_VDEATLE_OVER 1
#define ERROR_REMOVE_VDEATLE_OVER 2
'''

TRACE = r'''
static FILE *trace;
static UINT16 *counters[26];
static unsigned counterCount, frames, callbacks;
static void emit(UINT16 value) { assert(fwrite(&value, sizeof(value), 1, trace) == 1); }
static void snapshot(void) {
    int i;
    emit(g_stCellInfoReport.unMdlFault_First.all);
    emit(g_stCellInfoReport.unMdlFault_Second.all);
    emit(g_stCellInfoReport.unMdlFault_Third.all);
    emit(Fault_Flag_Fisrt.all); emit(Fault_Flag_Second.all); emit(Fault_Flag_Third.all);
    emit(FaultPoint_First2); emit(FaultPoint_Second2); emit(FaultPoint_Third2);
    for(i=0;i<Record_len;i++) {
        emit(Fault_record_First2[i]); emit(Fault_record_Second2[i]); emit(Fault_record_Third2[i]);
        /* Unused old First/Second banks were removed; keep zero trace slots. */
        emit(0); emit(0); emit(Fault_record_Third[i]);
    }
    emit(sys_time.cnt_10ms1); emit(sys_time.cnt_10ms2); emit(sys_time.cnt_10ms3);
    emit(sys_time.cnt_10ms4); emit(sys_time.cnt_10ms5);
    emit((UINT16)counterCount);
    for(i=0;i<(int)counterCount;i++) emit(*counters[i]);
}
UINT8 App_PubOPUPChk(SPUBOPUPCHK *c) {
    return Real_PubOPUPChk(c);
}
UINT8 System_ERROR_UserCallback(int code) {
    callbacks++;
    emit(0xF002); emit((UINT16)code); snapshot();
    return 0;
}
static void clear_records(void) {
    memset(Fault_record_First2, 0, sizeof(Fault_record_First2));
    memset(Fault_record_Second2, 0, sizeof(Fault_record_Second2));
    memset(Fault_record_Third2, 0, sizeof(Fault_record_Third2));
    FaultPoint_First2 = FaultPoint_Second2 = FaultPoint_Third2 = 0;
    Fault_Flag_Fisrt.all = Fault_Flag_Second.all = Fault_Flag_Third.all = 0;
}
static void flags(unsigned mask) {
    g_st_SysTimeFlag.bits.b1Sys10msFlag1 = (mask & 1) != 0;
    g_st_SysTimeFlag.bits.b1Sys10msFlag2 = (mask & 2) != 0;
    g_st_SysTimeFlag.bits.b1Sys10msFlag3 = (mask & 4) != 0;
    g_st_SysTimeFlag.bits.b1Sys10msFlag4 = (mask & 8) != 0;
    g_st_SysTimeFlag.bits.b1Sys10msFlag5 = (mask & 16) != 0;
}
static void step(int rule) {
    emit(0xF003); emit((UINT16)frames); emit((UINT16)(frames >> 16));
    if(rule < 0) App_WarnCtrl(); else checks[rule]();
    snapshot(); frames++;
}
static void reset(void) {
    unsigned i;
    for(i=0;i<counterCount;i++) *counters[i] = 0;
    memset(&g_stCellInfoReport, 0, sizeof(g_stCellInfoReport));
    memset(&sys_time, 0, sizeof(sys_time));
    clear_records(); init_params(); flags(31);
    g_stCellInfoReport.u16Ichg = g_stCellInfoReport.u16IDischg = 10;
}
static UINT32 randomState = 0x7690030;
static UINT32 rnd(void) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}
'''

DRIVER = r'''
static void snapshot(void);
static void reset(void);
static void clear_records(void);
static void flags(unsigned mask);
static void step(int rule);
static UINT32 rnd(void);
'''

TEST_MAIN = r'''
int main(int argc, char **argv) {
    int r, mode, n, f, gate;
    UINT16 filters[] = {0, 1, 3, 50000, 62535, 62536, 65535};
    assert(argc == 2); trace = fopen(argv[1], "wb"); assert(trace);
    assert(setvbuf(trace, NULL, _IOFBF, 1024*1024) == 0);
    register_counters();
    reset(); step(-1); assert(counterCount == 26);
    /* Every protection independently: exact boundaries, invalid/equal bounds,
       uint16 recovery-delay wrap, frozen gates, counter overflow and ring wrap. */
    for(r=0;r<26;r++) {
        for(f=0;f<7;f++) for(mode=0;mode<8;mode++) {
            reset(); set_rule(r, mode, filters[f]);
            for(n=0;n<14;n++) {
                if(n==4) clear_records();
                if(n==6) flags(0);
                if(n==7) flags(31);
                step(r);
            }
        }
        reset();
        for(n=0;n<66000;n++) { set_rule(r, logic[r] ? 1 : 4, 65535); step(r); }
        set_rule(r, logic[r] ? 4 : 1, 3);
        for(n=0;n<3010;n++) step(r);
        reset();
        for(n=0;n<50;n++) {
            set_rule(r, logic[r] ? 1 : 4, 1); step(r); clear_records(); step(r);
            set_rule(r, logic[r] ? 4 : 1, 1);
            for(f=0;f<3002;f++) step(r);
        }
        for(gate=0;gate<=2;gate++) {
            reset(); set_rule(r, logic[r] ? 1 : 4, 3); step(r); step(r);
            g_stCellInfoReport.u16Ichg = g_stCellInfoReport.u16IDischg = (UINT16)gate;
            for(n=0;n<5;n++) step(r);
            g_stCellInfoReport.u16Ichg = g_stCellInfoReport.u16IDischg = 10;
            step(r); clear_records(); step(r);
            set_rule(r, logic[r] ? 4 : 1, 3);
            g_stCellInfoReport.u16Ichg = g_stCellInfoReport.u16IDischg = 0;
            for(n=0;n<3010;n++) step(r);
        }
    }
    /* Simultaneous and staggered scheduling, online parameter changes,
       noisy thresholds, external latch/state changes and all gate boundaries. */
    reset();
    for(n=0;n<100000;n++) {
        r = (int)(rnd()%26); init_params(); set_rule(r, (int)(rnd()%8), (UINT16)(rnd()%7));
        g_stCellInfoReport.u16Ichg = (UINT16)(rnd()%4);
        g_stCellInfoReport.u16IDischg = (UINT16)(rnd()%4);
        if(n%31==0) clear_records();
        if(n%97==0) {
            g_stCellInfoReport.unMdlFault_Second.all = (UINT16)rnd();
            g_stCellInfoReport.unMdlFault_Third.all = (UINT16)rnd();
        }
        flags(n%3 ? 1u << (n%5) : rnd()%32); step(-1);
    }
    assert(callbacks > 0); assert(fclose(trace) == 0);
    business_tests();
    printf("frames=%u callbacks=%u counters=%u\n", frames, callbacks, counterCount);
    return 0;
}
'''


def mapping_tests(old):
    code = 'static void check_output_mapping(void) {\n'
    code += ' union MDLCHGFAULT_REG fault; union FAULT_FLAG_SECOND second; union FAULT_FLAG_THIRD third;\n'
    for i, row in enumerate(rules(old)):
        body = function(old, row['name'])
        event = re.search(r'FaultWarnRecord\((\w+)\)', body).group(1)
        record = 'second' if row['level'] == 'Second' else 'third'
        code += f' fault.all=0; fault.bits.{row["bit"]}=1;\n'
        code += f' assert(fault.all == protectionOutputs[{i//2}].faultMask);\n'
        code += f' {record}.all=0; {record}.bits.{event}=1;\n'
        code += f' assert({record}.all == protectionOutputs[{i//2}].recordMask[{i%2}]);\n'
        code += f' assert({event} == protectionOutputs[{i//2}].event[{i%2}]);\n'
    return code + '}\n'


BUSINESS_TESTS = r'''
/* Independent requirements, not expectations obtained from the old program. */
static void business_tests(void) {
    ProtectionLimits l = {200, 200, 100, 3, 4, TRIP_ABOVE};
    UINT16 count = 0;
    UINT8 active = 0;
    unsigned savedCallbacks = callbacks;
    int i;
    trace = fopen("business.trace", "wb"); assert(trace);
    check_output_mapping();
    assert(Protection_Evaluate(&l,&count,&active) && count==1 && !active);
    l.sample=199; Protection_Evaluate(&l,&count,&active); assert(count==0 && !active);
    l.sample=200;
    for(i=0;i<3;i++) Protection_Evaluate(&l,&count,&active);
    assert(active==1 && count==0);
    l.sample=100;
    for(i=0;i<3;i++) Protection_Evaluate(&l,&count,&active);
    assert(active==1 && count==3);
    Protection_Evaluate(&l,&count,&active); assert(!active && count==0);
    l.direction=TRIP_BELOW; l.trigger=100; l.recovery=200; l.sample=100;
    for(i=0;i<3;i++) Protection_Evaluate(&l,&count,&active);
    assert(active==1 && count==0);
    l.sample=200;
    for(i=0;i<4;i++) Protection_Evaluate(&l,&count,&active);
    assert(!active && count==0);
    l.trigger=300; count=2;
    assert(!Protection_Evaluate(&l,&count,&active) && count==2 && !active);
    l.trigger=100; l.sample=100; count=65535; l.triggerTicks=65535;
    assert(Protection_Evaluate(&l,&count,&active) && count==0 && !active);
    l.triggerTicks=0;
    assert(Protection_Evaluate(&l,&count,&active) && count==0 && active);

    reset();
    for(i=0;i<3;i++) {
        g_stCellInfoReport.u16Ichg = g_stCellInfoReport.u16IDischg = (UINT16)i;
        assert(Protection_GateOpen(GATE_CHARGE_CURRENT,0) == (i>1));
        assert(Protection_GateOpen(GATE_DISCHARGE_CURRENT,0) == (i>1));
        assert(Protection_GateOpen(GATE_CHARGE_CURRENT,1));
        assert(Protection_GateOpen(GATE_DISCHARGE_CURRENT,1));
    }
    g_stCellInfoReport.u16TempMax=200;
    Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    assert(protectionCounts[PROTECTION_CHARGE_HOT][LEVEL_SECOND]==2);
    g_stCellInfoReport.u16Ichg=0;
    for(i=0;i<10;i++) Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    assert(protectionCounts[PROTECTION_CHARGE_HOT][LEVEL_SECOND]==2);
    assert(!g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp);
    g_stCellInfoReport.u16Ichg=2;
    Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    assert(g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp);
    assert(Fault_Flag_Second.bits.CellChgOTp_Second && FaultPoint_Second2==1);
    assert(Fault_record_Second2[0]==CellChgOTp_Second);
    clear_records();
    Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    assert(g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp);
    assert(FaultPoint_Second2==1 && Fault_record_Second2[0]==CellChgOTp_Second);
    g_stCellInfoReport.u16Ichg=0; g_stCellInfoReport.u16TempMax=100;
    for(i=0;i<3;i++) Protection_Process(PROTECTION_CHARGE_HOT,LEVEL_SECOND);
    assert(!g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp);
    assert(!Fault_Flag_Second.bits.CellChgOTp_Second);

    reset(); g_stCellInfoReport.u16Ichg=220;
    for(i=0;i<3;i++) Protection_Process(PROTECTION_CHARGE_OC,LEVEL_THIRD);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp);
    g_stCellInfoReport.u16Ichg=120;
    for(i=0;i<3002;i++) Protection_Process(PROTECTION_CHARGE_OC,LEVEL_THIRD);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp);
    Protection_Process(PROTECTION_CHARGE_OC,LEVEL_THIRD);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp);

    reset();
    g_stCellInfoReport.unMdlFault_Third.bits.b1Rcved2=1;
    Protection_Publish(PROTECTION_CELL_DELTA,LEVEL_THIRD,1);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1Rcved2);
    assert(FaultPoint_Third2==1 && Fault_record_Third2[0]==VdeltaOvp_Third);
    Protection_Publish(PROTECTION_CELL_DELTA,LEVEL_THIRD,0);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1Rcved2);
    clear_records();
    for(i=0;i<12;i++) Protection_RecordEvent(CellOvp_Third);
    assert(FaultPoint_Third2==2 && Fault_record_Third2[0]==CellOvp_Third);
    assert(fclose(trace)==0);
    callbacks=savedCallbacks;
}
'''


def check_arm_layout(out, old, current):
    """用目标编译器实际生成位域对象，校验协议掩码，避免只依赖主机布局。"""
    entries = rules(old)
    arrays = {
        "layout_fault": ("MDLCHGFAULT_REG", [r['bit'] for r in entries[::2]]),
        "layout_second": ("FAULT_FLAG_SECOND", []),
        "layout_third": ("FAULT_FLAG_THIRD", []),
    }
    for i, r in enumerate(entries):
        event = re.search(r'FaultWarnRecord\((\w+)\)', function(old, r['name'])).group(1)
        arrays['layout_second' if i % 2 == 0 else 'layout_third'][1].append(event)
    code = header()
    for name, (kind, bits) in arrays.items():
        code += f'const union {kind} {name}[] = {{\n'
        code += ',\n'.join(f' {{ .bits = {{ .{bit} = 1 }} }}' for bit in bits)
        code += '\n};\n'
    source, obj = out / 'layout.c', out / 'layout.o'
    source.write_text(code, encoding='utf-8')
    compiler = Path('C:/Keil_v5/ARM/ARMCC/bin/armcc.exe')
    result = subprocess.run([str(compiler), '--c99', '--cpu', 'Cortex-M0', '-O2', '-c', str(source), '-o', str(obj)], cwd=out, capture_output=True)
    (out / 'layout-build.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr.decode(errors='replace'))
    data = obj.read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01'
    elf = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    sections = [struct.unpack_from('<IIIIIIIIII', data, elf[6]+i*elf[11]) for i in range(elf[12])]
    actual = {}
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        strings = sections[section[6]]
        for offset in range(section[4], section[4]+section[5], section[9]):
            sym = struct.unpack_from('<IIIBBH', data, offset)
            start = strings[4] + sym[0]
            name = data[start:data.index(b'\0', start)].decode('ascii', errors='replace')
            if name in arrays:
                storage = sections[sym[5]]
                actual[name] = data[storage[4]+sym[1]:storage[4]+sym[1]+sym[2]]
    masks = re.findall(r'\[PROTECTION_\w+\]\s*=\s*\{\s*(0x\w+),\s*\{\s*(0x\w+),\s*(0x\w+)', current)
    assert len(masks) == 13 and len(actual) == 3
    for i, name in enumerate(arrays):
        expected = struct.pack('<13H', *(int(row[i],16) for row in masks))
        assert actual[name] == expected, f'ARM 目标位域映射不一致：{name}'
    return {'status': 'PASS', 'compiler': str(compiler), 'fault_masks': 13, 'record_masks': 26}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    parent = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/030-TI"
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="warn-equivalence-", dir=parent))
    baseline_bytes = subprocess.check_output(["git", "show", f"{BASELINE}:Code/Source/Fault.c"], cwd=ROOT)
    current_bytes = (ROOT / "Code/Source/Fault.c").read_bytes()
    old = clean(baseline_bytes)
    layout = check_arm_layout(out, old, clean(current_bytes))
    pub = function(clean((ROOT / "Code/Source/PubFunc.c").read_bytes()), "App_PubOPUPChk")
    pub = pub.replace("App_PubOPUPChk", "Real_PubOPUPChk", 1)
    common = header()
    harness = driver(old) + "\n" + pub + "\n" + TRACE
    vcvars = Path(r"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
    if not vcvars.exists():
        raise SystemExit("未找到 Visual Studio 2022 C++ vcvars64.bat")
    results = {}
    for label, source in (("baseline", old), ("refactored", clean(current_bytes))):
        source = source.replace('#include "main.h"', '')
        adapter = ""
        if label == "baseline":
            # Only lift old static counters into a test array; expressions and
            # executed statements stay unchanged. No production source edits.
            for i, row in enumerate(rules(old)):
                body = function(source, row['name'])
                lifted = re.sub(r'static UINT16 s_i16TimeCnt = 0;', '', body)
                lifted = re.sub(r'\bs_i16TimeCnt\b', f'baselineCounts[{i}]', lifted)
                source = source.replace(body, lifted)
            source = 'static UINT16 baselineCounts[26];\n' + source
            counts = 'baselineCounts[i]'
            business = "static void business_tests(void) {}\n"
        else:
            # These legacy-name adapters exist only in the host harness. The
            # firmware has no generated/per-protection wrapper functions.
            for i, row in enumerate(rules(old)):
                adapter += f"static void {row['name']}(void) {{ if (Protection_SlotReady(protectionRules[{i//2}].slot)) Protection_Process((ProtectionId){i//2}, {i%2}); }}\n"
            counts = 'protectionCounts[i/2][i%2]'
            business = mapping_tests(old) + BUSINESS_TESTS
        register = f'static void register_counters(void) {{ unsigned i; counterCount=26; for(i=0;i<26;i++) counters[i]=&{counts}; }}\n'
        (out / f"{label}.c").write_text(common + source + adapter + harness + register + business + TEST_MAIN, encoding="utf-8")
        batch = out / f"{label}.cmd"
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /std:c11 /W3 /O2 {label}.c /Fe:{label}.exe\nexit /b %errorlevel%\n', encoding="utf-8")
        build = subprocess.run(["cmd", "/c", str(batch)], cwd=out, capture_output=True)
        (out / f"{label}-build.log").write_bytes(build.stdout + build.stderr)
        if build.returncode:
            raise SystemExit(build.stdout.decode("gbk", errors="replace") + str(out))
        result = subprocess.check_output([str(out / f"{label}.exe"), str(out / f"{label}.trace")], cwd=out, text=True).strip()
        results[label] = result
    # Streaming exact comparison: no checksum-only assertion and bounded memory.
    digest = hashlib.sha256()
    size = 0
    with (out / "baseline.trace").open("rb") as a, (out / "refactored.trace").open("rb") as b:
        while True:
            left, right = a.read(1024*1024), b.read(1024*1024)
            if left != right:
                raise AssertionError(f"保护行为不一致，偏移块 {size}，轨迹目录 {out}")
            if not left:
                break
            digest.update(left)
            size += len(left)
    assert results["baseline"] == results["refactored"]
    report = dict(status="PASS", baseline_ref=BASELINE,
                  baseline_source_sha256=hashlib.sha256(baseline_bytes).hexdigest(),
                  refactored_source_sha256=hashlib.sha256(current_bytes).hexdigest(),
                  pubfunc_sha256=hashlib.sha256((ROOT / 'Code/Source/PubFunc.c').read_bytes()).hexdigest(),
                  results=results, compared_bytes=size, trace_sha256=digest.hexdigest(),
                  business_tests="PASS: threshold, recovery, counter decay/pause/wrap, gate, record replay, callback, 26 protocol mappings",
                  arm_bitfield_layout=layout,
                  temp_directory=str(out), limitation="主机逻辑等价测试，不证明板上执行耗时或电气时序。")
    text = json.dumps(report, indent=2, ensure_ascii=False)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
