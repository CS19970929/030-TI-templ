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

// Ô­À´ÊÇºÍĞİÃßĞéµçÂ·¹Ò¹³µÄ£¬ÏÖÔÚ·Ö¿ª£¬OtherElement.u16Sleep_VirCur_Chg
// Èç¹ûÔ­À´ÉèÖÃ3AµÄ»°£¬¾Í»á³öÏÖ£¬µÍÎÂĞ¡µçÁ÷£¬ÎÊÌâºÜ´ó¡£
// ÏÖÔÚÄ¬ÈÏĞéµçÁ÷´óÓÚ0.1A£¬Ò²¼´0.2A²ÅÉúĞ§
#define OTP_UTP_VirCur_Chg 1
#define OTP_UTP_VirCur_Dsg 1

void FaultWarnRecord(enum FaultFlag num);
void FaultWarnRecord2(enum FaultFlag num);
void PwrMag_Protect_Record(enum FaultFlag num);
void PwrMag_Protect_Record_StartUp(void);

void App_CellOvp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VcellOvp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VcellOvp_First; // DELAYB10MS_500MS¸ÄÎªfilter
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VcellOvp_Filter;				   // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VcellOvp_Filter;				   // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												   // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellOvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1CellOvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.CellOvp_Second)
				{
					FaultWarnRecord(CellOvp_Second);
					FaultWarnRecord2(CellOvp_Second);
					Fault_Flag_Second.bits.CellOvp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellOvp_Second == 1)
			{
				Fault_Flag_Second.bits.CellOvp_Second = 0;
			}
		}
	}
}

void App_CellOvp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VcellOvp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VcellOvp_Rcv; // DELAYB10MS_500MS¸ÄÎªfilter
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VcellOvp_Filter;				  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VcellOvp_Filter;				  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.CellOvp_Third)
				{
					FaultWarnRecord(CellOvp_Third);
					FaultWarnRecord2(CellOvp_Third);
					Fault_Flag_Third.bits.CellOvp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellOvp_Third == 1)
			{
				Fault_Flag_Third.bits.CellOvp_Third = 0;
			}
		}
	}
}

void App_CellUvp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VcellUvp_First;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VcellUvp_Second;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VcellUvp_Filter;				   // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VcellUvp_Filter;				   // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;												   // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellUvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1CellUvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.CellUvp_Second)
				{
					FaultWarnRecord(CellUvp_Second);
					FaultWarnRecord2(CellUvp_Second);
					Fault_Flag_Second.bits.CellUvp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellUvp_Second == 1)
			{
				Fault_Flag_Second.bits.CellUvp_Second = 0;
			}
		}
	}
}

void App_CellUvp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VcellUvp_Rcv;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VcellUvp_Third;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VcellUvp_Filter;
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VcellUvp_Filter;
		t_sPubOPUPChk.u8FlagLogic = 0;												  // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.CellUvp_Third)
				{
					FaultWarnRecord(CellUvp_Third);
					FaultWarnRecord2(CellUvp_Third);
					Fault_Flag_Third.bits.CellUvp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellUvp_Third == 1)
			{
				Fault_Flag_Third.bits.CellUvp_Third = 0;
			}
		}
	}
}

void App_BatOvp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag2)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellTotle;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VbusOvp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VbusOvp_First;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VbusOvp_Filter;				  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VbusOvp_Filter;				  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1BatOvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1BatOvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.BatOvp_Second)
				{
					FaultWarnRecord(BatOvp_Second);
					FaultWarnRecord2(BatOvp_Second);
					Fault_Flag_Second.bits.BatOvp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.BatOvp_Second == 1)
			{
				Fault_Flag_Second.bits.BatOvp_Second = 0;
			}
		}
	}
}

void App_BatOvp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag2)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellTotle;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VbusOvp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VbusOvp_Rcv;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VbusOvp_Filter;				 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VbusOvp_Filter;				 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												 // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.BatOvp_Third)
				{
					FaultWarnRecord(BatOvp_Third);
					FaultWarnRecord2(BatOvp_Third);
					Fault_Flag_Third.bits.BatOvp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.BatOvp_Third == 1)
			{
				Fault_Flag_Third.bits.BatOvp_Third = 0;
			}
		}
	}
}

void App_BatUvp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag2)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellTotle;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VbusUvp_First;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VbusUvp_Second;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VbusUvp_Filter;				  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VbusUvp_Filter;				  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;												  // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1BatUvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1BatUvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.BatUvp_Second)
				{
					FaultWarnRecord(BatUvp_Second);
					FaultWarnRecord2(BatUvp_Second);
					Fault_Flag_Second.bits.BatUvp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.BatUvp_Second == 1)
			{
				Fault_Flag_Second.bits.BatUvp_Second = 0;
			}
		}
	}
}

void App_BatUvp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag2)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellTotle;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VbusUvp_Rcv;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VbusUvp_Third;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VbusUvp_Filter;				 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VbusUvp_Filter;				 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;												 // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.BatUvp_Third)
				{
					FaultWarnRecord(BatUvp_Third);
					FaultWarnRecord2(BatUvp_Third);
					Fault_Flag_Third.bits.BatUvp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.BatUvp_Third == 1)
			{
				Fault_Flag_Third.bits.BatUvp_Third = 0;
			}
		}
	}
}

void App_IchgOcp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	// static UINT16 s_i16TimeCntClr = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag3)
	{
		// if(0 == g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp) {
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16Ichg;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16IchgOcp_Second; // ¹ıÁ÷ÅĞ¶Ï
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16IchgOcp_First;	// Ã»ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16IchgOcp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16IchgOcp_Filter + CurOverFaultDelay; // Ã»ÓĞ»Ö¸´ÅĞ¶Ï-->ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.u8FlagLogic = 1;													  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp;	  // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.IchgOcp_Second)
				{
					FaultWarnRecord(IchgOcp_Second);
					FaultWarnRecord2(IchgOcp_Second);
					Fault_Flag_Second.bits.IchgOcp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.IchgOcp_Second == 1)
			{
				Fault_Flag_Second.bits.IchgOcp_Second = 0;
			}
		}
//}
#ifdef _hiccup_mode
		else
		{
			if ((++s_i16TimeCntClr) > DELAYB10MS_5S)
			{ // 5SºóÇå±êÖ¾Î»£¬¹ÊÕÏ»Ö¸´£¬¼´´òàÃ´¦Àí
				s_i16TimeCntClr = 0;
				g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 0;
			}
		}
#endif
	}
}

void App_IchgOcp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	// static UINT16 s_i16TimeCntClr = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag3)
	{
		// if(0 == g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp) {
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16Ichg;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16IchgOcp_Third; // ¹ıÁ÷ÅĞ¶Ï
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16IchgOcp_Rcv;   // Ã»ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16IchgOcp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16IchgOcp_Filter + CurOverFaultDelay; // Ã»ÓĞ»Ö¸´ÅĞ¶Ï-->ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.u8FlagLogic = 1;													  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp;	  // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.IchgOcp_Third)
				{
					FaultWarnRecord(IchgOcp_Third);
					FaultWarnRecord2(IchgOcp_Third);
					Fault_Flag_Third.bits.IchgOcp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.IchgOcp_Third == 1)
			{
				Fault_Flag_Third.bits.IchgOcp_Third = 0;
			}
		}
//}
#ifdef _hiccup_mode
		else
		{
			if ((++s_i16TimeCntClr) > DELAYB10MS_5S)
			{ // 5SºóÇå±êÖ¾Î»£¬¹ÊÕÏ»Ö¸´£¬¼´´òàÃ´¦Àí
				s_i16TimeCntClr = 0;
				g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = 0;
			}
		}
#endif
	}
}

void App_IdischgOcp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	// static UINT16 s_i16TimeCntClr = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag3)
	{
		// if(0 == g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp) {
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16IDischg;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16IdsgOcp_Second; // ¹ıÁ÷ÅĞ¶Ï
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16IdsgOcp_First;	// Ã»ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16IdsgOcp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16IdsgOcp_Filter + CurOverFaultDelay; // Ã»ÓĞ»Ö¸´ÅĞ¶Ï-->ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.u8FlagLogic = 1;													  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.IdischgOcp_Second)
				{
					FaultWarnRecord(IdischgOcp_Second);
					FaultWarnRecord2(IdischgOcp_Second);
					Fault_Flag_Second.bits.IdischgOcp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.IdischgOcp_Second == 1)
			{
				Fault_Flag_Second.bits.IdischgOcp_Second = 0;
			}
		}
//}
#ifdef _hiccup_mode
		else
		{
			if ((++s_i16TimeCntClr) > DELAYB10MS_5S)
			{ // 5SºóÇå±êÖ¾Î»£¬¹ÊÕÏ»Ö¸´£¬¼´´òàÃ´¦Àí
				s_i16TimeCntClr = 0;
				g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 0;
			}
		}
#endif
	}
}

void App_IdischgOcp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	// static UINT16 s_i16TimeCntClr = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag3)
	{
		// if(0 == g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp) {
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16IDischg;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16IdsgOcp_Third; // ¹ıÁ÷ÅĞ¶Ï
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16IdsgOcp_Rcv;   // Ã»ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16IdsgOcp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16IdsgOcp_Filter + CurOverFaultDelay; // Ã»ÓĞ»Ö¸´ÅĞ¶Ï-->ÓĞ»Ö¸´ÅĞ¶Ï
		t_sPubOPUPChk.u8FlagLogic = 1;													  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp;  // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.IdischgOcp_Third)
				{
					FaultWarnRecord(IdischgOcp_Third);
					FaultWarnRecord2(IdischgOcp_Third);
					Fault_Flag_Third.bits.IdischgOcp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.IdischgOcp_Third == 1)
			{
				Fault_Flag_Third.bits.IdischgOcp_Third = 0;
			}
		}
//}
#ifdef _hiccup_mode
		else
		{
			if ((++s_i16TimeCntClr) > DELAYB10MS_5S)
			{ // 5SºóÇå±êÖ¾Î»£¬¹ÊÕÏ»Ö¸´£¬¼´´òàÃ´¦Àí
				s_i16TimeCntClr = 0;
				g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = 0;
			}
		}
#endif
	}
}

void App_CellChgOtp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TChgOTp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TChgOTp_First;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TChgOTp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TChgOTp_Filter;					  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;													  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16Ichg > OTP_UTP_VirCur_Chg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Second.bits.CellChgOTp_Second)
						{
							FaultWarnRecord(CellChgOTp_Second);
							FaultWarnRecord2(CellChgOTp_Second);
							Fault_Flag_Second.bits.CellChgOTp_Second = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellChgOTp_Second == 1)
					{
						Fault_Flag_Second.bits.CellChgOTp_Second = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Second.bits.CellChgOTp_Second)
					{
						FaultWarnRecord(CellChgOTp_Second);
						FaultWarnRecord2(CellChgOTp_Second);
						Fault_Flag_Second.bits.CellChgOTp_Second = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellChgOTp_Second == 1)
				{
					Fault_Flag_Second.bits.CellChgOTp_Second = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellChgOtp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TChgOTp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TChgOTp_Rcv;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TChgOTp_Filter;					 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TChgOTp_Filter;					 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;													 // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16Ichg > OTP_UTP_VirCur_Chg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Third.bits.CellChgOTp_Third)
						{
							FaultWarnRecord(CellChgOTp_Third);
							FaultWarnRecord2(CellChgOTp_Third);
							Fault_Flag_Third.bits.CellChgOTp_Third = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellChgOTp_Third == 1)
					{
						Fault_Flag_Third.bits.CellChgOTp_Third = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Third.bits.CellChgOTp_Third)
					{
						FaultWarnRecord(CellChgOTp_Third);
						FaultWarnRecord2(CellChgOTp_Third);
						Fault_Flag_Third.bits.CellChgOTp_Third = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellChgOTp_Third == 1)
				{
					Fault_Flag_Third.bits.CellChgOTp_Third = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellDisChgOtp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TdischgOTp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TdischgOTp_First;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TdischgOTp_Filter;					 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TdischgOTp_Filter;					 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;														 // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16IDischg > OTP_UTP_VirCur_Dsg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Second.bits.CellDsgOTp_Second)
						{
							FaultWarnRecord(CellDsgOTp_Second);
							FaultWarnRecord2(CellDsgOTp_Second);
							Fault_Flag_Second.bits.CellDsgOTp_Second = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellDsgOTp_Second == 1)
					{
						Fault_Flag_Second.bits.CellDsgOTp_Second = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Second.bits.CellDsgOTp_Second)
					{
						FaultWarnRecord(CellDsgOTp_Second);
						FaultWarnRecord2(CellDsgOTp_Second);
						Fault_Flag_Second.bits.CellDsgOTp_Second = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellDsgOTp_Second == 1)
				{
					Fault_Flag_Second.bits.CellDsgOTp_Second = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellDisChgOtp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMax;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TdischgOTp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TdischgOTp_Rcv;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TdischgOTp_Filter;					// ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TdischgOTp_Filter;					// ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;														// ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16IDischg > OTP_UTP_VirCur_Dsg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Third.bits.CellDsgOTp_Third)
						{
							FaultWarnRecord(CellDsgOTp_Third);
							FaultWarnRecord2(CellDsgOTp_Third);
							Fault_Flag_Third.bits.CellDsgOTp_Third = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellDsgOTp_Third == 1)
					{
						Fault_Flag_Third.bits.CellDsgOTp_Third = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Third.bits.CellDsgOTp_Third)
					{
						FaultWarnRecord(CellDsgOTp_Third);
						FaultWarnRecord2(CellDsgOTp_Third);
						Fault_Flag_Third.bits.CellDsgOTp_Third = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellDsgOTp_Third == 1)
				{
					Fault_Flag_Third.bits.CellDsgOTp_Third = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_MosOtp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16Temperature[MOS_TEMP1];
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TmosOTp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TmosOTp_First;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TmosOTp_Filter;				   // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TmosOTp_Filter;				   // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												   // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1TmosOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1TmosOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.MosOTp_Second)
				{
					FaultWarnRecord(MosOTp_Second);
					FaultWarnRecord2(MosOTp_Second);
					Fault_Flag_Second.bits.MosOTp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.MosOTp_Second == 1)
			{
				Fault_Flag_Second.bits.MosOTp_Second = 0;
			}
		}
	}
}

void App_MosOtp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16Temperature[MOS_TEMP1];
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TmosOTp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TmosOTp_Rcv;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TmosOTp_Filter;				  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TmosOTp_Filter;				  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;												  // ÕıÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.MosOTp_Third)
				{
					FaultWarnRecord(MosOTp_Third);
					FaultWarnRecord2(MosOTp_Third);
					Fault_Flag_Third.bits.MosOTp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.MosOTp_Third == 1)
			{
				Fault_Flag_Third.bits.MosOTp_Third = 0;
			}
		}
	}
}

void App_CellChgUtp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TchgUTp_First;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TchgUTp_Second;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TchgUTp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TchgUTp_Filter;					  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;													  // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgUtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16Ichg > OTP_UTP_VirCur_Chg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Second.bits.CellChgUTp_Second)
						{
							FaultWarnRecord(CellChgUTp_Second);
							FaultWarnRecord2(CellChgUTp_Second);
							Fault_Flag_Second.bits.CellChgUTp_Second = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellChgUTp_Second == 1)
					{
						Fault_Flag_Second.bits.CellChgUTp_Second = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Second.bits.CellChgUTp_Second)
					{
						FaultWarnRecord(CellChgUTp_Second);
						FaultWarnRecord2(CellChgUTp_Second);
						Fault_Flag_Second.bits.CellChgUTp_Second = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellChgUTp_Second == 1)
				{
					Fault_Flag_Second.bits.CellChgUTp_Second = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellChgUtp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TchgUTp_Rcv;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TchgUTp_Third;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TchgUTp_Filter;					 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TchgUTp_Filter;					 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;													 // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16Ichg > OTP_UTP_VirCur_Chg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Third.bits.CellChgUTp_Third)
						{
							FaultWarnRecord(CellChgUTp_Third);
							FaultWarnRecord2(CellChgUTp_Third);
							Fault_Flag_Third.bits.CellChgUTp_Third = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellChgUTp_Third == 1)
					{
						Fault_Flag_Third.bits.CellChgUTp_Third = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Third.bits.CellChgUTp_Third)
					{
						FaultWarnRecord(CellChgUTp_Third);
						FaultWarnRecord2(CellChgUTp_Third);
						Fault_Flag_Third.bits.CellChgUTp_Third = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellChgUTp_Third == 1)
				{
					Fault_Flag_Third.bits.CellChgUTp_Third = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellDischgUtp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TdischgUTp_First;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TdischgUTp_Second;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TdischgUTp_Filter;					 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TdischgUTp_Filter;					 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;														 // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgUtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16IDischg > OTP_UTP_VirCur_Dsg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Second.bits.CellDsgUTp_Second)
						{
							FaultWarnRecord(CellDsgUTp_Second);
							FaultWarnRecord2(CellDsgUTp_Second);
							Fault_Flag_Second.bits.CellDsgUTp_Second = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellDsgUTp_Second == 1)
					{
						Fault_Flag_Second.bits.CellDsgUTp_Second = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Second.bits.CellDsgUTp_Second)
					{
						FaultWarnRecord(CellDsgUTp_Second);
						FaultWarnRecord2(CellDsgUTp_Second);
						Fault_Flag_Second.bits.CellDsgUTp_Second = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellDsgUTp_Second == 1)
				{
					Fault_Flag_Second.bits.CellDsgUTp_Second = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellDischgUtp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16TempMin;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16TdischgUTp_Rcv;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16TdischgUTp_Third;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16TdischgUTp_Filter;					// ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16TdischgUTp_Filter;					// ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;														// ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		switch (t_sPubOPUPChk.u8FlagBit)
		{
		case 0:
			if (g_stCellInfoReport.u16IDischg > OTP_UTP_VirCur_Dsg)
			{
				if (App_PubOPUPChk(&t_sPubOPUPChk))
				{
					g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
					if (t_sPubOPUPChk.u8FlagBit == 1)
					{
						if (0 == Fault_Flag_Third.bits.CellDsgUTp_Third)
						{
							FaultWarnRecord(CellDsgUTp_Third);
							FaultWarnRecord2(CellDsgUTp_Third);
							Fault_Flag_Third.bits.CellDsgUTp_Third = 1;
						}
					}
					if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellDsgUTp_Third == 1)
					{
						Fault_Flag_Third.bits.CellDsgUTp_Third = 0;
					}
				}
			}
			break;

		case 1:
			if (App_PubOPUPChk(&t_sPubOPUPChk))
			{
				g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
				if (t_sPubOPUPChk.u8FlagBit == 1)
				{
					if (0 == Fault_Flag_Third.bits.CellDsgUTp_Third)
					{
						FaultWarnRecord(CellDsgUTp_Third);
						FaultWarnRecord2(CellDsgUTp_Third);
						Fault_Flag_Third.bits.CellDsgUTp_Third = 1;
					}
				}
				if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellDsgUTp_Third == 1)
				{
					Fault_Flag_Third.bits.CellDsgUTp_Third = 0;
				}
			}
			break;

		default:
			break;
		}
	}
}

void App_CellSocUp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.SocElement.u16Soc;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16SocUp_First;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16SocUp_Second;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16SocUp_Filter;					  // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16SocUp_Filter;					  // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;												  // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1SocLow; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1SocLow = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.CellSocUp_Second)
				{
					FaultWarnRecord(CellSocUp_Second);
					FaultWarnRecord2(CellSocUp_Second);
					Fault_Flag_Second.bits.CellSocUp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.CellSocUp_Second == 1)
			{
				Fault_Flag_Second.bits.CellSocUp_Second = 0;
			}
		}
	}
}

void App_CellSocUp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag4)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.SocElement.u16Soc;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16SocUp_Rcv;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16SocUp_Third;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16SocUp_Filter;					 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16SocUp_Filter;					 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 0;												 // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.CellSocUp_Third)
				{
					FaultWarnRecord(CellSocUp_Third);
					FaultWarnRecord2(CellSocUp_Third);
					Fault_Flag_Third.bits.CellSocUp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.CellSocUp_Third == 1)
			{
				Fault_Flag_Third.bits.CellSocUp_Third = 0;
			}
		}
	}
}

void App_VdeltaOp_SecondCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellDelta;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VdeltaOvp_Second;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VdeltaOvp_First;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VdeltaOvp_Filter;						 // ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VdeltaOvp_Filter;						 // ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;														 // ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Second.bits.b1VcellDeltaBig; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Second.bits.b1VcellDeltaBig = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Second.bits.VdeltaOvp_Second)
				{
					FaultWarnRecord(VdeltaOvp_Second);
					FaultWarnRecord2(VdeltaOvp_Second);
					Fault_Flag_Second.bits.VdeltaOvp_Second = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Second.bits.VdeltaOvp_Second == 1)
			{
				Fault_Flag_Second.bits.VdeltaOvp_Second = 0;
			}
		}
	}
}

void App_VdeltaOp_ThirdCheck(void)
{
	static UINT16 s_i16TimeCnt = 0;
	SPUBOPUPCHK t_sPubOPUPChk;

	if (1 == g_st_SysTimeFlag.bits.b1Sys10msFlag5)
	{
		t_sPubOPUPChk.u16ChkVal = g_stCellInfoReport.u16VCellDelta;
		t_sPubOPUPChk.u16OPValB = PRT_E2ROMParas.u16VdeltaOvp_Third;
		t_sPubOPUPChk.u16OPValS = PRT_E2ROMParas.u16VdeltaOvp_Rcv;
		t_sPubOPUPChk.i16ChkCnt = &s_i16TimeCnt;
		t_sPubOPUPChk.u16TimeCntB = PRT_E2ROMParas.u16VdeltaOvp_Filter;						// ¹ÊÕÏÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u16TimeCntS = PRT_E2ROMParas.u16VdeltaOvp_Filter;						// ¹ÊÕÏ»Ö¸´ÅĞ¶ÏÊ±¼ä500ms
		t_sPubOPUPChk.u8FlagLogic = 1;														// ¸ºÂß¼­
		t_sPubOPUPChk.u8FlagBit = g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig; // ¹ÊÕÏ±êÖ¾¸³¾ÉÖµ

		if (App_PubOPUPChk(&t_sPubOPUPChk))
		{
			g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig = t_sPubOPUPChk.u8FlagBit; // ·µ»Ø¹ÊÕÏÅĞ¶Ï½á¹û
			if (t_sPubOPUPChk.u8FlagBit == 1)
			{
				if (0 == Fault_Flag_Third.bits.VdeltaOvp_Third)
				{
					FaultWarnRecord(VdeltaOvp_Third);
					FaultWarnRecord2(VdeltaOvp_Third);
					System_ERROR_UserCallback(ERROR_VDEATLE_OVER);
					Fault_Flag_Third.bits.VdeltaOvp_Third = 1;
				}
			}
			if (t_sPubOPUPChk.u8FlagBit == 0 && Fault_Flag_Third.bits.VdeltaOvp_Third == 1)
			{
				Fault_Flag_Third.bits.VdeltaOvp_Third = 0;
			}
		}
	}
}

/*******************************************************************************
 *Function name: App_WarnCtrl()
 *Description :  IO port state sample, filter, warning judge and treatment
 *input:         void
 *global vars:   g_u16RunFlag.bit.WARN: run flag, 10ms once
 *output:        void
 *CALLED BY:     main()
 ******************************************************************************/
void App_WarnCtrl(void)
{
#if 0 // Ô­À´º¯ÊıÊ±»ù±»ÄÚÖÃÁË£¬ÀÁµÃ¸ÄÁË¡£
	if(0 == g_st_SysTimeFlag.bits.b1Sys10msFlag3) {
		return STARTUP_CONT;
	}
#endif

	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_PROTECT))
	{
		return;
	}

	App_CellOvp_SecondCheck();
	App_CellOvp_ThirdCheck();
	App_CellUvp_SecondCheck();
	App_CellUvp_ThirdCheck();

	App_BatOvp_SecondCheck();
	App_BatOvp_ThirdCheck();
	App_BatUvp_SecondCheck();
	App_BatUvp_ThirdCheck();

	App_MosOtp_SecondCheck();
	App_MosOtp_ThirdCheck();
	App_VdeltaOp_SecondCheck();
	App_VdeltaOp_ThirdCheck();

	App_IdischgOcp_SecondCheck();
	App_IdischgOcp_ThirdCheck();
	App_IchgOcp_SecondCheck();
	App_IchgOcp_ThirdCheck();

	App_CellSocUp_SecondCheck();
	App_CellSocUp_ThirdCheck();

	App_CellDisChgOtp_SecondCheck();
	App_CellDisChgOtp_ThirdCheck();
	App_CellDischgUtp_SecondCheck();
	App_CellDischgUtp_ThirdCheck();

	App_CellChgOtp_SecondCheck();
	App_CellChgOtp_ThirdCheck();
	App_CellChgUtp_SecondCheck();
	App_CellChgUtp_ThirdCheck();

	// PwrMag_Protect_Record_StartUp();
}

// ¼ÇÂ¼ÊÇ°´Ë³Ğò¼ÇÂ¼ÏÂÈ¥£¬ÉÏ´«ÔòÊÇ×îĞÂµÄÔÚ¶¥²¿
void FaultWarnRecord(enum FaultFlag num)
{
	if (num >= 1 && num <= 13)
	{
		if (FaultPoint_First >= Record_len)
		{
			FaultPoint_First = 0;
		}
		Fault_record_First[FaultPoint_First++] = num;
	}
	else if (num >= 14 && num <= 26)
	{
		if (FaultPoint_Second >= Record_len)
		{
			FaultPoint_Second = 0;
		}
		Fault_record_Second[FaultPoint_Second++] = num;
	}
	else
	{
		if (FaultPoint_Third >= Record_len)
		{
			FaultPoint_Third = 0;
		}
		/*
		RTC_Fault_record_Third[FaultPoint_Third][0] = RTC_time.RTC_Time_Year;
		RTC_Fault_record_Third[FaultPoint_Third][1] = RTC_time.RTC_Time_Month;
		RTC_Fault_record_Third[FaultPoint_Third][2] = RTC_time.RTC_Time_Day;
		RTC_Fault_record_Third[FaultPoint_Third][3] = RTC_time.RTC_Time_Hour;
		RTC_Fault_record_Third[FaultPoint_Third][4] = RTC_time.RTC_Time_Minute;
		RTC_Fault_record_Third[FaultPoint_Third][5] = RTC_time.RTC_Time_Second;
		*/
		Fault_record_Third[FaultPoint_Third++] = num;
	}
#ifdef _FAULT_RECORD
	PwrMag_Protect_Record(num);
#endif
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

#ifdef _FAULT_RECORD
void PwrMag_Protect_Record(enum FaultFlag num)
{
	UINT8 j;

	if (SystemStatus.bits.b1StartUpBMS)
	{ // ¿ª»úÍê±ÏÔÙ½øÈë£¬¿ª»úÇ°µÄ¼ì²â½á¹û²»ÔÚÕâĞ´
		if (num >= 1 && num <= 13)
		{
			++FaultCnt_StartUp_First;
		}
		else if (num >= 14 && num <= 26)
		{
			++FaultCnt_StartUp_Second;
		}
		else
		{
			++FaultCnt_StartUp_Third;
		}
	}
	else
	{
		if (num >= 1 && num <= 13)
		{
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_FIRST + ((FaultPoint_First - 1) << 1), Fault_record_First[FaultPoint_First - 1]);
			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_FIRST, FaultPoint_First);
		}
		else if (num >= 14 && num <= 26)
		{
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_SECOND + ((FaultPoint_Second - 1) << 1), Fault_record_Second[FaultPoint_Second - 1]);
			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_SECOND, FaultPoint_Second);
		}
		else
		{
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_THIRD + ((FaultPoint_Third - 1) << 1), Fault_record_Third[FaultPoint_Third - 1]);
			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_THIRD, FaultPoint_Third);
			for (j = 0; j < 6; ++j)
			{
				WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_THIRD_RTC + (((FaultPoint_Third - 1) * 6 + j) << 1), RTC_Fault_record_Third[FaultPoint_Third - 1][j]);
			}
		}
	}
}

// ¸Ãº¯ÊıµÄ±³¾°ÊÇÈç¹û¸Õ¿ª»úµÄÊ±ºò³öÏÖ2-3¸ö±£»¤£¬Ôò»á¿¨×¡£¬Ê¹Ê±»ù¿¨×¡1.5s×óÓÒ£¬µ¼ÖÂ¿ª»úÊ±¼äÑÓºó
void PwrMag_Protect_Record_StartUp(void)
{
	UINT8 j;
	static UINT8 su8_StartUpRecord = 0;
	if (SystemStatus.bits.b1StartUpBMS)
	{ // ¿ª»úÍê±ÏÔÙ½øÈë
		return;
	}

	switch (su8_StartUpRecord)
	{
	case 0:
		if (FaultCnt_StartUp_First)
		{
			// MCUO_DEBUG_LED2 = 0;
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_FIRST + ((FaultCnt_StartUp_First - 1) << 1), Fault_record_First[FaultCnt_StartUp_First - 1]);
			// MCUO_DEBUG_LED2 = 1;

			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_FIRST, FaultCnt_StartUp_First);
			--FaultCnt_StartUp_First;
		}
		else if (FaultCnt_StartUp_Second)
		{
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_SECOND + ((FaultCnt_StartUp_Second - 1) << 1), Fault_record_Second[FaultCnt_StartUp_Second - 1]);
			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_SECOND, FaultCnt_StartUp_Second);
			--FaultCnt_StartUp_Second;
		}
		else if (FaultCnt_StartUp_Third)
		{
			MCUO_DEBUG_LED2 = 0;
			WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_THIRD + ((FaultCnt_StartUp_Third - 1) << 1), Fault_record_Third[FaultCnt_StartUp_Third - 1]);
			WriteEEPROM_Word_WithZone(E2P_ADDR_E2POS_FR_TEMP_THIRD, FaultCnt_StartUp_Third);
			for (j = 0; j < 6; ++j)
			{
				WriteEEPROM_Word_WithZone(E2P_ADDR_START_FR_THIRD_RTC + (((FaultCnt_StartUp_Third - 1) * 6 + j) << 1), RTC_Fault_record_Third[FaultCnt_StartUp_Third - 1][j]);
			}
			MCUO_DEBUG_LED2 = 1;
			--FaultCnt_StartUp_Third;
		}
		else
		{
			su8_StartUpRecord = 1;
		}
		break;

	case 1:
		break;

	default:
		break;
	}
}
#endif
