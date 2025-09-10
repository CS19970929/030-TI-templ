#include "main.h"

// LED_COMMAND LED_Command = LED_STARTUP;
LED_MODE Led_Mode;
LED_COMMAND LED_Command;
UINT8 gu8_MosOpenFlag = 0;

void LED_StartUp(void)
{
	static UINT8 su8_Tcnt = 0;

	++su8_Tcnt;
	if (su8_Tcnt < 10)
	{ // 红1s
		MCUO_LED_RED = LED_OPEN;
		MCUO_LED_GREEN = LED_CLOSE;
		MCUO_BUZZER = LED_CLOSE;
	}
	else if (su8_Tcnt < 20)
	{ // 绿1s
		MCUO_LED_RED = LED_CLOSE;
		MCUO_LED_GREEN = LED_OPEN;
		MCUO_BUZZER = LED_CLOSE;
	}
	else if (su8_Tcnt < 30)
	{ // 绿1s且叫1s
		MCUO_LED_RED = LED_CLOSE;
		MCUO_LED_GREEN = LED_OPEN;
		MCUO_BUZZER = LED_OPEN;
	}
	else
	{
		// su8_Tcnt = 0;			//只有重启时，这个函数才会再次执行一轮
		Led_Mode.all = 0;
		LED_Command = LED_FAULT_MONITOR;
	}
}

// 2个状况，从100%-0%，从0-100%
void LED_SocLow(void)
{
	static UINT16 su16_Tcnt = 0;
	static UINT16 su16_TwinkleCnt = 0;
	static UINT16 su16_Buzzer3minFlag = 0;
	static UINT16 su16_Buzzer15minFlag = 0;

	if (!MCUI_EMERGENCY_KEY)
	{
		LED_Command = LED_RESCUE_PIVOT;
		MCUO_LED_RED = 0;
		MCUO_LED_GREEN = 0;
		MCUO_BUZZER = 0;
		su16_Tcnt = 0;
		su16_TwinkleCnt = 0;
		su16_Buzzer3minFlag = 0;
		su16_Buzzer15minFlag = 0;
		return;
	}

	if (Led_Mode.bits.b1_LoadErr || Led_Mode.bits.b1_BatteryErr)
	{
		/*
		//这里不能有外部设备操作动作，不然就三个函数都出现操作，会乱
		MCUO_LED_RED = 0;
		MCUO_LED_GREEN = 1;
		MCUO_BUZZER = 0;
		*/
		su16_Tcnt = 0;
		su16_TwinkleCnt = 0;
		su16_Buzzer3minFlag = 0;
		su16_Buzzer15minFlag = 0;
		Led_Mode.bits.b1_SocLow = 0; // 必须得重新判断
		return;
	}

	if (g_stCellInfoReport.SocElement.u16Soc < 50 || Led_Mode.bits.b1_SocLow)
	{
		Led_Mode.bits.b1_SocLow = 1;
		if (++su16_TwinkleCnt >= 5)
		{
			su16_TwinkleCnt = 0;
			MCUO_LED_RED = !MCUO_LED_RED;
			MCUO_LED_GREEN = MCUO_LED_RED; // 两盏灯同闪
		}

		if (g_stCellInfoReport.SocElement.u16Soc > 30)
		{
			if (!su16_Buzzer3minFlag)
			{
				if (++su16_Tcnt < 3 * 600)
				{ // 3min
					MCUO_BUZZER = 1;
				}
				else
				{
					su16_Tcnt = 0;
					MCUO_BUZZER = 0;
					su16_Buzzer3minFlag = 1;
				}
			}
		}
		else
		{
			if (!su16_Buzzer15minFlag)
			{
				if (++su16_Tcnt < 15 * 600)
				{ // 15min
					MCUO_BUZZER = 1;
				}
				else
				{
					su16_Tcnt = 0;
					MCUO_BUZZER = 0;
					su16_Buzzer15minFlag = 1;
				}
			}
			if (!su16_Buzzer3minFlag)
				su16_Buzzer3minFlag = 1; // 刚开机是低于30%的时候
		}
	}

	if (g_stCellInfoReport.SocElement.u16Soc > 55)
	{ // 恢复正常，绿灯常亮
		MCUO_LED_RED = LED_CLOSE;
		MCUO_LED_GREEN = LED_OPEN;
		MCUO_BUZZER = LED_CLOSE;
		su16_Tcnt = 0;
		su16_TwinkleCnt = 0;
		su16_Buzzer3minFlag = 0;
		su16_Buzzer15minFlag = 0;
		Led_Mode.bits.b1_SocLow = 0;
	}
}

void LED_BatteryErr(void)
{
	if (!MCUI_EMERGENCY_KEY)
	{
		LED_Command = LED_RESCUE_PIVOT;
		MCUO_LED_RED = 0;
		MCUO_BUZZER = 0;
		MCUO_LED_GREEN = 0;
		return;
	}

	if (Led_Mode.bits.b1_LoadErr)
	{
		Led_Mode.bits.b1_BatteryErr = 0;
		return;
	}

	switch (Led_Mode.bits.b1_BatteryErr)
	{
	case 0:
		if ((g_stCellInfoReport.unMdlFault_Third.all & 0xEFCF) != 0)
		{ // 电池组异常，红灯常亮，警报持续，绿灯灭
			MCUO_LED_RED = 1;
			MCUO_BUZZER = 1;
			MCUO_LED_GREEN = 0;
			Led_Mode.bits.b1_BatteryErr = 1;
		}
		break;

	case 1:
		if ((g_stCellInfoReport.unMdlFault_Third.all & 0xEFCF) == 0)
		{ // 复原
			MCUO_LED_RED = LED_CLOSE;
			MCUO_BUZZER = LED_CLOSE;
			MCUO_LED_GREEN = LED_CLOSE;
			Led_Mode.bits.b1_BatteryErr = 0;
		}
		break;

	default:
		break;
	}

	/*
	//以下这种写法明显有问题，还是得switch语句
	if((g_stCellInfoReport.unMdlFault.all&0xEFCF) != 0) {	//电池组异常，红灯常亮，警报持续，绿灯灭
		MCUO_LED_RED = 1;
		MCUO_BUZZER = 1;
		MCUO_LED_GREEN = 0;
		Led_Mode.bits.b1_BatteryErr = 1;
	}
	else {								//复原
		MCUO_LED_RED = LED_CLOSE;
		MCUO_BUZZER = LED_CLOSE;
		MCUO_LED_GREEN = LED_CLOSE;
		Led_Mode.bits.b1_BatteryErr = 0;
	}
	*/
}

// 负载异常，如果不按按钮，则永不复原，是这样吗？
// 后续讨论，按键复位，取消报警
void LED_LoadErr(void)
{
	static UINT16 su16_TwinkleCnt = 0;

	if (!MCUI_EMERGENCY_KEY)
	{
		LED_Command = LED_RESCUE_PIVOT;

		su16_TwinkleCnt = 0;
		MCUO_LED_RED = 0;
		MCUO_LED_GREEN = 0;
		MCUO_BUZZER = 0;
		return;
	}

	switch (Led_Mode.bits.b1_LoadErr)
	{
	case 0:
		if (g_stCellInfoReport.unMdlFault_Third.all & 0x0030)
		{ // 负载异常(过流，短路)，红灯闪，警报持续
			Led_Mode.bits.b1_LoadErr = 1;
		}
		break;

	case 1:
		if (++su16_TwinkleCnt >= 5)
		{
			su16_TwinkleCnt = 0;
			MCUO_LED_RED = !MCUO_LED_RED;
		}
		MCUO_BUZZER = 1;
		MCUO_LED_GREEN = 0;

		if (0)
		{ // 复原，只能通过按键复原
			su16_TwinkleCnt = 0;
			MCUO_LED_RED = 0;
			MCUO_LED_GREEN = 0;
			MCUO_BUZZER = 0;
			Led_Mode.bits.b1_LoadErr = 0;
		}
		break;

	default:
		break;
	}
}

void LED_Rescue_Pivot(void)
{
	static UINT16 su16_Tcnt = 0;
	static UINT16 su16_TwinkleCnt = 0;

	if (!MCUI_EMERGENCY_KEY)
	{
		++su16_Tcnt;
		if (++su16_TwinkleCnt >= 5)
		{ // 在按着按键时，两盏灯交互闪
			su16_TwinkleCnt = 0;
			MCUO_LED_RED = !MCUO_LED_RED;
			MCUO_LED_GREEN = !MCUO_LED_RED;
		}

		if (su16_Tcnt >= 10 * 10)
		{ // 在线监测，如果超过10s，直接重启，类似电脑
			LED_Command = LED_RESTART_MPU;
			su16_Tcnt = 0; // 复原
			su16_TwinkleCnt = 0;
			MCUO_LED_RED = LED_CLOSE;
			MCUO_LED_GREEN = LED_CLOSE;
		}
	}
	else
	{
		if (su16_Tcnt >= 2 * 10 && su16_Tcnt <= 5 * 10)
		{
			LED_Command = LED_EMERGENCY;
		}
		else if (su16_Tcnt >= 8 * 10)
		{
			LED_Command = LED_RESTART_MPU;
		}
		else
		{
			LED_Command = LED_STARTUP;
		}

		su16_Tcnt = 0; // 复原
		su16_TwinkleCnt = 0;
		MCUO_LED_RED = LED_CLOSE;
		MCUO_LED_GREEN = LED_CLOSE;
	}
}

void LED_Emergency(void)
{
	static UINT16 su16_Tcnt = 0;
	static UINT16 su16_TwinkleCnt = 0;

	if (!MCUI_EMERGENCY_KEY)
	{ // 还是按的话，继续回去选择
		su16_TwinkleCnt = 0;
		su16_Tcnt = 0;
		MCUO_LED_RED = 0;
		MCUO_LED_GREEN = 0;
		MCUO_BUZZER = 0;
		LED_Command = LED_RESCUE_PIVOT;
		return;
	}

	if (++su16_Tcnt < 40 * 10)
	{
		gu8_MosOpenFlag = 1; // 强制打开MOS
		if (++su16_TwinkleCnt >= 5)
		{ // 绿灯闪烁40s
			su16_TwinkleCnt = 0;
			MCUO_LED_GREEN = !MCUO_LED_GREEN;
		}
	}
	else
	{
		su16_TwinkleCnt = 0; // 复原
		su16_Tcnt = 0;
		gu8_MosOpenFlag = 0;
		MCUO_LED_GREEN = 0;
		LED_Command = LED_STARTUP; // 还是建议回去灯模块重新启动一下，另外一个是单片机重启
	}
}

void LED_ReStartMPU(void)
{
	MCU_RESET(); // 单片机重启
}

void InitLED(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	// PA12_MCUO_BUZZER
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// PC14_LED_RED，PC15_LED_GREEN
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// PA0_MCUI_KEY
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	LED_Command = LED_STARTUP;
	Led_Mode.all = 0;

	// MCUO_LED_GREEN = 1;
}

void APP_LED(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
	{
		return;
	}

	if (SystemStatus.bits.b1StartUpBMS)
	{
		return;
	}

	switch (LED_Command)
	{
	case LED_STARTUP:
		LED_StartUp();
		break;

	case LED_FAULT_MONITOR:
		LED_LoadErr(); // 优先级如函数顺序所示
		LED_BatteryErr();
		LED_SocLow();
		break;

	case LED_RESCUE_PIVOT:
		LED_Rescue_Pivot();
		break;

	case LED_EMERGENCY:
		LED_Emergency();
		break;

	case LED_RESTART_MPU:
		LED_ReStartMPU();
		break;

	// 这个写法错误，如果同时出现多种错误会紊乱，必须有优先级的问题，要切记
	// 还是这种写法好点
	/*
	case LED_MONITOR:
		LED_SocLow();
		LED_BatteryErr();
		LED_LoadErr();
		break;
	*/
	default:
		break;
	}
}
