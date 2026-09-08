#include "main.h"

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_First[Record_len];
UINT16 Fault_record_Second[Record_len];
UINT16 Fault_record_Third[Record_len];
UINT16 RTC_Fault_record_Third[Record_len][6];

UINT16 Fault_record_First2[Record_len];
UINT16 Fault_record_Second2[Record_len];
UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_First;
UINT8 FaultPoint_Second;
UINT8 FaultPoint_Third;

UINT8 FaultPoint_First2;
UINT8 FaultPoint_Second2;
UINT8 FaultPoint_Third2;

UINT16 FaultCnt_StartUp_First = 0;
UINT16 FaultCnt_StartUp_Second = 0;
UINT16 FaultCnt_StartUp_Third = 0;

/* 电流单位为 0.1 A；严格大于 1 才允许产生新的温度故障。 */
#define OTP_UTP_VirCur_Chg 1
#define OTP_UTP_VirCur_Dsg 1

void FaultWarnRecord(enum FaultFlag num);
void FaultWarnRecord2(enum FaultFlag num);

/* 原 _hiccup_mode 分支引用了已注释的计数器，原工程也无法启用。
 * 明确拒绝该未实现配置，避免重构后悄悄改变它的含义。 */
#ifdef _hiccup_mode
#error _hiccup_mode requires a separately specified and tested recovery policy
#endif

/* 已产生故障时，温度恢复不受电流门限限制；门控关闭时保留滤波计数。 */
#define WARN_GATE_ALWAYS(flag)       1
#define WARN_GATE_CHARGE(flag)       ((flag) == 1 || g_stCellInfoReport.u16Ichg > OTP_UTP_VirCur_Chg)
#define WARN_GATE_DISCHARGE(flag)    ((flag) == 1 || g_stCellInfoReport.u16IDischg > OTP_UTP_VirCur_Dsg)

#define WARN_RAISE_NONE()            ((void)0)
#define WARN_CLEAR_NONE()            ((void)0)
#define WARN_RAISE_DELTA()           System_ERROR_UserCallback(ERROR_VDEATLE_OVER)
#define WARN_CLEAR_DELTA()           System_ERROR_UserCallback(ERROR_REMOVE_VDEATLE_OVER)

/* 唯一的保护流程模板。使用编译期展开，保留直接位域访问和独立静态计数，
 * 无运行时配置表、函数指针或位号推算。参数每次执行时读取，支持在线修改。
 * App_PubOPUPChk 保持原样：条件不满足时计数减一，返回 1 表示参数有效，
 * 并不表示故障翻转。因此每次有效检查都同步记录锁存，兼容通信清记录。
 *
 * 配置参数按以下四组填写：
 *   函数 / 时隙 / 故障级别 / 状态位 / 记录枚举；
 *   采样字段 / 上阈值 / 下阈值；
 *   滤波参数 / 下阈值额外延时 / 逻辑方向 / 电流门控；
 *   时隙统计动作 / 特殊回调策略。
 * 上、下阈值是数值大小关系；欠压/低温的触发阈值在下侧。
 */
#define WARN_DEFINE(name, slot, level, bit, event, sample, high, low, filter, extra, logic, gate, tick, hook) \
void name(void) \
{ \
    static UINT16 timeCount = 0; \
    SPUBOPUPCHK check; \
    if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag##slot) \
    { \
        tick; \
        check.u16ChkVal = g_stCellInfoReport.sample; \
        check.u16OPValB = PRT_E2ROMParas.high; \
        check.u16OPValS = PRT_E2ROMParas.low; \
        check.i16ChkCnt = &timeCount; \
        check.u16TimeCntB = PRT_E2ROMParas.filter; \
        check.u16TimeCntS = PRT_E2ROMParas.filter + (extra); \
        check.u8FlagLogic = logic; \
        check.u8FlagBit = g_stCellInfoReport.unMdlFault_##level.bits.bit; \
        if (WARN_GATE_##gate(check.u8FlagBit) && App_PubOPUPChk(&check)) \
        { \
            g_stCellInfoReport.unMdlFault_##level.bits.bit = check.u8FlagBit; \
            if (check.u8FlagBit == 1) \
            { \
                if (0 == Fault_Flag_##level.bits.event) \
                { \
                    FaultWarnRecord(event); \
                    FaultWarnRecord2(event); \
                    WARN_RAISE_##hook(); \
                    Fault_Flag_##level.bits.event = 1; \
                } \
            } \
            if (check.u8FlagBit == 0 && Fault_Flag_##level.bits.event == 1) \
            { \
                WARN_CLEAR_##hook(); \
                Fault_Flag_##level.bits.event = 0; \
            } \
        } \
    } \
}

/* 顺序与原 App_WarnCtrl 完全一致；不得按时隙重新排序。
 * 时隙 1=单体电压，2=总压，3=电流，4=SOC/高温，5=MOS/压差/低温。
 * 正常调度各时隙错开 2 ms，每项每 10 ms 检查一次。
 */
#define WARN_RULES(X) \
    X(App_CellOvp_SecondCheck, 1, Second, b1CellOvp, CellOvp_Second, \
      u16VCellMax, u16VcellOvp_Second, u16VcellOvp_First, \
      u16VcellOvp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_CellOvp_ThirdCheck, 1, Third, b1CellOvp, CellOvp_Third, \
      u16VCellMax, u16VcellOvp_Third, u16VcellOvp_Rcv, \
      u16VcellOvp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_CellUvp_SecondCheck, 1, Second, b1CellUvp, CellUvp_Second, \
      u16VCellMin, u16VcellUvp_First, u16VcellUvp_Second, \
      u16VcellUvp_Filter, 0, 0, ALWAYS, (void)0, NONE) \
    X(App_CellUvp_ThirdCheck, 1, Third, b1CellUvp, CellUvp_Third, \
      u16VCellMin, u16VcellUvp_Rcv, u16VcellUvp_Third, \
      u16VcellUvp_Filter, 0, 0, ALWAYS, ++sys_time.cnt_10ms1, NONE) \
    X(App_BatOvp_SecondCheck, 2, Second, b1BatOvp, BatOvp_Second, \
      u16VCellTotle, u16VbusOvp_Second, u16VbusOvp_First, \
      u16VbusOvp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_BatOvp_ThirdCheck, 2, Third, b1BatOvp, BatOvp_Third, \
      u16VCellTotle, u16VbusOvp_Third, u16VbusOvp_Rcv, \
      u16VbusOvp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_BatUvp_SecondCheck, 2, Second, b1BatUvp, BatUvp_Second, \
      u16VCellTotle, u16VbusUvp_First, u16VbusUvp_Second, \
      u16VbusUvp_Filter, 0, 0, ALWAYS, (void)0, NONE) \
    X(App_BatUvp_ThirdCheck, 2, Third, b1BatUvp, BatUvp_Third, \
      u16VCellTotle, u16VbusUvp_Rcv, u16VbusUvp_Third, \
      u16VbusUvp_Filter, 0, 0, ALWAYS, ++sys_time.cnt_10ms2, NONE) \
    X(App_MosOtp_SecondCheck, 5, Second, b1TmosOtp, MosOTp_Second, \
      u16Temperature[MOS_TEMP1], u16TmosOTp_Second, u16TmosOTp_First, \
      u16TmosOTp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_MosOtp_ThirdCheck, 5, Third, b1TmosOtp, MosOTp_Third, \
      u16Temperature[MOS_TEMP1], u16TmosOTp_Third, u16TmosOTp_Rcv, \
      u16TmosOTp_Filter, 0, 1, ALWAYS, (void)0, NONE) \
    X(App_VdeltaOp_SecondCheck, 5, Second, b1VcellDeltaBig, VdeltaOvp_Second, \
      u16VCellDelta, u16VdeltaOvp_Second, u16VdeltaOvp_First, \
      u16VdeltaOvp_Filter, 0, 1, ALWAYS, ++sys_time.cnt_10ms5, NONE) \
    X(App_VdeltaOp_ThirdCheck, 5, Third, b1VcellDeltaBig, VdeltaOvp_Third, \
      u16VCellDelta, u16VdeltaOvp_Third, u16VdeltaOvp_Rcv, \
      u16VdeltaOvp_Filter, 0, 1, ALWAYS, (void)0, DELTA) \
    X(App_IdischgOcp_SecondCheck, 3, Second, b1IdischgOcp, IdischgOcp_Second, \
      u16IDischg, u16IdsgOcp_Second, u16IdsgOcp_First, \
      u16IdsgOcp_Filter, CurOverFaultDelay, 1, ALWAYS, (void)0, NONE) \
    X(App_IdischgOcp_ThirdCheck, 3, Third, b1IdischgOcp, IdischgOcp_Third, \
      u16IDischg, u16IdsgOcp_Third, u16IdsgOcp_Rcv, \
      u16IdsgOcp_Filter, CurOverFaultDelay, 1, ALWAYS, ++sys_time.cnt_10ms3, NONE) \
    X(App_IchgOcp_SecondCheck, 3, Second, b1IchgOcp, IchgOcp_Second, \
      u16Ichg, u16IchgOcp_Second, u16IchgOcp_First, \
      u16IchgOcp_Filter, CurOverFaultDelay, 1, ALWAYS, (void)0, NONE) \
    X(App_IchgOcp_ThirdCheck, 3, Third, b1IchgOcp, IchgOcp_Third, \
      u16Ichg, u16IchgOcp_Third, u16IchgOcp_Rcv, \
      u16IchgOcp_Filter, CurOverFaultDelay, 1, ALWAYS, (void)0, NONE) \
    X(App_CellSocUp_SecondCheck, 4, Second, b1SocLow, CellSocUp_Second, \
      SocElement.u16Soc, u16SocUp_First, u16SocUp_Second, \
      u16SocUp_Filter, 0, 0, ALWAYS, (void)0, NONE) \
    X(App_CellSocUp_ThirdCheck, 4, Third, b1SocLow, CellSocUp_Third, \
      SocElement.u16Soc, u16SocUp_Rcv, u16SocUp_Third, \
      u16SocUp_Filter, 0, 0, ALWAYS, ++sys_time.cnt_10ms4, NONE) \
    X(App_CellDisChgOtp_SecondCheck, 4, Second, b1CellDischgOtp, CellDsgOTp_Second, \
      u16TempMax, u16TdischgOTp_Second, u16TdischgOTp_First, \
      u16TdischgOTp_Filter, 0, 1, DISCHARGE, (void)0, NONE) \
    X(App_CellDisChgOtp_ThirdCheck, 4, Third, b1CellDischgOtp, CellDsgOTp_Third, \
      u16TempMax, u16TdischgOTp_Third, u16TdischgOTp_Rcv, \
      u16TdischgOTp_Filter, 0, 1, DISCHARGE, (void)0, NONE) \
    X(App_CellDischgUtp_SecondCheck, 5, Second, b1CellDischgUtp, CellDsgUTp_Second, \
      u16TempMin, u16TdischgUTp_First, u16TdischgUTp_Second, \
      u16TdischgUTp_Filter, 0, 0, DISCHARGE, (void)0, NONE) \
    X(App_CellDischgUtp_ThirdCheck, 5, Third, b1CellDischgUtp, CellDsgUTp_Third, \
      u16TempMin, u16TdischgUTp_Rcv, u16TdischgUTp_Third, \
      u16TdischgUTp_Filter, 0, 0, DISCHARGE, (void)0, NONE) \
    X(App_CellChgOtp_SecondCheck, 4, Second, b1CellChgOtp, CellChgOTp_Second, \
      u16TempMax, u16TChgOTp_Second, u16TChgOTp_First, \
      u16TChgOTp_Filter, 0, 1, CHARGE, (void)0, NONE) \
    X(App_CellChgOtp_ThirdCheck, 4, Third, b1CellChgOtp, CellChgOTp_Third, \
      u16TempMax, u16TChgOTp_Third, u16TChgOTp_Rcv, \
      u16TChgOTp_Filter, 0, 1, CHARGE, (void)0, NONE) \
    X(App_CellChgUtp_SecondCheck, 5, Second, b1CellChgUtp, CellChgUTp_Second, \
      u16TempMin, u16TchgUTp_First, u16TchgUTp_Second, \
      u16TchgUTp_Filter, 0, 0, CHARGE, (void)0, NONE) \
    X(App_CellChgUtp_ThirdCheck, 5, Third, b1CellChgUtp, CellChgUTp_Third, \
      u16TempMin, u16TchgUTp_Rcv, u16TchgUTp_Third, \
      u16TchgUTp_Filter, 0, 0, CHARGE, (void)0, NONE)

WARN_RULES(WARN_DEFINE)

/* 与采样、MOS 控制的主循环相对位置不变；不引入额外总开关或启动延时。 */
void App_WarnCtrl(void)
{
#define WARN_CALL(name, slot, level, bit, event, sample, high, low, filter, extra, logic, gate, tick, hook) name();
    WARN_RULES(WARN_CALL)
#undef WARN_CALL
}

#undef WARN_RULES
#undef WARN_DEFINE

/* 保留旧接口；原实现已由 #if 0 禁用。实际记录仍由 FaultWarnRecord2 完成。 */
void FaultWarnRecord(enum FaultFlag num)
{
    (void)num;
}

void FaultWarnRecord2(enum FaultFlag num)
{
	if (num >= 1 && num <= 13)
	{
		if (FaultPoint_First2 >= Record_len)
		{
			FaultPoint_First2 = 0;
		}
		Fault_record_First2[FaultPoint_First2++] = num;
	}
	else if (num >= 14 && num <= 26)
	{
		if (FaultPoint_Second2 >= Record_len)
		{
			FaultPoint_Second2 = 0;
		}
		Fault_record_Second2[FaultPoint_Second2++] = num;
	}
	else
	{
		if (FaultPoint_Third2 >= Record_len)
		{
			FaultPoint_Third2 = 0;
		}
		Fault_record_Third2[FaultPoint_Third2++] = num;
	}
}
