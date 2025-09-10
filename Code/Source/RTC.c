#include "main.h"

struct RTC_ELEMENT RTC_time;
RTC_InitTypeDef RTC_InitStructure;
RTC_TimeTypeDef RTC_TimeStructure;
RTC_DateTypeDef RTC_DateStructure;
RTC_AlarmTypeDef RTC_AlarmStructure;

UINT8 RTC_ClockConfig(void)
{
	__IO UINT16 StartUpCounter = 0, HSEStatus = 0;
	UINT8 result = 0;
	PWR_BackupAccessCmd(ENABLE); // 允许访问RTC
	RCC_LSEConfig(RCC_LSE_ON);	 // 使能外部LSE晶振，RCC_LSE_Bypass旁路的意思应该是使能这个LSE时钟，但是单片机不用，外围电路用?
	do
	{
		HSEStatus = RCC_GetFlagStatus(RCC_FLAG_LSERDY);
		StartUpCounter++;
	} while ((HSEStatus == RESET) && (StartUpCounter < LSE_START_TIMEOUT)); // 等待到 LSE 预备

	if (StartUpCounter < LSE_START_TIMEOUT)
	{
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE); // 把RTC 时钟源配置为LSE
		RCC_RTCCLKCmd(ENABLE);					// 使能RTC时钟
		RTC_WaitForSynchro();					// 等待 RTC APB 寄存器同步
		// 分频设置，Flsi/((AsynchPrediv+1)(SynchPrediv+1)) = 1Hz = 1s
		RTC_InitStructure.RTC_AsynchPrediv = 99; // 异步分频，0x7F最大，拉满可以降低功耗
		RTC_InitStructure.RTC_SynchPrediv = 327; // 同步分频，LSE = 32768Hz
		RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
		result = 0;
	}
	else
	{
		// System_ERROR_UserCallback(ERROR_LSE);					//RTC错误单数为LSE出错
		RCC_LSICmd(ENABLE); // 使能 LSI 振荡
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
			; // 等待到 LSI 预备

		RCC->BDCR = 0x10000;		 // 这个是耕耘这边处理的，原因是开机就选定了LSE
		PWR_BackupAccessCmd(ENABLE); // 导致没法计数，不能唤醒，所以必须对RTC软复位一次
		RCC->BDCR = 0x8200;			 // 再次配置其时钟源才没问题

		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);	 // 把RTC 时钟源配置为LSI
		RCC_RTCCLKCmd(ENABLE);					 // 使能RTC时钟
		RTC_WaitForSynchro();					 // 等待 RTC APB 寄存器同步
		RTC_InitStructure.RTC_AsynchPrediv = 99; // 异步分频，0x7F最大，拉满可以降低功耗
		RTC_InitStructure.RTC_SynchPrediv = 399; // 同步分频，LSI = 40KHz
		RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
		result = 1;
	}

	if (RTC_Init(&RTC_InitStructure) == ERROR)
	{
		result = 1;
	}
	return result;
}

UINT8 RTC_TimeConfig(void)
{
	UINT8 result = 0;

	RTC_DateStructure.RTC_Year = RTC_time.RTC_Time_Year;
	RTC_DateStructure.RTC_Month = RTC_time.RTC_Time_Month;
	RTC_DateStructure.RTC_Date = RTC_time.RTC_Time_Day;
	RTC_TimeStructure.RTC_H12 = RTC_H12_AM; // 24小时制
	RTC_TimeStructure.RTC_Hours = RTC_time.RTC_Time_Hour;
	RTC_TimeStructure.RTC_Minutes = RTC_time.RTC_Time_Minute;
	RTC_TimeStructure.RTC_Seconds = RTC_time.RTC_Time_Second;

	if (RTC_SetTime(RTC_Format_BIN, &RTC_TimeStructure) == ERROR ||
		RTC_SetDate(RTC_Format_BIN, &RTC_DateStructure) == ERROR)
	{
		// printf("\n\r>> !! RTC Set Time failed. !! <<\n\r");
		result = 1;
	}
	return result;
}

// 还没调好，不知道效果如何，中断就没搞定
void RTC_AlarmConfig(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;

	RTC_AlarmCmd(RTC_Alarm_A, DISABLE);					   // Disable the Alarm A
	RTC_AlarmStructure.RTC_AlarmTime.RTC_H12 = RTC_H12_AM; // 24小时制

#if 0
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours = RTC_time.RTC_Alarm_Hour + (OtherElement.u16Sleep_RTC_WakeUpTime/60);
	UPDNLMT16(RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours,23,0);
	
	//下面那个不会超过59
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = RTC_time.RTC_Alarm_Minute + (OtherElement.u16Sleep_RTC_WakeUpTime%60);
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = RTC_time.RTC_Alarm_Second;
#endif

#if 1
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours = RTC_time.RTC_Alarm_Hour + 0;
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = RTC_time.RTC_Alarm_Minute + 0;
	RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = RTC_time.RTC_Alarm_Second + 30;
#endif

	// Set the Alarm A
	RTC_AlarmStructure.RTC_AlarmDateWeekDay = 31;
	RTC_AlarmStructure.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date; // 选择0-31天，不是星期数
	RTC_AlarmStructure.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;			   // 0-31天对Alarm A 无影响
	RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);			   // 这个有问题，按datasheet应该是BCD格式，但是库函数要BIN才能变为BCD
																			   // RTC_Alarm_A必须要有才行，不知道有什么用
	// RTC_AlarmShow();
	RTC_ITConfig(RTC_IT_ALRA, ENABLE); // Enable the RTC Alarm A Interrupt
	RTC_AlarmCmd(RTC_Alarm_A, ENABLE); // Enable the alarm	A

	EXTI_ClearITPendingBit(EXTI_Line17); // 外部上升沿中断，能从standby模式唤醒
	EXTI_InitStructure.EXTI_Line = EXTI_Line17;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn; // Enable the RTC Alarm Interrupt
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void Init_RTC_Clock(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外设时钟，待机模式，RTC，看门狗
	RTC_ClockConfig();									// RTC时钟配置

	if (RTC_ReadBackupRegister(RTC_BKP_DR0) != BKP_VALUE)
	{ // 读取备份里面的值是否被写过。
		if (0 == RTC_TimeConfig())
		{
			RTC_WriteBackupRegister(RTC_BKP_DR0, BKP_VALUE);
		}
		else
		{
			// 配置失败操作
		}
		RTC_AlarmConfig();
	}
	else
	{ // 以下这段话需要吗？
		if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
		{ // 这是啥
		  //("\r\n Power On Reset occurred....\n\r");
		  //++RTC_Faultcnt;
		}
		else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
		{ // 这是啥
		  // printf("\r\n External Reset occurred....\n\r");
		}
		RTC_ClearFlag(RTC_FLAG_ALRAF); // Clear the RTC Alarm Flag
		EXTI_ClearITPendingBit(EXTI_Line17);
		// RTC_TimeShow();				//Display the RTC Time and Alarm，这个后面会用到
		// RTC_AlarmShow();
	}
}

void Init_RTC_HaltWakeUp(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外设时钟，待机模式，RTC，看门狗
	RTC_ClockConfig();									// RTC时钟配置
}

void Init_RTC(void)
{
	UINT8 i;
	Init_RTC_HaltWakeUp();

#ifdef _HALT_ACTIVE_MODE
	Init_RTC_HaltWakeUp();
#else
// Init_RTC_Clock();
#endif

	// 默认为0
	for (i = 0; i < 12; i++)
	{
		*(&RTC_time.RTC_Time_Year + i) = 0;
	}
}

void App_RTC(void)
{
#ifndef _HALT_ACTIVE_MODE
	static UINT8 u8RTCcnt = 0;
	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag1)
	{
		return;
	}
	if (++u8RTCcnt < 5)
	{
		return;
	}
	u8RTCcnt = 0;
	RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
	RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
	RTC_GetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);
#endif
}

void RTC_IRQHandler(void)
{
	if (RTC_GetITStatus(RTC_IT_ALRA) != RESET)
	{
		RTC_ClearITPendingBit(RTC_IT_ALRA);
	}
	EXTI_ClearITPendingBit(EXTI_Line17);

	if (FlashReadOneHalfWord(FLASH_ADDR_WAKE_TYPE) != FLASH_VALUE_WAKE_RTC)
	{
		FlashWriteOneHalfWord(FLASH_ADDR_WAKE_TYPE, FLASH_VALUE_WAKE_RTC);
	}
}
