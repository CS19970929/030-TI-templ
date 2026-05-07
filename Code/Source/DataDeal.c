#include "main.h"

UINT8 u8IICFaultcnt1 = 0;
UINT8 u8WakeCnt1 = 0;
UINT8 u8IICFaultcnt2 = 0;
UINT8 u8WakeCnt2 = 0;

UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];

UINT16 CopperLoss[CompensateNUM]; // uΩ
UINT16 CopperLoss_Num[CompensateNUM];

UINT32 g_u32CS_Res_AFE = 0;

struct OTHER_ELEMENT OtherElement;

void Init_Registers(UINT8 num)
{
	UINT8 j;
	switch (num)
	{
	case 0:
		for (j = 0; j < 40; j++)
		{
			*(&(Registers_AFE1.VCell1.VCell1Byte.VC1_HI) + j) = 0;
		}
		break;

	case 1:
		break;

	default:
		break;
	}
	// CHG_OFF;
	// DSG_OFF;
}

void DataLoad_CellVolt_Test(void)
{
#if 0
	MCUO_ENO_DO1 = 1;
	g_stCellInfoReport.u16VCell[31] = (UINT16)pow(2.718,6.12);
	MCUO_ENO_DO1 = 0;
	
	g_stCellInfoReport.u16VCell[15] = 'd';
#endif

#if 0
	//g_stCellInfoReport.u16VCell[31] = SeriousFaultFlag;
	g_stCellInfoReport.u16VCell[30] = ChgValue;
	g_stCellInfoReport.u16VCell[29] = DsgValue;
#endif

#if 0
	UINT8 	i;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp = 1;

	g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = 1;
	
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1TempDeltaBig = 1;

	g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp = 1;
	g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp = 1;

	for(i = 0; i < 24; ++i) {
		*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + i) = i + 1;
	}
#endif

}

// 这里排列好就行，不需要电池位号映射表。>61000为不用
// 经过验算，AFE1校准一次，然后本身再校准一次叠加是可以的。不需要确定某一个KB值的做法。
// 假设先确定用AFE1还是本身的KB的话，会出现问题。如下：
// 假设需要整体校准，行，AFE1先行，然后发现某几串出问题，继续使用本身KB值，然后本身KB值需要同步前面AFE1的KB值一起算才行
// 如果又变成单独使用本身KB值校准，出现错误。
void DataLoad_CellVolt(void)
{
	UINT8 i;
	INT32 t_i32temp;

	for (i = 0; i < SeriesNum; ++i)
	{
		t_i32temp = (UINT32)g_stBq769x0_Read_AFE1.u16VCell[SeriesSelect_AFE1[SeriesNum - 1][i]];
		if (g_u16CalibCoefK[VOLT_AFE1] != 1024 || g_i16CalibCoefB[VOLT_AFE1] != 0)
		{
			t_i32temp = ((t_i32temp * g_u16CalibCoefK[VOLT_AFE1]) >> 10) + g_i16CalibCoefB[VOLT_AFE1];
		}
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[i]) >> 10) + g_i16CalibCoefB[i];
		t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
		g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
	}

	if (SeriesNum < 32)
	{
		for (i = SeriesNum; i < 32; ++i)
		{
			g_stCellInfoReport.u16VCell[i] = 61001;
		}
	}

}

void DataLoad_CellVoltMaxMinFind(void)
{
	UINT8 i;
	UINT16 t_u16VcellTemp;
	UINT16 t_u16VcellMaxTemp;
	UINT16 t_u16VcellMinTemp;
	UINT8 t_u8VcellMaxPosition;
	UINT8 t_u8VcellMinPosition;
	UINT32 u32VCellTotle;

	t_u16VcellMaxTemp = 0;
	t_u16VcellMinTemp = 0x7FFF;
	t_u8VcellMaxPosition = 0;
	t_u8VcellMinPosition = 0;
	u32VCellTotle = 0;

	for (i = 0; i < SeriesNum; i++)
	{
		t_u16VcellTemp = g_stCellInfoReport.u16VCell[i];
		u32VCellTotle += g_stCellInfoReport.u16VCell[i];
		if (t_u16VcellMaxTemp < t_u16VcellTemp)
		{
			t_u16VcellMaxTemp = t_u16VcellTemp;
			t_u8VcellMaxPosition = i;
		}
		if (t_u16VcellMinTemp > t_u16VcellTemp)
		{
			t_u16VcellMinTemp = t_u16VcellTemp;
			t_u8VcellMinPosition = i;
		}
	}

	// 单片机读总压
	// u32VCellTotle = ((g_i32ADCResult[ADC_VBC]*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
	// AFE读总压
	// u32VCellTotle = ((g_stBq769x0_Read_AFE1.u32VBat*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
	// 所有单节电池电压加起来
	u32VCellTotle = ((u32VCellTotle * g_u16CalibCoefK[VOLT_VBUS]) >> 10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS] * 1000;

	g_stCellInfoReport.u16VCellTotle = (UINT16)((u32VCellTotle * 1638 >> 14) & 0xFFFF); // 除以10
	g_stCellInfoReport.u16VCellMax = t_u16VcellMaxTemp;									// max cell voltage
	g_stCellInfoReport.u16VCellMin = t_u16VcellMinTemp;									// min cell voltage
	g_stCellInfoReport.u16VCellDelta = t_u16VcellMaxTemp - t_u16VcellMinTemp;			// delta cell voltage
	g_stCellInfoReport.u16VCellMaxPosition = t_u8VcellMaxPosition + 1;					// max cell voltage
	g_stCellInfoReport.u16VCellMinPosition = t_u8VcellMinPosition + 1;					// min cell voltage
}

/*这个是数据溢出的问题，其次是>>这个的优先级和别的符号优先级的问题
  运算符优先级太混乱导致数据溢出的问题
   (UINT16)(t_i32temp/100) 和
	(UINT16)(t_i32temp)/100不一样
*/
void DataLoad_Temperature(void)
{
	UINT8 i;
	INT32 t_i32temp;
	UINT8 Select;

	if (SeriesNum > 10)
	{
		Select = 3;
	}
	else if (SeriesNum > 6)
	{
		Select = 2;
		g_stCellInfoReport.u16Temperature[2] = 0;
	}
	else
	{
		Select = 1;
		g_stCellInfoReport.u16Temperature[1] = 0;
		g_stCellInfoReport.u16Temperature[2] = 0;
	}

	// Select = 1;
	for (i = 0; i < Select; i++)
	{
		t_i32temp = (INT32)g_stBq769x0_Read_AFE1.u16TempBat[i] / 10 - 40;
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP1 + i]) + g_i16CalibCoefB[MDL_TEMP1 + i]) >> 10;
		g_stCellInfoReport.u16Temperature[i] = (UINT16)(t_i32temp * 10 + 400);
		Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[i]);
	}

#if 0
	// 环境温度1
	t_i32temp = g_i32ADCResult[ADC_TEMP_EV1] / 10 - 40; // 放大1000倍和B值对应的意思
	t_i32temp = -40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1]) >> 10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
#endif

	// 环境温度2
	t_i32temp = g_i32ADCResult[ADC_TEMP_EV2] / 10 - 40; // 放大1000倍和B值对应的意思
	t_i32temp = -40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV2]) + g_i16CalibCoefB[MDL_TEMP_ENV2]) >> 10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP2] = (UINT16)(t_i32temp * 10 + 400);

	// 环境温度3
	t_i32temp = -40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV3]) + g_i16CalibCoefB[MDL_TEMP_ENV3]) >> 10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP3] = (UINT16)(t_i32temp * 10 + 400);

	// MOS温度为散热片温度
	// 取两者最大值
	t_i32temp = g_i32ADCResult[ADC_TEMP_MOS1];
	t_i32temp = t_i32temp / 10 - 40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_MOS1]) + g_i16CalibCoefB[MDL_TEMP_MOS1]) >> 10;
	g_stCellInfoReport.u16Temperature[MOS_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[MOS_TEMP1]);
}

void DataLoad_TemperatureMaxMinFind(void)
{
	UINT8 i;
	UINT16 t_u16VcellTemp;
	UINT16 t_u16VcellMaxTemp;
	UINT16 t_u16VcellMinTemp;
	t_u16VcellMaxTemp = 0;
	t_u16VcellMinTemp = 0x7FFF;

	// 如果是两个环境温度，则改为8便可
	for (i = 0; i < TEMP_NUM - 1; i++)
	{ // 默认只有一个环境温度，纳入计算
		if (g_stCellInfoReport.u16Temperature[i] == 0)
		{			  // 这段代码什么意思，断了就不判断吗？
			continue; // 有的，则必定会被赋值，要么-29摄氏度。
		} // 空的，则就是默认刚上电的值0
		t_u16VcellTemp = g_stCellInfoReport.u16Temperature[i];
		if (t_u16VcellMaxTemp < t_u16VcellTemp)
		{
			t_u16VcellMaxTemp = t_u16VcellTemp;
		}
		if (t_u16VcellMinTemp > t_u16VcellTemp)
		{
			t_u16VcellMinTemp = t_u16VcellTemp;
		}
	}

	g_stCellInfoReport.u16TempMax = t_u16VcellMaxTemp; // max temp
	g_stCellInfoReport.u16TempMin = t_u16VcellMinTemp; // min temp
}

// 用AFE1还是AD采样后面再说
void DataLoad_Current(void)
{
	UINT8 temp_I;
	INT32 t_i32temp;

	// 这个要优化优化计算，大概率不用校准，校准的B值为Q10
	// g_stBq769x0_Read_AFE1.u16Current = 0x0999;
	// g_stBq769x0_Read_AFE1.u16Current = 0xFF00;
	if (g_stBq769x0_Read_AFE1.u16Current <= 0x8000)
	{
		// t_i32temp = (INT32)g_stBq769x0_Read.i16Current*8440/5000;	  //mA
		// t_i32temp = (INT32)g_stBq769x0_Read_AFE1.u16Current*CS_Res_AFE>>10;    //mA
		t_i32temp = (UINT32)g_stBq769x0_Read_AFE1.u16Current * g_u32CS_Res_AFE >> 10;
		temp_I = CurCHG;
	}
	else
	{
		// t_i32temp = ((INT32)(0xFFFF - ((INT16)g_stBq769x0_Read_AFE1.u16Current&0xFFFF) + 1))*CS_Res_AFE>>10; //mA
		t_i32temp = (UINT32)(0xFFFF - (g_stBq769x0_Read_AFE1.u16Current & 0xFFFF) + 1) * g_u32CS_Res_AFE >> 10; // mA
		temp_I = CurDSG;
	}

	if (temp_I == CurDSG)
	{
		/* 电流大于2a校准有b值 */
		if (t_i32temp > 2000)
		{
			t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_IDSG])) + (INT32)g_i16CalibCoefB[MDL_IDSG] * 1000; // B值是基于A为单位计算出来的
		}
		/* 电流小于2a校准无b值 */
		else
		{
			t_i32temp = ((t_i32temp * 1024));
		}
	}
	else
	{
		/* 电流大于2a校准有b值 */
		if (t_i32temp > 2000)
		{
			t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_ICHG])) + (INT32)g_i16CalibCoefB[MDL_ICHG] * 1000;
		}
		/* 电流小于2a校准无b值 */
		else
		{
			t_i32temp = ((t_i32temp * 1024));
		}
	}

	t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
	if (temp_I == CurDSG)
	{
		g_stCellInfoReport.u16Ichg = 0;
		g_stCellInfoReport.u16IDischg = (UINT16)(t_i32temp / 100 >> 10);
	}
	else
	{
		g_stCellInfoReport.u16Ichg = (UINT16)(t_i32temp / 100 >> 10);
		g_stCellInfoReport.u16IDischg = 0;
	}

	if (g_stCellInfoReport.u16Ichg <= 2)
	{
		g_stCellInfoReport.u16Ichg = 0;
	}
	if (g_stCellInfoReport.u16IDischg <= 2)
	{
		g_stCellInfoReport.u16IDischg = 0;
	}
#ifdef __VIRTURE_CURRENT__
	if (sys_time.isdebugenable == 1)
	{
		g_stCellInfoReport.u16Ichg = sys_time.CHG;
		g_stCellInfoReport.u16IDischg = sys_time.DSG;
	}
#endif
}

void MonitorAFE(UINT8 num, UINT8 Result)
{
	static UINT16 su16_Sleep_DelayT1 = 0;
	static UINT16 su16_Sleep_DelayT2 = 0;
	static UINT16 su16_Sleep_DelayT3 = 0;

	switch (num)
	{
	case 0:
		if (Result != 0)
		{
			++u8IICFaultcnt1;
			if (u8IICFaultcnt1 > 60)
			{ // 20次1s
				Init_Registers(num);
				u8IICFaultcnt1 = 0;
				System_ERROR_UserCallback(ERROR_AFE1); // 这里调用便可
			}
			if (u8IICFaultcnt1 == 30 && u8WakeCnt1 <= 20)
			{
				App_WakeUpAFE(); // 因为这两个会使两个AFE垮掉
				// MCUO_WAKEUP_AFE = !MCUO_WAKEUP_AFE;
				InitialisebqMaximo(DEVICE_ADDR_AFE1);
				// InitialisebqMaximo2(DEVICE_ADDR_AFE1);
				++u8WakeCnt1;
			}
			SystemStatus.bits.b1Status_AFE1 = 0;
		}
		else
		{
			if (u8IICFaultcnt1 > 0)
			{
				u8IICFaultcnt1--;
			}
			if (u8WakeCnt1 > 0)
			{
				u8WakeCnt1--;
			}

			if (u8IICFaultcnt1 == 0 && u8WakeCnt1 == 0)
			{
				SystemStatus.bits.b1Status_AFE1 = 1;
				// MCUO_WAKEUP_AFE = 0;
				// System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
			}
		}
		break;

	case 1:
		if (Result != 0)
		{
			++u8IICFaultcnt2;
			if (u8IICFaultcnt2 > 60)
			{
				Init_Registers(num);
				u8IICFaultcnt2 = 0;
				System_ERROR_UserCallback(ERROR_AFE2); // 这里调用便可
			}
			if (u8IICFaultcnt2 == 30 && u8WakeCnt2 <= 20)
			{
				App_WakeUpAFE(); // 因为这两个会使两个AFE垮掉
				// InitialisebqMaximo(DEVICE_ADDR_AFE1);
				// InitialisebqMaximo2(DEVICE_ADDR_AFE1);
				++u8WakeCnt2;
			}
			SystemStatus.bits.b1Status_AFE2 = 0;
		}
		else
		{
			if (u8IICFaultcnt2 > 0)
			{
				u8IICFaultcnt2--;
			}
			if (u8WakeCnt2 > 0)
			{
				u8WakeCnt2--;
			}
			SystemStatus.bits.b1Status_AFE2 = 1;
			// System_ERROR_UserCallback(ERROR_REMOVE_AFE2);
		}
		break;
	default:
		break;
	}

	if (System_ERROR_UserCallback(ERROR_STATUS_AFE1))
	{
		if (++su16_Sleep_DelayT1 >= 20 * 60 * 5)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT1 = 0;
			// Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
		}
	}
	else
	{
		su16_Sleep_DelayT1 = 0;
	}

	if (System_ERROR_UserCallback(ERROR_STATUS_AFE2))
	{
		if (++su16_Sleep_DelayT2 >= 20 * 60 * 5)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT2 = 0;
			// Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
		}
	}
	else
	{
		su16_Sleep_DelayT2 = 0;
	}

	// 暂时寄存这里
	if (System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM) || System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE))
	{
		if (++su16_Sleep_DelayT3 >= 20 * 60 * 5)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT3 = 0;
			// Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
		}
	}
	else
	{
		su16_Sleep_DelayT3 = 0;
	}
}

void test_Autocurrent_cycle(void)
{
	static uint8_t step = 0;
#if 1
	static uint16_t CHG_current = 450;
	static uint16_t DSG_current = 450;
#else
	static uint16_t CHG_current = 200;
	static uint16_t DSG_current = 400;
#endif

	switch (step)
	{
	case 0:
		if (g_stCellInfoReport.SocElement.u16Soc < 99)
		{
			step = 1;
			g_stCellInfoReport.u16Ichg = CHG_current;
			g_stCellInfoReport.u16IDischg = 0;
		}
		else
		{
			step = 1;
		}
		break;
	case 1:
	{
		if (g_stCellInfoReport.SocElement.u16Soc >= 99)
		{
			step = 2;
			g_stCellInfoReport.u16Ichg = 0;
			g_stCellInfoReport.u16IDischg = DSG_current;
		}
		break;
	}
	case 2:
		if (g_stCellInfoReport.SocElement.u16Soc <= 1)
		{
			step = 0;
		}
		break;
	default:
		break;
	}
}

void App_AFEGet(void)
{
	static UINT8 ts_u8TempSel = 0;

	// if (0 == g_st_SysTimeFlag.bits.b1Sys50msFlag )
	// {
	// 	return;
	// }
	if (0 == gu8_200msAccClock_Flag)
	{
		return;
	}

	if (u32E2P_Pro_VolCur_WriteFlag != 0 || u32E2P_Pro_Temp_WriteFlag != 0 || u32E2P_Pro_Other_WriteFlag != 0 || u32E2P_OtherElement1_WriteFlag != 0 || u8E2P_KB_WriteFlag != 0)
	{
		return;
	}

	if (FaultCnt_StartUp_First || FaultCnt_StartUp_Second || FaultCnt_StartUp_Third)
	{
		return;
	}

	// MonitorAFE(0, UpdateVoltageFromBqMaximo_Partition(DEVICE_ADDR_AFE1, ts_u8TempSel++));
	MonitorAFE(0, UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1));
	// if (ts_u8TempSel >= 4)
	// 	ts_u8TempSel = 0;

	DataLoad_CellVolt();
	// DataLoad_CellVolt_Test();
	DataLoad_CellVoltMaxMinFind();
	DataLoad_Temperature();
	DataLoad_TemperatureMaxMinFind();
	DataLoad_Current();
	// test_Autocurrent_cycle();
	App_SOC();
	App_BQ769X0_Monitor();

	gu8_200msAccClock_Flag = 0;
}
