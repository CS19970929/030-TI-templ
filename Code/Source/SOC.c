#include "main.h"

UINT16 SOC_Table_Set[SOC_TABLE_SIZE];

const UINT16 SOC_Table_Default[SOC_TABLE_SIZE] = {
	3336,
	100,
	3332,
	90,
	3330,
	80,
	3327,
	75,
	3316,
	70,
	3301,
	65,
	3294,
	60,
	3291,
	55,
	3290,
	50,
	3288,
	45,
	3286,
	40,
	3279,
	35,
	3266,
	30,
	3254,
	25,
	3236,
	20,
	3212,
	15,
	3198,
	10,
	3112,
	5,
	2526,
	0,
	1000,
	0,
	1000,
	0,
};

static void SOC_LoadDefaultTableIfNeeded(void)
{
	UINT16 i;

	// 当前工程没有稳定的 SOC 表写入路径，开机时先把空白表填成默认表。
	// 如果后续真的写入了自定义表，这里不会覆盖已有有效数据。
	if (SOC_Table_Set[0] != 0 && SOC_Table_Set[1] != 0)
	{
		return;
	}

	for (i = 0; i < SOC_TABLE_SIZE; ++i)
	{
		SOC_Table_Set[i] = SOC_Table_Default[i];
	}
}

static UINT16 SOC_GetReportedSoc(void)
{
	UINT16 soc;

	soc = SOC_GetDisplaySoc();
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		return 60;
	}
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
	{
		return 0;
	}
	return soc;
}

// ³¤ÆÚ¸üÐÂÊý¾Ý
void RefreshData_SOC(void)
{
	SOC_Enhance_Element.u16_VCellMax = g_stCellInfoReport.u16VCellMax;
	// SOC_Enhance_Element.u16_VCellMin = g_stCellInfoReport.u16VCellMin;	//À©É¢³öÈ¥£¬²»ÓÃÕâ¸öÖµ£¬È¥µô6ºÍ16´®
	SOC_Enhance_Element.u16_VCellMin = g_stCellInfoReport.u16VCellMin; // ¹«°æ¾ö¶¨²»À©É¢³öÈ¥£¬°üº¬6ºÍ16´®£¬¿Í»§Ê¹ÓÃÌåÑéÎÊÌâ£¬µÍÑ¹±£»¤SOCÒ»¶¨Òª½µÏÂÀ´
	SOC_Enhance_Element.u16_Ichg = g_stCellInfoReport.u16Ichg;
	SOC_Enhance_Element.u16_Idsg = g_stCellInfoReport.u16IDischg;
	SOC_Enhance_Element.u16_TempMax = g_stCellInfoReport.u16TempMax;
	SOC_Enhance_Element.u16_TempMin = g_stCellInfoReport.u16TempMin;
}

static void SOC_CopyTableToEnhance(void)
{
	UINT16 i;

	for (i = 0; i < SOC_Size_TableCanSet; ++i)
	{
		SOC_Enhance_Element.SOC_Table_CanSet[i] = SOC_Table_Set[i];
	}
}

// »ñÈ¡Êý¾Ý
void GetData_SOC(void)
{
	System_ErrFlag.u8ErrFlag_SOC_Cail = SOC_Enhance_Element.u16_SOC_CailFaultCnt;

	g_stCellInfoReport.SocElement.u16Soc = SOC_GetReportedSoc();
	g_stCellInfoReport.SocElement.u16Soh = SOC_Enhance_Element.u8_SOH;
	g_stCellInfoReport.SocElement.u16CapacityNow = SOC_Enhance_Element.u16_CapacityNow;
	g_stCellInfoReport.SocElement.u16CapacityFull = SOC_Enhance_Element.u16_CapacityFull;
	g_stCellInfoReport.SocElement.u16CapacityFactory = SOC_Enhance_Element.u16_CapacityFactory;
	g_stCellInfoReport.SocElement.u16Cycle_times = SOC_Enhance_Element.u16_Cycle_times;

	// g_stCellInfoReport.u16VCell[30] = SOC_Enhance_Element.u8_SOC_OCV_Cali;
}

// Ò»´ÎÐÔ¸³Öµ
void InitData_SOC(void)
{
	UINT16 i;

	SOC_LoadDefaultTableIfNeeded();

	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
	SOC_Enhance_Element.u16_SOC_CycleT_Limit = 5000;
	SOC_Enhance_Element.u16_SOC_TableSelect = OtherElement.u16Soc_TableSelect;
	// SOC_Enhance_Element.u16_SOC_DsgVcell_Limit = OtherElement.u16Soc_V_0;
	SOC_Enhance_Element.u16_SOC_100_Vol = OtherElement.u16Soc_V_100;
	SOC_Enhance_Element.u16_SOC_0_Vol = OtherElement.u16Soc_V_0;

	SOC_Enhance_Element.u8_LargeCurFlag_Chg = 0; // Ä¬ÈÏÊÇ0£¬³ý·ÇÄ©¶Ë´óµçÁ÷CC³ä·Åµçµ¼ÖÂÃ»·¨ÔÚ¶Ëµã´ïµ½100%ºÍ0%ÖÃ1
	SOC_Enhance_Element.u8_LargeCurFlag_Dsg = 0;

	for (i = 0; i < E2P_AdressNum; ++i)
	{
		SOC_Enhance_Element.SOC_E2P_Adress[i] = E2P_ADDR_E2POS_ENHANCE_SOC + 2 * i;
	}

	SOC_CopyTableToEnhance();
	// SOC_Enhance_Element.SOC_E2P_Adress = E2P_ADDR_E2POS_ENHANCE_SOC;
}

void App_SOC(void)
{
	toggleLed();
	
	RefreshData_SOC();
	SOC_IntEnhance_Ctrl(gu8_200msAccClock_Flag);
	GetData_SOC();


	if (SOC_Enhance_Element.u16_SOC_InitOver)
	{
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0; // ³õÊ¼»¯Íê±Ï
	}
}
