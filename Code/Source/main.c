#include "main.h"

UINT8 SeriesNum = 16;

// 不同串数维护的表格
const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 1串
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 2串
	{0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 3   76920
	{0, 1, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 4   76920
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 5   76920
	//{0 ,1 ,2 ,3 ,4 ,15,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0, 0},   	//6   76920 + AD	//第6串映射到16串，刘总说的果然有用
	{0, 1, 4, 5, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 6   76930 		//刘总说的没用了，改为930，外扩怕了
	{0, 1, 2, 4, 5, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 7   76930
	{0, 1, 2, 4, 5, 6, 7, 9, 0, 0, 0, 0, 0, 0, 0, 0},	   // 8   76930
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 0, 0, 0, 0, 0, 0, 0},	   // 9   76930
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},	   // 10  76930
	{0, 1, 3, 4, 5, 6, 7, 9, 10, 11, 14, 0, 0, 0, 0, 0},   // 11  76940
	{0, 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 0, 0, 0, 0},  // 12  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 14, 0, 0, 0},  // 13  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 0, 0},  // 14  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16  76940 + AD	//这个版本不会有16串了
};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);
void MCU_ClockTest(void);

int main(void)
{
	InitDevice(); // 初始化外设，这两个函数的位置需要斟酌一下，现在换回去先
	InitVar();	  // 初始化变量

	while (1)
	{
#if (defined _DEBUG_CODE)
		App_SysTime();
		// App_NormalSleepTest();
		Feed_IWatchDog;

#else
		App_SysTime();
		App_Sci();
		App_AFEGet();
		App_BQ769X0_Monitor();
		App_WarnCtrl();
		App_MOS_Relay_Ctrl();
		App_AnlogCal();
		App_E2promDeal();
		// App_RTC();
		App_CellBalance();
		App_SOC();
		App_SleepDeal(); // 放在App_MOS_Relay_Control()后面
		App_Heat_Cool_Ctrl();
		App_ChargerLoad_Det();

		App_FlashUpdateDet();
		App_LogRecord();
		App_ProID_Deal();

		Feed_IWatchDog;
#endif
	}
}

// 这个初始化函数很容易出问题
void InitDevice(void)
{
	SystemInit(); // 直接调用就可以了。
				  // A，先reset所有配置，使用HSI(8MHz)运行。reset默认是使用HSI运行。
				  // B，调用SetSysClock()，默认使用8MHz外部晶振，然后六倍频输出，倍频输出不能超过48MHz(我使用12MHz，所以改为4倍频)
				  // C，如果倍频失败，会有个else语句让我改，输出一些标志位，我目前没改
				  // D，当从待机和停止模式返回或用作系统时钟的HSE 振荡器发生故障时，该位由硬件置来启动HSI 振荡器。
				  // E，言下之意，进入待机模式要关外部晶振，回来，先用HSI运行，然后开启外部晶振和倍频。
				  // F，还有一个切换时钟的函数，SystemCoreClockUpdate()，使用条件后面了解。
				  // G，外部晶振修改的话，改主头文件HSE_VALUE的值，会影响串口波特率。
				  // H，如果不使用HSE，直接焊掉外部晶振便可，系统会默认返回HSI，SystemCoreClock自动改为8M，后续观察串口波特率和I2C频率是否符合需求
	Init_IAPAPP();

#if (defined _DEBUG_CODE)
	InitIO();
	InitDelay();
	InitTimer();
	InitSystemWakeUp();
	// MCU_ClockTest();

	// Init_IWDG();

#else
	IsSleepStartUp();
	InitIO();
	InitDelay();
	//__delay_ms(1000);
	InitTimer();
	InitSystemWakeUp();
	InitE2PROM(); // 内部EEPROM，不需要初始化
	InitSci();
	InitADC();

	InitData_SOC();
	Init_RTC(); // 必须放在EEPROM读完数据后面！
				// 如果用了LSE_32KHz的口，暂时先关掉RTC，这个的配置使IO口配置失效不可控
	// Init_I2CSlaver();
	InitHeat_Cool();
	InitMosRelay_DOx();
	Init_ChargerLoad_Det();

	// InitPWM();			//关于CBC的输出

	InitAFE1();

	Init_IWDG();
#endif
}

void InitVar(void)
{
	UINT16 i;

	// SystemMonitorResetData_EEPROM();							//这个函数的初始化默认需求功能修改了，要修改EEPROM的上电标志位
	InitSystemMonitorData_EEPROM();
	MCUO_RELAY_PRE = SystemStatus.bits.b1Status_Relay_PRE; // 选择，需要的在这里赋值
	MCUO_RELAY_MAIN = SystemStatus.bits.b1Status_Relay_MAIN;

	// Switch功能
	// Switch_OnOFF_Func.all = 0;
	// InitSwitchData_EEPROM();

	// 总系统错误监控系统初始化
	for (i = 0; i < ERROR_NUM; ++i)
	{
		//*(&System_ErrFlag.u8ErrFlag_Com_AFE1+i)  =  0;		//有病，这样写就把错误清除了
	}

	// 保护标志位初始化
	g_stCellInfoReport.unMdlFault_First.all = 0;
	g_stCellInfoReport.unMdlFault_Second.all = 0;
	g_stCellInfoReport.unMdlFault_Third.all = 0;
	// 当次保护记录初始化
	FaultPoint_First = 0;
	FaultPoint_Second = 0;
	FaultPoint_Third = 0;

	FaultPoint_First2 = 0;
	FaultPoint_Second2 = 0;
	FaultPoint_Third2 = 0;
	for (i = 0; i < Record_len; ++i)
	{
		Fault_record_First[i] = 0;
		Fault_record_Second[i] = 0;
		Fault_record_Third[i] = 0;

		Fault_record_First2[i] = 0;
		Fault_record_Second2[i] = 0;
		Fault_record_Third2[i] = 0;
	}
	Fault_Flag_Fisrt.all = 0;
	Fault_Flag_Second.all = 0;
	Fault_Flag_Third.all = 0;

	// 继电器驱动开启初始化							//不打算放在这里
	// RelayCtrl_Command = RELAY_PRE_DET;	//不打算放在这里
	// HeatCtrl_Command = ST_HEAT_DET_SELF;
	// CoolCtrl_Command = ST_COOL_DET_SELF;

	// 休眠相关

	// 这样写就不用管前面到底读出来还是复位了(在EEPROM很多个地方算)
	SeriesNum = OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 844 << 10) / OtherElement.u16Sys_CS_Res / 100; // 算CS检流电阻

	LogRecord_Flag.bits.Log_StartUp = 1;
}

void App_WakeUpAFE(void)
{
	MCUO_WAKEUP_AFE = 0;
	__delay_ms(1);
	MCUO_WAKEUP_AFE = 1;
	__delay_ms(5); // max 2ms，tBOOT_max
	MCUO_WAKEUP_AFE = 0;
	__delay_ms(10); // 自检，10ms，tBOOTREADY
}

void InitSystemWakeUp(void)
{
	// MCUO_SD_DRV_CHG = 0;

	// MCUO_PWSV_STB = 0;

	// MCUO_PWSV_LDO = 1;
	// MCUO_PWSV_CTR = 1;

	App_WakeUpAFE();
	MCUO_BEL_EN = 1;

	MCUO_AFE_ALARM = 1;

	// MCUO_PW_RS485_EN = 1;

	__delay_ms(20);
}

void InitSci(void)
{
	InitUSART_CommonUpper();
	InitUSART_UartClient();
}

void App_Sci(void)
{
	App_CommonUpper();
	App_UartClient_Updata();
}

void MCU_ClockTest(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_DBGMCU, ENABLE);

	/*!< Configure sEE_I2C pins: SDA */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_0); // 这个AF选项找芯片手册非reg版

	RCC->CFGR |= RCC_CFGR_MCO_SYSCLK;
	// RCC->CFGR |= RCC_CFGR_MCO_HSE;
}
