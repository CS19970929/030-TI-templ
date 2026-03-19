#include "main.h"
#include "bsp.h"
#include "SleepManager_LowPower.h"

UINT8 SeriesNum = 16;

// ��ͬ����ά���ı���
const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 1��
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 2��
	{0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 3   76920
	{0, 1, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 4   76920
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 5   76920
	//{0 ,1 ,2 ,3 ,4 ,15,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0, 0},   	//6   76920 + AD	//��6��ӳ�䵽16��������˵�Ĺ�Ȼ����
	{0, 1, 4, 5, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 6   76930 		//����˵��û���ˣ���Ϊ930����������
	{0, 1, 2, 4, 5, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 7   76930
	{0, 1, 2, 4, 5, 6, 7, 9, 0, 0, 0, 0, 0, 0, 0, 0},	   // 8   76930
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 0, 0, 0, 0, 0, 0, 0},	   // 9   76930
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},	   // 10  76930
	{0, 1, 3, 4, 5, 6, 7, 9, 10, 11, 14, 0, 0, 0, 0, 0},   // 11  76940
	{0, 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 0, 0, 0, 0},  // 12  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 14, 0, 0, 0},  // 13  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 0, 0},  // 14  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15  76940
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16  76940 + AD	//����汾������16����
};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);

int main(void)
{
	InitDevice(); // ��ʼ�����裬������������λ����Ҫ����һ�£����ڻ���ȥ��
	InitVar();	  // ��ʼ������

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
		App_SleepDeal(); // ����App_MOS_Relay_Control()����
		
		// 低功耗处理（1秒调用一次）
		if (g_st_SysTimeFlag.bits.b1Sys1000msFlag1) {
			g_st_SysTimeFlag.bits.b1Sys1000msFlag1 = 0;
			Sleep_Process();
		}
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
	SystemInit(); // ֱ�ӵ��þͿ����ˡ�
				  // A����reset�������ã�ʹ��HSI(8MHz)���С�resetĬ����ʹ��HSI���С�
				  // B������SetSysClock()��Ĭ��ʹ��8MHz�ⲿ����Ȼ������Ƶ�������Ƶ������ܳ���48MHz(��ʹ��12MHz�����Ը�Ϊ4��Ƶ)
				  // C�������Ƶʧ�ܣ����и�else������Ҹģ����һЩ��־λ����Ŀǰû��
				  // D�����Ӵ�����ֹͣģʽ���ػ�����ϵͳʱ�ӵ�HSE ������������ʱ����λ��Ӳ����������HSI ������
				  // E������֮�⣬�������ģʽҪ���ⲿ���񣬻���������HSI���У�Ȼ�����ⲿ����ͱ�Ƶ��
				  // F������һ���л�ʱ�ӵĺ�����SystemCoreClockUpdate()��ʹ�����������˽⡣
				  // G���ⲿ�����޸ĵĻ�������ͷ�ļ�HSE_VALUE��ֵ����Ӱ�촮�ڲ����ʡ�
				  // H�������ʹ��HSE��ֱ�Ӻ����ⲿ�����ɣ�ϵͳ��Ĭ�Ϸ���HSI��SystemCoreClock�Զ���Ϊ8M�������۲촮�ڲ����ʺ�I2CƵ���Ƿ��������
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
	InitE2PROM(); // �ڲ�EEPROM������Ҫ��ʼ��
	InitSci();
	InitADC();

	InitData_SOC();
	Init_RTC(); // �������EEPROM�������ݺ��棡
				// �������LSE_32KHz�Ŀڣ���ʱ�ȹص�RTC�����������ʹIO������ʧЧ���ɿ�
#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif // DEBUG
	InitMosRelay_DOx();
	Init_ChargerLoad_Det();

	MCU_GetResetType();
	LoadParam();

	InitAFE1();
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
	// ����д�Ͳ��ù�ǰ�浽�浽�׶��������Ǹ�λ��(��EEPROM�ܶ���ط���)
	SeriesNum = OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 844 << 10) / OtherElement.u16Sys_CS_Res / 100; // ��CS��������

	LogRecord_Flag.bits.Log_StartUp = 1;
	SystemStatus.bits.b1StartUpBMS = 0;
	
	// 初始化低功耗管理器
	Sleep_Init(NULL);
}

void App_WakeUpAFE(void)
{
	MCUO_WAKEUP_AFE = 0;
	__delay_ms(1);
	MCUO_WAKEUP_AFE = 1;
	__delay_ms(5); // max 2ms��tBOOT_max
	MCUO_WAKEUP_AFE = 0;
	__delay_ms(10); // �Լ죬10ms��tBOOTREADY
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
