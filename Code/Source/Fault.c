#include "main.h"
#include <stddef.h>

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_Third[Record_len];
UINT16 RTC_Fault_record_Third[Record_len][6];

UINT16 Fault_record_First2[Record_len];
UINT16 Fault_record_Second2[Record_len];
UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_Third;

UINT8 FaultPoint_First2;
UINT8 FaultPoint_Second2;
UINT8 FaultPoint_Third2;

UINT16 FaultCnt_StartUp_First = 0;
UINT16 FaultCnt_StartUp_Second = 0;
UINT16 FaultCnt_StartUp_Third = 0;

/* 上方变量属于既有通信/诊断接口。实时状态与记录锁存各只有一份，
 * 引擎不另存 active 标志：通信清记录、启动强置故障都立即生效。 */
#ifdef _hiccup_mode
#error _hiccup_mode requires a separately specified and tested recovery policy
#endif

typedef enum {
    PROTECTION_CELL_OV,
    PROTECTION_CELL_UV,
    PROTECTION_PACK_OV,
    PROTECTION_PACK_UV,
    PROTECTION_MOS_HOT,
    PROTECTION_CELL_DELTA,
    PROTECTION_DISCHARGE_OC,
    PROTECTION_CHARGE_OC,
    PROTECTION_SOC_LOW,
    PROTECTION_DISCHARGE_HOT,
    PROTECTION_DISCHARGE_COLD,
    PROTECTION_CHARGE_HOT,
    PROTECTION_CHARGE_COLD,
    PROTECTION_COUNT
} ProtectionId;

enum { LEVEL_SECOND, LEVEL_THIRD, LEVEL_COUNT };
enum { TRIP_BELOW, TRIP_ABOVE };
enum { GATE_ALWAYS, GATE_CHARGE_CURRENT, GATE_DISCHARGE_CURRENT };

typedef struct {
    const UINT16 *sample;
    const UINT16 *trigger[LEVEL_COUNT];
    const UINT16 *recovery[LEVEL_COUNT];
    const UINT16 *filterTicks;
    UINT16 recoveryExtraTicks;
    UINT8 direction;
    UINT8 gate;
    UINT8 slot;
    UINT16 *slotCounter;
    UINT8 countAtLevel;
} ProtectionRule;

/* 一类保护只有一份规则，两级阈值直接引用 EEPROM 参数，支持在线修改。
 * 计数单位是该项的检查周期（正常 10 ms）；恢复附加时间仅用于过流。
 * 数组顺序保留历史检查顺序，不按时隙重新排序。 */
static const ProtectionRule protectionRules[PROTECTION_COUNT] = {
    /* 单体过压 */
    [PROTECTION_CELL_OV] = {
        .sample = &g_stCellInfoReport.u16VCellMax,
        .trigger = { &PRT_E2ROMParas.u16VcellOvp_Second, &PRT_E2ROMParas.u16VcellOvp_Third },
        .recovery = { &PRT_E2ROMParas.u16VcellOvp_First, &PRT_E2ROMParas.u16VcellOvp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16VcellOvp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 1, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 单体欠压 */
    [PROTECTION_CELL_UV] = {
        .sample = &g_stCellInfoReport.u16VCellMin,
        .trigger = { &PRT_E2ROMParas.u16VcellUvp_Second, &PRT_E2ROMParas.u16VcellUvp_Third },
        .recovery = { &PRT_E2ROMParas.u16VcellUvp_First, &PRT_E2ROMParas.u16VcellUvp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16VcellUvp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_BELOW,
        .gate = GATE_ALWAYS, .slot = 1, .slotCounter = &sys_time.cnt_10ms1, .countAtLevel = LEVEL_THIRD
    },
    /* 总压过压 */
    [PROTECTION_PACK_OV] = {
        .sample = &g_stCellInfoReport.u16VCellTotle,
        .trigger = { &PRT_E2ROMParas.u16VbusOvp_Second, &PRT_E2ROMParas.u16VbusOvp_Third },
        .recovery = { &PRT_E2ROMParas.u16VbusOvp_First, &PRT_E2ROMParas.u16VbusOvp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16VbusOvp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 2, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 总压欠压 */
    [PROTECTION_PACK_UV] = {
        .sample = &g_stCellInfoReport.u16VCellTotle,
        .trigger = { &PRT_E2ROMParas.u16VbusUvp_Second, &PRT_E2ROMParas.u16VbusUvp_Third },
        .recovery = { &PRT_E2ROMParas.u16VbusUvp_First, &PRT_E2ROMParas.u16VbusUvp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16VbusUvp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_BELOW,
        .gate = GATE_ALWAYS, .slot = 2, .slotCounter = &sys_time.cnt_10ms2, .countAtLevel = LEVEL_THIRD
    },
    /* MOS 过温 */
    [PROTECTION_MOS_HOT] = {
        .sample = &g_stCellInfoReport.u16Temperature[MOS_TEMP1],
        .trigger = { &PRT_E2ROMParas.u16TmosOTp_Second, &PRT_E2ROMParas.u16TmosOTp_Third },
        .recovery = { &PRT_E2ROMParas.u16TmosOTp_First, &PRT_E2ROMParas.u16TmosOTp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16TmosOTp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 5, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 单体压差 */
    [PROTECTION_CELL_DELTA] = {
        .sample = &g_stCellInfoReport.u16VCellDelta,
        .trigger = { &PRT_E2ROMParas.u16VdeltaOvp_Second, &PRT_E2ROMParas.u16VdeltaOvp_Third },
        .recovery = { &PRT_E2ROMParas.u16VdeltaOvp_First, &PRT_E2ROMParas.u16VdeltaOvp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16VdeltaOvp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 5, .slotCounter = &sys_time.cnt_10ms5, .countAtLevel = LEVEL_SECOND
    },
    /* 放电过流 */
    [PROTECTION_DISCHARGE_OC] = {
        .sample = &g_stCellInfoReport.u16IDischg,
        .trigger = { &PRT_E2ROMParas.u16IdsgOcp_Second, &PRT_E2ROMParas.u16IdsgOcp_Third },
        .recovery = { &PRT_E2ROMParas.u16IdsgOcp_First, &PRT_E2ROMParas.u16IdsgOcp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16IdsgOcp_Filter,
        .recoveryExtraTicks = CurOverFaultDelay, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 3, .slotCounter = &sys_time.cnt_10ms3, .countAtLevel = LEVEL_THIRD
    },
    /* 充电过流 */
    [PROTECTION_CHARGE_OC] = {
        .sample = &g_stCellInfoReport.u16Ichg,
        .trigger = { &PRT_E2ROMParas.u16IchgOcp_Second, &PRT_E2ROMParas.u16IchgOcp_Third },
        .recovery = { &PRT_E2ROMParas.u16IchgOcp_First, &PRT_E2ROMParas.u16IchgOcp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16IchgOcp_Filter,
        .recoveryExtraTicks = CurOverFaultDelay, .direction = TRIP_ABOVE,
        .gate = GATE_ALWAYS, .slot = 3, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* SOC 过低 */
    [PROTECTION_SOC_LOW] = {
        .sample = &g_stCellInfoReport.SocElement.u16Soc,
        .trigger = { &PRT_E2ROMParas.u16SocUp_Second, &PRT_E2ROMParas.u16SocUp_Third },
        .recovery = { &PRT_E2ROMParas.u16SocUp_First, &PRT_E2ROMParas.u16SocUp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16SocUp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_BELOW,
        .gate = GATE_ALWAYS, .slot = 4, .slotCounter = &sys_time.cnt_10ms4, .countAtLevel = LEVEL_THIRD
    },
    /* 放电过温 */
    [PROTECTION_DISCHARGE_HOT] = {
        .sample = &g_stCellInfoReport.u16TempMax,
        .trigger = { &PRT_E2ROMParas.u16TdischgOTp_Second, &PRT_E2ROMParas.u16TdischgOTp_Third },
        .recovery = { &PRT_E2ROMParas.u16TdischgOTp_First, &PRT_E2ROMParas.u16TdischgOTp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16TdischgOTp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_DISCHARGE_CURRENT, .slot = 4, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 放电低温 */
    [PROTECTION_DISCHARGE_COLD] = {
        .sample = &g_stCellInfoReport.u16TempMin,
        .trigger = { &PRT_E2ROMParas.u16TdischgUTp_Second, &PRT_E2ROMParas.u16TdischgUTp_Third },
        .recovery = { &PRT_E2ROMParas.u16TdischgUTp_First, &PRT_E2ROMParas.u16TdischgUTp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16TdischgUTp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_BELOW,
        .gate = GATE_DISCHARGE_CURRENT, .slot = 5, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 充电过温 */
    [PROTECTION_CHARGE_HOT] = {
        .sample = &g_stCellInfoReport.u16TempMax,
        .trigger = { &PRT_E2ROMParas.u16TChgOTp_Second, &PRT_E2ROMParas.u16TChgOTp_Third },
        .recovery = { &PRT_E2ROMParas.u16TChgOTp_First, &PRT_E2ROMParas.u16TChgOTp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16TChgOTp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_ABOVE,
        .gate = GATE_CHARGE_CURRENT, .slot = 4, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
    /* 充电低温 */
    [PROTECTION_CHARGE_COLD] = {
        .sample = &g_stCellInfoReport.u16TempMin,
        .trigger = { &PRT_E2ROMParas.u16TchgUTp_Second, &PRT_E2ROMParas.u16TchgUTp_Third },
        .recovery = { &PRT_E2ROMParas.u16TchgUTp_First, &PRT_E2ROMParas.u16TchgUTp_Rcv },
        .filterTicks = &PRT_E2ROMParas.u16TchgUTp_Filter,
        .recoveryExtraTicks = 0, .direction = TRIP_BELOW,
        .gate = GATE_CHARGE_CURRENT, .slot = 5, .slotCounter = NULL, .countAtLevel = LEVEL_SECOND
    },
};

/* 唯一的运行计数，按“保护类型/级别”索引；无 26 个独立函数局部变量。 */
static UINT16 protectionCounts[PROTECTION_COUNT][LEVEL_COUNT];

typedef struct {
    UINT16 faultMask;
    UINT16 recordMask[LEVEL_COUNT];
    enum FaultFlag event[LEVEL_COUNT];
} ProtectionOutput;

/* 兼容层：明确列出协议状态位、二/三级记录锁存位和记录编号。
 * 不从枚举值推算位号，特别注意充电低温/放电高温的两级记录位不同。
 * 仅本层了解现有 .all 的布局；位映射由真实头文件的位域测试校验。 */
static const ProtectionOutput protectionOutputs[PROTECTION_COUNT] = {
    [PROTECTION_CELL_OV] = { 0x0001, { 0x0001, 0x0001 }, { CellOvp_Second, CellOvp_Third } },
    [PROTECTION_CELL_UV] = { 0x0002, { 0x0002, 0x0002 }, { CellUvp_Second, CellUvp_Third } },
    [PROTECTION_PACK_OV] = { 0x0004, { 0x0004, 0x0004 }, { BatOvp_Second, BatOvp_Third } },
    [PROTECTION_PACK_UV] = { 0x0008, { 0x0008, 0x0008 }, { BatUvp_Second, BatUvp_Third } },
    [PROTECTION_MOS_HOT] = { 0x2000, { 0x0400, 0x0400 }, { MosOTp_Second, MosOTp_Third } },
    [PROTECTION_CELL_DELTA] = { 0x0400, { 0x0800, 0x0800 }, { VdeltaOvp_Second, VdeltaOvp_Third } },
    [PROTECTION_DISCHARGE_OC] = { 0x0020, { 0x0020, 0x0020 }, { IdischgOcp_Second, IdischgOcp_Third } },
    [PROTECTION_CHARGE_OC] = { 0x0010, { 0x0010, 0x0010 }, { IchgOcp_Second, IchgOcp_Third } },
    [PROTECTION_SOC_LOW] = { 0x1000, { 0x1000, 0x1000 }, { CellSocUp_Second, CellSocUp_Third } },
    [PROTECTION_DISCHARGE_HOT] = { 0x0080, { 0x0080, 0x0100 }, { CellDsgOTp_Second, CellDsgOTp_Third } },
    [PROTECTION_DISCHARGE_COLD] = { 0x0200, { 0x0200, 0x0200 }, { CellDsgUTp_Second, CellDsgUTp_Third } },
    [PROTECTION_CHARGE_HOT] = { 0x0040, { 0x0040, 0x0040 }, { CellChgOTp_Second, CellChgOTp_Third } },
    [PROTECTION_CHARGE_COLD] = { 0x0100, { 0x0100, 0x0080 }, { CellChgUTp_Second, CellChgUTp_Third } },
};

typedef struct {
    UINT16 sample;
    UINT16 trigger;
    UINT16 recovery;
    UINT16 triggerTicks;
    UINT16 recoveryTicks;
    UINT8 direction;
} ProtectionLimits;

/* 纯判断函数：只操作传入的计数和状态，不读取协议、EEPROM 或全局变量。
 * 返回 0 表示阈值无效，此时完全不改变状态；返回 1 表示完成一次有效检查。
 * 保留积分式滤波：命中加一，未命中减一；翻转后清零，而非连续超限计时。 */
static UINT8 Protection_Evaluate(const ProtectionLimits *limits, UINT16 *count, UINT8 *active)
{
    UINT8 rising;
    UINT8 reached;
    UINT16 threshold;
    UINT16 ticks;

    if (limits->direction > TRIP_ABOVE ||
        (limits->direction == TRIP_ABOVE && limits->trigger < limits->recovery) ||
        (limits->direction == TRIP_BELOW && limits->trigger > limits->recovery))
        return 0;

    threshold = *active ? limits->recovery : limits->trigger;
    ticks = *active ? limits->recoveryTicks : limits->triggerTicks;
    rising = *active ? !limits->direction : limits->direction;
    reached = rising ? limits->sample >= threshold : limits->sample <= threshold;
    if (reached)
    {
        *count = (UINT16)(*count + 1); /* 保留原 16 位边界行为。 */
        if (*count >= ticks)
        {
            *count = 0;
            *active = !*active;
        }
    }
    else if (*count > 0)
        --*count;
    return 1;
}

static UINT8 Protection_SlotReady(UINT8 slot)
{
    switch (slot)
    {
    case 1: return g_st_SysTimeFlag.bits.b1Sys10msFlag1;
    case 2: return g_st_SysTimeFlag.bits.b1Sys10msFlag2;
    case 3: return g_st_SysTimeFlag.bits.b1Sys10msFlag3;
    case 4: return g_st_SysTimeFlag.bits.b1Sys10msFlag4;
    case 5: return g_st_SysTimeFlag.bits.b1Sys10msFlag5;
    default: return 0;
    }
}

static UINT8 Protection_GateOpen(UINT8 gate, UINT8 active)
{
    if (active) return 1; /* 已产生温度故障时，无电流也必须允许恢复。 */
    switch (gate)
    {
    case GATE_CHARGE_CURRENT: return g_stCellInfoReport.u16Ichg > 1;
    case GATE_DISCHARGE_CURRENT: return g_stCellInfoReport.u16IDischg > 1;
    default: return 1;
    }
}

/* 环形记录的写入只实现一次。保留编号分段和回绕方式。 */
static void Protection_RecordEvent(enum FaultFlag event)
{
    UINT16 *records;
    UINT8 *position;
    if (event >= 1 && event <= 13)
    {
        records = Fault_record_First2;
        position = &FaultPoint_First2;
    }
    else if (event >= 14 && event <= 26)
    {
        records = Fault_record_Second2;
        position = &FaultPoint_Second2;
    }
    else
    {
        records = Fault_record_Third2;
        position = &FaultPoint_Third2;
    }
    if (*position >= Record_len) *position = 0;
    records[(*position)++] = (UINT16)event;
}

/* 每项检查后立即发布，不积攒到循环结束：保留事件之间的可见顺序。
 * 实时故障与记录锁存职责不同，不能合并：清记录不清保护状态。 */
static void Protection_Publish(ProtectionId id, UINT8 level, UINT8 active)
{
    const ProtectionOutput *output = &protectionOutputs[id];
    UINT16 *faultWord = level == LEVEL_SECOND ?
        &g_stCellInfoReport.unMdlFault_Second.all : &g_stCellInfoReport.unMdlFault_Third.all;
    UINT16 *recordWord = level == LEVEL_SECOND ? &Fault_Flag_Second.all : &Fault_Flag_Third.all;
    UINT16 recordMask = output->recordMask[level];

    if (active)
    {
        *faultWord |= output->faultMask;
        if ((*recordWord & recordMask) == 0)
        {
            Protection_RecordEvent(output->event[level]);
            if (id == PROTECTION_CELL_DELTA && level == LEVEL_THIRD)
                System_ERROR_UserCallback(ERROR_VDEATLE_OVER);
            *recordWord |= recordMask;
        }
    }
    else
    {
        *faultWord &= (UINT16)~output->faultMask;
        if (*recordWord & recordMask)
        {
            if (id == PROTECTION_CELL_DELTA && level == LEVEL_THIRD)
                System_ERROR_UserCallback(ERROR_REMOVE_VDEATLE_OVER);
            *recordWord &= (UINT16)~recordMask;
        }
    }
}

static void Protection_Process(ProtectionId id, UINT8 level)
{
    const ProtectionRule *rule = &protectionRules[id];
    const ProtectionOutput *output = &protectionOutputs[id];
    UINT16 faultWord;
    UINT8 active;
    ProtectionLimits limits;

    if (rule->slotCounter != NULL && level == rule->countAtLevel)
        ++*rule->slotCounter;

    faultWord = level == LEVEL_SECOND ?
        g_stCellInfoReport.unMdlFault_Second.all : g_stCellInfoReport.unMdlFault_Third.all;
    active = (faultWord & output->faultMask) != 0;
    if (!Protection_GateOpen(rule->gate, active)) return; /* 暂停而非清零。 */

    limits.sample = *rule->sample;
    limits.trigger = *rule->trigger[level];
    limits.recovery = *rule->recovery[level];
    limits.triggerTicks = *rule->filterTicks;
    limits.recoveryTicks = (UINT16)(*rule->filterTicks + rule->recoveryExtraTicks);
    limits.direction = rule->direction;
    if (Protection_Evaluate(&limits, &protectionCounts[id][level], &active))
        Protection_Publish(id, level, active);
}

/* 主循环仍在采样之后、MOS 控制之前调用；各项按原顺序先二级再三级。 */
void App_WarnCtrl(void)
{
    ProtectionId id;
    /* 主循环大部分轮次没有到期项，先跳过整次调度。 */
    if (!(g_st_SysTimeFlag.bits.b1Sys10msFlag1 ||
          g_st_SysTimeFlag.bits.b1Sys10msFlag2 ||
          g_st_SysTimeFlag.bits.b1Sys10msFlag3 ||
          g_st_SysTimeFlag.bits.b1Sys10msFlag4 ||
          g_st_SysTimeFlag.bits.b1Sys10msFlag5)) return;
    for (id = PROTECTION_CELL_OV; id < PROTECTION_COUNT; ++id)
    {
        if (!Protection_SlotReady(protectionRules[id].slot)) continue;
        Protection_Process(id, LEVEL_SECOND);
        Protection_Process(id, LEVEL_THIRD);
    }
}
