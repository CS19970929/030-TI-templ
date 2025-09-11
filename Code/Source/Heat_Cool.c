#include "main.h"

enum HEAT_COOL_CTRL_STATUS HeatCtrl_Command = ST_HEAT_DET_SELF;
enum HEAT_COOL_CTRL_STATUS CoolCtrl_Command = ST_COOL_DET_SELF;
union HEAT_COOL_FAULT_FLAG Heat_Cool_FaultFlag;

struct HEAT_COOL_ELEMENT Heat_Cool_Element;

void Heat_Control(void)
{
	static UINT8 heat_Flag = 0; // heat_Flag == 1，加热功能打开
	static UINT8 temp_Count = 0;
	static UINT8 heat_Count = 0;
	static UINT16 closeChgMosCount = 0;
	static UINT8 dsg_Count = 0; // 放电时间计数

	if (!System_OnOFF_Func.bits.b1OnOFF_Heat)
	{ // 没有这个功能，不进来
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
		heat_Flag = 0;
		Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		return;
	}

	if (heat_Flag)
	{
		/* 加热后温度 大于0 又小于0 */
		if (g_stCellInfoReport.u16TempMin < Heat_Cool_Element.u16Heat_OpenTemp)
		{
			/* 打开加热继电器1s左右后，才关闭充电管 */
			if (1 == closeChgMosCount++)
			{
				closeChgMosCount = 0;
				/* 外部强制关闭充电管 */

				Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;
			}
		}

		if (g_stCellInfoReport.u16TempMin > Heat_Cool_Element.u16Heat_OpenTemp)
		{
			/* 外部不控制充电管 */
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		}

		/* 放电时间大于 2A，时间大于2s 关闭加热 */
		if (g_stCellInfoReport.u16IDischg > 20)
		{
			dsg_Count++;
		}

		/* 加热温度大于10度 */ /* 或者 放电电流大于3A */
		if ((g_stCellInfoReport.u16TempMin > Heat_Cool_Element.u16Heat_CloseTemp) || (dsg_Count == 2))
		{
			dsg_Count = 0;

			/* 外部不控制充电管 */
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

			/* 停止加热 */
			SystemStatus.bits.b1Status_Heat = 0;
			heat_Flag = 0;
		}

		/* 加热时间超过最大加热时间时间 */
		if (++heat_Count == 60 * 60 * 3)
		{
			/* 外部不控制充电管 */
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

			System_ERROR_UserCallback(ERROR_HEAT);
			heat_Flag = 0;
		}
	}
	else
	{
		/* 电流大于5A并且温度低于0 */
		if ((g_stCellInfoReport.u16Ichg >= Heat_Cool_Element.u16Heat_OpenCur) && (g_stCellInfoReport.u16TempMin < Heat_Cool_Element.u16Heat_OpenTemp))
		{
			/* 温度连续4秒低于0 */
			if (++temp_Count == 2)
			{
				temp_Count = 0;

				/* 开始加热 */
				SystemStatus.bits.b1Status_Heat = 1;
				heat_Flag = 1;
			}
		}
		else
		{

			/* 外部不控制充电管 */
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;

			/* 关闭加热 */
			SystemStatus.bits.b1Status_Heat = 0;
			temp_Count = 0;
		}
	}

	MCUO_RELAY_HEAT = SystemStatus.bits.b1Status_Heat;
}

void InitHeat_Cool(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	// PA12加热
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

// 加热冷凝电流均无法检测
void App_Heat_Cool_Ctrl(void)
{
	Heat_Control(); // 关闭和电流不挂钩(证明开始使用了)，打开必须挂钩，不然就长期在那里耗，把电池耗到低压保护。
}
