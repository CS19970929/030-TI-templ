#include "main.h"

UINT16 SOC_Table_Set[SOC_TABLE_SIZE];

const UINT16 SOC_Table_Default[42] = {
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

// 长期更新数据
void RefreshData_SOC(void)
{
	SOC_Enhance_Element.u16_VCellMax = g_stCellInfoReport.u16VCellMax;
	// SOC_Enhance_Element.u16_VCellMin = g_stCellInfoReport.u16VCellMin;	//扩散出去，不用这个值，去掉6和16串
	SOC_Enhance_Element.u16_VCellMin = g_stCellInfoReport.u16VCellMin; // 公版决定不扩散出去，包含6和16串，客户使用体验问题，低压保护SOC一定要降下来
	SOC_Enhance_Element.u16_Ichg = g_stCellInfoReport.u16Ichg;
	SOC_Enhance_Element.u16_Idsg = g_stCellInfoReport.u16IDischg;
}

// 获取数据
void GetData_SOC(void)
{
	System_ErrFlag.u8ErrFlag_SOC_Cail = SOC_Enhance_Element.u16_SOC_CailFaultCnt;

	g_stCellInfoReport.SocElement.u16Soc = SOC_Enhance_Element.u8_SOC;
	g_stCellInfoReport.SocElement.u16Soh = SOC_Enhance_Element.u8_SOH;
	g_stCellInfoReport.SocElement.u16CapacityNow = SOC_Enhance_Element.u16_CapacityNow;
	g_stCellInfoReport.SocElement.u16CapacityFull = SOC_Enhance_Element.u16_CapacityFull;
	g_stCellInfoReport.SocElement.u16CapacityFactory = SOC_Enhance_Element.u16_CapacityFactory;
	g_stCellInfoReport.SocElement.u16Cycle_times = SOC_Enhance_Element.u16_Cycle_times;

	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		g_stCellInfoReport.SocElement.u16Soc = 60;
	}
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
	{
		g_stCellInfoReport.SocElement.u16Soc = 0;
	}

	// g_stCellInfoReport.u16VCell[30] = SOC_Enhance_Element.u8_SOC_OCV_Cali;
}

// 一次性赋值
void InitData_SOC(void)
{
	UINT16 i;

	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
	;
	SOC_Enhance_Element.u16_SOC_CycleT_Limit = 5000;
	SOC_Enhance_Element.u16_SOC_TableSelect = OtherElement.u16Soc_TableSelect;
	// SOC_Enhance_Element.u16_SOC_DsgVcell_Limit = OtherElement.u16Soc_V_0;
	SOC_Enhance_Element.u16_SOC_100_Vol = OtherElement.u16Soc_V_100;
	SOC_Enhance_Element.u16_SOC_0_Vol = OtherElement.u16Soc_V_0;

	SOC_Enhance_Element.u8_LargeCurFlag_Chg = 0; // 默认是0，除非末端大电流CC充放电导致没法在端点达到100%和0%置1
	SOC_Enhance_Element.u8_LargeCurFlag_Dsg = 0;

	for (i = 0; i < E2P_AdressNum; ++i)
	{
		SOC_Enhance_Element.SOC_E2P_Adress[i] = E2P_ADDR_E2POS_ENHANCE_SOC + 2 * i;
	}

	for (i = 0; i < SOC_Size_TableCanSet; ++i)
	{
		SOC_Enhance_Element.SOC_Table_CanSet[i] = SOC_Table_Set[i];
	}
	// SOC_Enhance_Element.SOC_E2P_Adress = E2P_ADDR_E2POS_ENHANCE_SOC;
}

void SOC_OCV_Fix(void)
{
	static UINT8 su8_OCV_Cali_Flag = 0;
	static UINT16 gu16_RTC_TimeCnt = 0;

	switch (su8_OCV_Cali_Flag)
	{
	case 0:
		// 太麻烦了，直接卡休眠前那个点进行操作便可
		if (Sleep_Mode.bits.b1_ToSleepFlag)
		{
			su8_OCV_Cali_Flag = 1;
		}
		break;

	case 1:
		// 开机统计+1
		gu16_RTC_TimeCnt = ReadEEPROM_Word_WithZone(E2P_ADDR_SOC_RTC_CNT);
		if (gu8_WakeUp_Type == FLASH_VALUE_WAKE_RTC)
		{
			gu16_RTC_TimeCnt++;
		}
		else
		{
			// 别的情况唤醒，就不是RTC唤醒，则清空。
			// 只在RTC唤醒的时候操作，别的时候不校准。
			// 因为在深度休眠的时候，大概率RTC已经校准为0了。
			gu16_RTC_TimeCnt = 0;
		}
		WriteEEPROM_Word_WithZone(E2P_ADDR_SOC_RTC_CNT, gu16_RTC_TimeCnt);
		su8_OCV_Cali_Flag = 2;
		break;

	case 2:
		// 原来是电流和通讯不会校准，现在改为RTC次数统计(已经包含电流和通讯的情况)
		if (gu16_RTC_TimeCnt >= 2 * 60 * 24 * 20)
		{ // 20天
			// if(gu16_RTC_TimeCnt >= 4) {		//4h
			gu16_RTC_TimeCnt = 0;

			// 校准后更新次数
			WriteEEPROM_Word_WithZone(E2P_ADDR_SOC_RTC_CNT, gu16_RTC_TimeCnt);

			SOC_Enhance_Element.u16_RefreshData_Flag = 1;
		}

		su8_OCV_Cali_Flag = 3;
		break;

	case 3:
		// 释放，进入休眠
		Sleep_Mode.bits.b1_ToSleepFlag = 0;
		break;

	default:
		break;
	}
}

void App_SOC(void)
{
	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_SOC))
	{
		return;
	}

	/*	//放这里，开机会出现电量过低保护，因为200ms后才赋值，该时间内使保护逻辑(10ms时基)运行完毕
		//太细了
	if(SOC_Enhance_Element.u16_SOC_InitOver) {
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0;				//初始化完毕
	}
	*/

	if (0 == gu8_200msAccClock_Flag)
	{
		return;
	}

	// SOC lib处理
	SOC_OCV_Fix();
	RefreshData_SOC();
	GetData_SOC();
	SOC_IntEnhance_Ctrl(gu8_200msAccClock_Flag);

	gu8_200msAccClock_Flag = 0;

	if (SOC_Enhance_Element.u16_SOC_InitOver)
	{
		System_Func_StartUp.bits.b1StartUpFlag_SOC = 0; // 初始化完毕
	}
}
