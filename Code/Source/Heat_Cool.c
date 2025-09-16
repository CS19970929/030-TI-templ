#include "main.h"

enum HEAT_COOL_CTRL_STATUS HeatCtrl_Command = ST_HEAT_DET_SELF;
enum HEAT_COOL_CTRL_STATUS CoolCtrl_Command = ST_COOL_DET_SELF;
union HEAT_COOL_FAULT_FLAG Heat_Cool_FaultFlag;

struct HEAT_COOL_ELEMENT Heat_Cool_Element;

void Heat_Control(void)
{
	static UINT8 temp_Count = 0;
	static uint16_t heat_Count = 0;
	static UINT16 closeChgMosCount = 0;
	static UINT8 dsg_Count = 0; // 放电时间计数

	static uint8_t state_heat = 0;

	if (!System_OnOFF_Func.bits.b1OnOFF_Heat)
	{ // 没有这个功能，不进来
		SystemStatus.bits.b1Status_Heat = 0;
		MCUO_RELAY_HEAT = 0;

		state_heat = 0;
		return;
	}

	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_HEAT))
	{
		return;
	}

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	/* 加热电流设置为50A的时候关闭加热 */
	if (Heat_Cool_Element.u16Heat_OpenCur == 500)
	{
		SystemStatus.bits.b1Status_Heat = 0;
		MCUO_RELAY_HEAT = 0;
		Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

		state_heat = 0;
		return;
	}

	switch (state_heat)
	{
	case 0:
		if ((g_stCellInfoReport.u16Ichg >= Heat_Cool_Element.u16Heat_OpenCur) && (g_stCellInfoReport.u16TempMin < Heat_Cool_Element.u16Heat_OpenTemp))
		{
			if (++temp_Count == 2)
			{
				temp_Count = 0;
				SystemStatus.bits.b1Status_Heat = 1;
				state_heat = 1;

				Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;
			}
		}
		break;
	case 1:
		if (g_stCellInfoReport.u16TempMin > Heat_Cool_Element.u16Heat_OpenTemp)
		{
			/* 外部不控制充电管 */
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		}

		/* 放电时间大于 2A，时间大于2s 关闭加热 */
		if (g_stCellInfoReport.u16IDischg > 0)
		{
			// dsg_Count++;
			/* 停止加热 */
			SystemStatus.bits.b1Status_Heat = 0;
			// state_heat = 0;

			if (++dsg_Count >= 3)
			{
				dsg_Count = 0;
				state_heat = 2;

				Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;
				Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_CLOSE_MODE;
				System_ERROR_UserCallback(ERROR_HEAT);
			}
			else
			{
				Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
				state_heat = 0;
			}
		}

		if ((g_stCellInfoReport.u16TempMin > Heat_Cool_Element.u16Heat_CloseTemp))
		{
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

			SystemStatus.bits.b1Status_Heat = 0;
			state_heat = 0;
		}

		/* 加热时间超过最大加热时间时间 */
		// if (++heat_Count == 60 * 60 * 2)
		// {
		// 	heat_Count = 0;
		// 	/* 外部不控制充电管 */
		// 	// Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		// 	SH367309_DriverMos_Ctrl(GPIO_CHG, 1);

		// 	System_ERROR_UserCallback(ERROR_HEAT);

		// 	SystemStatus.bits.b1Status_Heat = 0;
		// 	state_heat = 0;
		// }
		break;
	case 2:
		static uint8_t cnt_30s = 0;
		if (++cnt_30s >= 28)
		{
			cnt_30s = 0;

			state_heat = 3;
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
		}
		break;
	case 3:
		static uint16_t cnt_1h = 0;

		static uint8_t heat_err_state = 0;

		if (System_ERROR_UserCallback(ERROR_STATUS_HEAT))
		{
			switch (heat_err_state)
			{
			case 0:
				ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 0;
				heat_err_state = 1;
				break;
			case 1:
				if (ChargerLoad_Func.bits.b1ON_Charger_AllSeries)
				{
					heat_err_state = 0;
					ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 0;

					System_ERROR_UserCallback(ERROR_REMOVE_HEAT);
					state_heat = 0;

					Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
				}
				break;

			default:
				break;
			}
		}
		// static uint8_t first = 0;
		// static uint8_t sub_chgmos_state = 0;
		if (g_stCellInfoReport.u16IDischg >= 20 || (g_stCellInfoReport.u16TempMin > (0 + 40) * 10) || (++cnt_1h >= 60 * 60))
		{
			cnt_1h = 0;
			heat_err_state = 0;

			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

			state_heat = 0;
			System_ERROR_UserCallback(ERROR_REMOVE_HEAT);
		}
		// if (g_stCellInfoReport.u16TempMin > (0 + 40) * 10)
		// {
		// 	Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		// 	state_heat = 0;
		// 	System_ERROR_UserCallback(ERROR_REMOVE_HEAT);
		// }
		// switch (sub_chgmos_state)
		// {
		// case 0:
		// 	if (g_stCellInfoReport.u16IDischg >= 20)
		// 	{
		// 		SH367309_DriverMos_Ctrl(GPIO_CHG, 1);
		// 		sub_chgmos_state = 1;
		// 	}
		// 	break;
		// case 1:
		// 	if (g_stCellInfoReport.u16IDischg < 20)
		// 	{
		// 		SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
		// 		sub_chgmos_state = 0;
		// 	}
		// 	break;
		// default:
		// 	break;
		// }
		// if (ChargerLoad_Func.bits.b1ON_Charger_AllSeries)
		// {
		// 	ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 0;

		// 	System_ERROR_UserCallback(ERROR_REMOVE_HEAT);
		// 	state_heat = 0;
		// }

		// if (++cnt_1h >= 60 * 60)
		// {
		// 	cnt_1h = 0;
		// 	state_heat = 0;

		// 	Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

		// 	System_ERROR_UserCallback(ERROR_REMOVE_HEAT);
		// }
		break;
	default:
		break;
	}

	MCUO_RELAY_HEAT = SystemStatus.bits.b1Status_Heat;
}


void InitHeat_Cool(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = PIN_HT_CHG;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIO_HT_CHG, &GPIO_InitStructure);
}

// 加热冷凝电流均无法检测
void App_Heat_Cool_Ctrl(void)
{
	Heat_Control(); // 关闭和电流不挂钩(证明开始使用了)，打开必须挂钩，不然就长期在那里耗，把电池耗到低压保护。
}
