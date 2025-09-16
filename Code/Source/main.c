#include "main.h"
#include "bsp.h"

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
#ifdef __FUNC__HEAT__
		App_Heat_Cool_Ctrl();
#endif // DEBUG
		App_ChargerLoad_Det();

		App_FlashUpdateDet();
		App_LogRecord();
		App_ProID_Deal();

		Feed_IWatchDog;
#endif
	}
}

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
	// Init_IWDG();
#else
	InitDelay();
	IsSleepStartUp();
	InitIO();
	//__delay_ms(1000);
	InitTimer();
	InitSystemWakeUp();
	InitE2PROM(); // 内部EEPROM，不需要初始化
	InitSci();
	InitADC();

	InitData_SOC();
	Init_RTC(); // 必须放在EEPROM读完数据后面！
				// 如果用了LSE_32KHz的口，暂时先关掉RTC，这个的配置使IO口配置失效不可控
#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif // DEBUG
	InitMosRelay_DOx();
	Init_ChargerLoad_Det();

	InitAFE1();

	MCU_GetResetType();

#ifdef wdog_enable
	Init_IWDG();
#endif // !1
#ifdef __test__
	DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif
	// DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif
}

void InitVar(void)
{
	InitSystemMonitorData_EEPROM();
	// 这样写就不用管前面到底读出来还是复位了(在EEPROM很多个地方算)
	SeriesNum = OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 844 << 10) / OtherElement.u16Sys_CS_Res / 100; // 算CS检流电阻

	LogRecord_Flag.bits.Log_StartUp = 1;
	SystemStatus.bits.b1StartUpBMS = 0;
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
	App_WakeUpAFE();
	__delay_ms(20);
}

void InitSci(void)
{
	InitUSART_CommonUpper();
}

void App_Sci(void)
{
	App_CommonUpper();
}
