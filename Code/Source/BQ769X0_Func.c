#include "main.h"

uint8_t aaa11 = 0;

void BQ769X0_DriverMos_Ctrl(GPIO_Type Type, UINT8 OnOFF)
{
	switch (Type)
	{
	case GPIO_PreCHG:
		// SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS = OnOFF;
		break;

	case GPIO_CHG:
		Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON = OnOFF;
		break;

	case GPIO_DSG:
		Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON = OnOFF;
		break;

	default:
		break;
	}

	I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, Registers_AFE1.SysCtrl2.SysCtrl2Byte);
}

void BQ769X0_Deal_OC_SC(void)
{
	static UINT8 su8_SC_Flag = 0;
	static UINT8 su8_OC_Flag = 0;

	switch (su8_SC_Flag)
	{
	case 0:
		if (Registers_AFE1.SysStatus.StatusBit.SCD)
		{
			System_ERROR_UserCallback(ERROR_CBC_DSG);	  // 报错
			System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0; // 关掉驱动，其会自动关DSG，CHG需要手动关。
														  // 手动关后激活负载移除检测功能
			su8_SC_Flag = 1;							  // 跳到下一级处理
		}
		break;

	case 1:
		ChargerLoad_Func.bits.b1OFFDriver_CBC = 1; // 确保驱动关闭，AFE短路保护消除后，启动单片的公版CBC恢复程序
		// 直接过去就行，System_OnOFF_Func.bits.b1OnOFF_MOS_Relay已经被置零了，先模拟前端恢复也行。
		su8_SC_Flag = 2;

		// I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL1, &(Registers_AFE1.SysCtrl1.SysCtrl1Byte));
		// //负载移除了
		// if(!Registers_AFE1.SysCtrl1.SysCtrl1Bit.LOAD_PRESENT) {
		// 	su8_SC_Flag = 2;
		// }
		break;

	case 2: // 恢复操作
		Registers_AFE1.SysStatus.StatusBit.SCD = 1;
		I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, Registers_AFE1.SysStatus.StatusByte);
		Registers_AFE1.SysStatus.StatusBit.SCD = 0;

		// 用了单片机本身的恢复程序，则不能用这个
		// System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1;
		su8_SC_Flag = 0;
		break;

	default:
		break;
	}

	switch (su8_OC_Flag)
	{
	case 0:
		if (Registers_AFE1.SysStatus.StatusBit.OCD)
		{
			System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0; // 关掉驱动，其会自动关DSG，CHG需要手动关。
														  // 手动关后激活负载移除检测功能
			System_ERROR_UserCallback(ERROR_BLUETOOTH);
			su8_OC_Flag = 1; // 跳到下一级处理
		}
		break;

	case 1:
		ChargerLoad_Func.bits.b1OFFDriver_CBC = 1; // 确保驱动关闭，AFE短路保护消除后，启动单片的公版CBC恢复程序
		// 直接过去就行，System_OnOFF_Func.bits.b1OnOFF_MOS_Relay已经被置零了，先模拟前端恢复也行。
		su8_OC_Flag = 2;

		// I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL1, &(Registers_AFE1.SysCtrl1.SysCtrl1Byte));
		// //负载移除了
		// if(!Registers_AFE1.SysCtrl1.SysCtrl1Bit.LOAD_PRESENT) {
		// 	su8_OC_Flag = 2;
		// }
		break;

	case 2: // 恢复操作
		Registers_AFE1.SysStatus.StatusBit.OCD = 1;
		I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, Registers_AFE1.SysStatus.StatusByte);
		Registers_AFE1.SysStatus.StatusBit.OCD = 0; // 复原

		// 用了单片机本身的恢复程序，则不能用这个
		//  System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1;
		su8_OC_Flag = 0;
		break;

	default:
		break;
	}
}

// 2，不休眠
// 1，进入L2。
// 0，进入L1.
UINT8 AFE_SleepMode_Judge(void)
{
	UINT8 result = 0;

	// 过压不能进入RTC休眠，别的都可。
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp)
	{
		result = 2;
	}
	// 过放等进入普通休眠(和深度休眠差不多)
	else if (										/*g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp\
													|| g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp\
															*/
			 g_stCellInfoReport.u16VCellMin <= 2500 /*g_stCellInfoReport.u16VCellTotle <= 2500\*/
													/*|| System_ERROR_UserCallback(ERROR_STATUS_AFE1)\
													|| System_ERROR_UserCallback(ERROR_STATUS_AFE2)\*/
			 || System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
	{ // 短路也整上去吧
		result = 1;
	}
	else
	{
		result = 0;
	}

	return result;
}

UINT16 Sys_FindProtectFilterMax_Real(void)
{
	UINT8 i;
	static UINT16 s_u16ProtectFilterMax = 0;
	if (0 != s_u16ProtectFilterMax)
	{
		return s_u16ProtectFilterMax + 2; // 推迟20ms
	}
	for (i = 0; i < 13; ++i)
	{
		if (*(&PRT_E2ROMParas.u16VcellOvp_Filter + 5 * i) > s_u16ProtectFilterMax)
		{
			s_u16ProtectFilterMax = *(&PRT_E2ROMParas.u16VcellOvp_Filter + 5 * i);
		}
	}
	return s_u16ProtectFilterMax + 2; // 推迟20ms
}

UINT16 gu8_WakeUp_Type = 0;
#if 0
void BQ769x0_SleepMode_Ctrl(void)
{
	static UINT8 su8_StartUp_Flag = 0;
	static UINT16 su16_Delay_100msTCnt = 0;

	static UINT8 su8_SleepExtComCnt = 0;

	static UINT16 su16_RTC1_100msTCnt = 0;
	static UINT16 su16_Normal1_100msTCnt = 0;
	static UINT16 su16_RTC2_100msTCnt = 0;
	static UINT16 su16_Normal2_100msTCnt = 0;

	UINT8 u8_CurComDelay_Flag = 0;

	// gu8_WakeUp_Type = FLASH_VALUE_WAKE_RTC;

	switch (su8_StartUp_Flag)
	{
	case 0:
		if (++su16_Delay_100msTCnt >= (Sys_FindProtectFilterMax_Real() / 100 + 1))
		{
			su16_Delay_100msTCnt = 0;
			su8_StartUp_Flag = 1;
		}
		break;

	case 1:
		if ((g_stCellInfoReport.u16Ichg > 2) || (g_stCellInfoReport.u16IDischg > 2))
		{
			u8_CurComDelay_Flag = 1;
		}
		else if (su8_SleepExtComCnt != RTC_ExtComCnt)
		{
			su8_SleepExtComCnt = RTC_ExtComCnt;
			u8_CurComDelay_Flag = 1;
		}

		if (u8_CurComDelay_Flag)
		{
			su16_RTC1_100msTCnt = 0;
			su16_RTC2_100msTCnt = 0;
			su16_Normal1_100msTCnt = 0;
			su16_Normal2_100msTCnt = 0;
			gu8_WakeUp_Type = FLASH_VALUE_WAKE_OTHER;
		}
		else
		{
			switch (gu8_WakeUp_Type)
			{
			case FLASH_VALUE_WAKE_RTC:
				if (AFE_SleepMode_Judge() == 1)
				{ // mos过温也纳入处理
					if (++su16_Normal1_100msTCnt > 60 * 10)
					{
						su16_Normal1_100msTCnt = 0;
						Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
					}
					if (su16_RTC1_100msTCnt)
						su16_RTC1_100msTCnt = 0;
				}
				else if (AFE_SleepMode_Judge() == 0)
				{
					if (++su16_RTC1_100msTCnt > 2)
					{
						su16_RTC1_100msTCnt = 0;
						Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
					}
					if (su16_Normal1_100msTCnt)
						su16_Normal1_100msTCnt = 0;
				}
				else
				{
					// 不进入休眠
				}
				aaa11 = 1;
				break;

			case FLASH_VALUE_WAKE_OTHER:
				if (AFE_SleepMode_Judge() == 1)
				{
					if (++su16_Normal2_100msTCnt >= 60 * 10)
					{
						su16_Normal2_100msTCnt = 0;
						Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
					}
					if (su16_RTC2_100msTCnt)
						su16_RTC2_100msTCnt = 0;
				}
				else if (AFE_SleepMode_Judge() == 0)
				{
					if (++su16_RTC2_100msTCnt >= 60 * 10)
					{
						su16_RTC2_100msTCnt = 0;
						Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
					}
					if (su16_Normal2_100msTCnt)
						su16_Normal2_100msTCnt = 0;
				}
				else
				{
					// 不进入休眠
				}

				aaa11 = 2;
				break;

			default:
				gu8_WakeUp_Type = FLASH_VALUE_WAKE_OTHER;
				FlashWriteOneHalfWord(FLASH_ADDR_WAKE_TYPE, FLASH_VALUE_WAKE_OTHER);
				break;
			}
		}
		break;

	default:
		break;
	}

	if (Sleep_Mode.bits.b1ForceToSleep_L1)
	{
		// 如果是低压，则会自动进入L2，和L3也无区别
	}

	aaaaaa1 = su16_RTC1_100msTCnt;
	aaaaaa2 = su16_Normal1_100msTCnt;
	aaaaaa3 = su16_RTC2_100msTCnt;
	aaaaaa4 = su16_Normal2_100msTCnt;
}
#endif

void App_BQ769X0_Monitor(void)
{
	UINT8 u8_Change = 0;
	static UINT8 su8_AfeFault_Tcnt = 0;
	static UINT8 su8_AlarmFault_Tcnt = 0;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag3)
	{
		return;
	}

	I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, &(Registers_AFE1.SysCtrl2.SysCtrl2Byte));
	I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, &(Registers_AFE1.SysStatus.StatusByte));

	// SystemStatus.bits.b1Status_MOS_PRE = ;

	// SystemStatus.bits.b1Status_MOS_CHG = Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON & Registers_AFE2.SysCtrl2.SysCtrl2Bit.CHG_ON;
	// SystemStatus.bits.b1Status_MOS_DSG = Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON & Registers_AFE2.SysCtrl2.SysCtrl2Bit.DSG_ON;

	SystemStatus.bits.b1Status_MOS_CHG = Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON;
	SystemStatus.bits.b1Status_MOS_DSG = Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON;

	// hold1 = Registers_AFE1.SysStatus.StatusByte;

	// Alarm引脚被外部拉低。
	// 这个是硬件问题，先不作处理。就算自己清除了意义也不大。可以让其进入休眠
	if (Registers_AFE1.SysStatus.StatusBit.OVRD_ALERT)
	{
		Registers_AFE1.SysStatus.StatusBit.OVRD_ALERT = 1;
		System_ERROR_UserCallback(ERROR_SPI);
		if (++su8_AlarmFault_Tcnt >= 4 * 60)
		{
			su8_AlarmFault_Tcnt = 0;
			// Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
		}
	}
	// AFE内部错误处理
	if (Registers_AFE1.SysStatus.StatusBit.DEVICE_XREADY)
	{
		if (++su8_AfeFault_Tcnt >= 20)
		{
			su8_AfeFault_Tcnt = 0;
			Registers_AFE1.SysStatus.StatusBit.DEVICE_XREADY = 1;
			u8_Change = 1;
		}
		System_ERROR_UserCallback(ERROR_UPPER);
	}
	else
	{
		System_ERROR_UserCallback(ERROR_REMOVE_UPPER);
	}

	if (Registers_AFE1.SysStatus.StatusBit.OV)
	{
		// todo
		if (g_stCellInfoReport.u16VCellMax < OVPThreshold)
		{
			Registers_AFE1.SysStatus.StatusBit.OV = 1;
			u8_Change = 1;
		}
		System_ERROR_UserCallback(ERROR_WIFI);
	}
	else
	{
		System_ERROR_UserCallback(ERROR_REMOVE_WIFI);
	}
	if (Registers_AFE1.SysStatus.StatusBit.UV)
	{
		if (g_stCellInfoReport.u16VCellMin > UVPThreshold)
		{
			Registers_AFE1.SysStatus.StatusBit.UV = 1;
			u8_Change = 1;
		}
		System_ERROR_UserCallback(ERROR_BLUETOOTH);
	}
	else
	{
		System_ERROR_UserCallback(ERROR_REMOVE_BLUETOOTH);
	}

	BQ769X0_Deal_OC_SC();

	if (u8_Change)
	{
		// I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, Registers_AFE1.SysCtrl2.SysCtrl2Byte);
		// I2CWriteRegisterByteWithCRC2(DEVICE_ADDR_AFE1, SYS_CTRL2, Registers_AFE2.SysCtrl2.SysCtrl2Byte);
		I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, Registers_AFE1.SysStatus.StatusByte);
	}

	// BQ769x0_SleepMode_Ctrl();
}
