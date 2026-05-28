#include "main.h"
#include "gan_huang_guan_logi.h"

volatile union SLEEP_MODE Sleep_Mode; // 用于外部控制进入休眠标志�?
enum SLEEP_STATUS Sleep_Status = SLEEP_HICCUP_SHIFT;

UINT8 gu8_SleepStatus = 0;
UINT8 RTC_ExtComCnt = 0;

static UINT8 s_sleep_wakeup_by_di1 = 0;
uint8_t reset_sleep_state = 0;
#define DI1_SOC_PREVIEW_WAKE_10MS ((UINT16)1) // PC13�պϺ󾡿컽�ѵ�����Ԥ��̬

static UINT8 IsPA0WakeupActive(void)
{
	return (UINT8)PORT_IN_GPIOA->bit0;
}

static UINT8 IsDI1Pressed(void)
{
	// PC13���رպ�Ϊ�͵�ƽ����EXTI13�½��ػ��ѱ���һ�£�
	return (UINT8)(MCUI_ENI_DI1 == 0);
}

static void SleepDeal_ClearDi1Wakeup(void)
{
	s_sleep_wakeup_by_di1 = 0;
	WakeDisplayState_Clear();
	EXTI_ClearITPendingBit(EXTI_Line13);
}

static UINT8 IsSleepWakeupValid(void)
{
	UINT16 hold_cnt = 0;

	if (IsPA0WakeupActive())
	{
		if (is_open_gan1())
		{
			s_sleep_wakeup_by_di1 = 0;
			WakeDisplayState_Clear();
			return 1;
		}
		return 0;
	}

	if (!IsDI1Pressed())
	{
		return 0;
	}

	if (!is_open_gan2())
	{
		SleepDeal_ClearDi1Wakeup();
		return 0;
	}

	while (IsDI1Pressed())
	{
		if (IsPA0WakeupActive() && is_open_gan1())
		{
			s_sleep_wakeup_by_di1 = 0;
			WakeDisplayState_Clear();
			return 1;
		}

		if (!is_open_gan2())
		{
			SleepDeal_ClearDi1Wakeup();
			return 0;
		}

		__delay_ms(10);
		if (++hold_cnt >= DI1_SOC_PREVIEW_WAKE_10MS)
		{
			s_sleep_wakeup_by_di1 = 1;
			WakeDisplay_RequestSocPreview();
			return 1;
		}
	}

	return 0;
}

// 通�??唤醒对深度休眠不起效果。不能再Base加入通�??唤醒�?

static UINT8 SleepDeal_IsIdleCurrent(void)
{
	return (UINT8)((g_stCellInfoReport.u16Ichg <= OtherElement.u16Sleep_VirCur_Chg) &&
				   (g_stCellInfoReport.u16IDischg <= OtherElement.u16Sleep_VirCur_Dsg));
}

void InitWakeUp_Base(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外�?�时钟，待机模式，RTC，看门狗
#if 1
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; // 选择要用的GPIO引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 设置引脚模式为上拉输入模�?
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// 设置�?�?�?0，EXTI0和PA0挂钩
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
	// 配置PA0_WKUP外部上升沿中�?
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising; // 上升沿中�?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// �?�?嵌�?��?��??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_1_IRQn; // 使能按键WK_UP所在的外部�?�?通道
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00; // 抢占优先�?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;	   // 使能外部�?�?通道
	NVIC_Init(&NVIC_InitStructure);
#endif

	// DI1
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13; // 选择要用的GPIO引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 设置引脚模式为上拉输入模�?
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 设置�?�?�?1，EXTI1和PA1挂钩
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource13);
	// 配置PA1_WKUP外部上升沿中�?
	EXTI_InitStruct.EXTI_Line = EXTI_Line13;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // 上升沿中�?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// �?�?嵌�?��?��??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_15_IRQn; // 使能按键WK_UP所在的外部�?�?通道
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;	// 抢占优先�?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		// 使能外部�?�?通道
	NVIC_Init(&NVIC_InitStructure);

	{
		GPIO_InitStructure.GPIO_Pin = PIN_GAN1; // 选择要用的GPIO引脚
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 设置引脚模式为上拉输入模�?
		GPIO_Init(GPIO_GAN1, &GPIO_InitStructure);

		SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource4);
		EXTI_InitStruct.EXTI_Line = EXTI_Line4;
		EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // 上升沿中�?
		EXTI_InitStruct.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStruct);
		// �?�?嵌�?��?��??
		NVIC_InitStructure.NVIC_IRQChannel = EXTI4_15_IRQn; // 使能按键WK_UP所在的外部�?�?通道
		NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;	// 抢占优先�?0
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		// 使能外部�?�?通道
		NVIC_Init(&NVIC_InitStructure);
	}
}

void InitWakeUp_NormalMode(void)
{
	InitWakeUp_Base();
}

void InitWakeUp_RTCMode(void)
{
	// InitWakeUp_Base();
	InitWakeUp_NormalMode(); // 包含了Base的唤醒方�?
	RTC_TimeConfig();
	RTC_AlarmConfig();
}

// 如果是standby模式的话，PA0的wkup不用�?
// 通�??唤醒对深度休眠不能起效果�?
void InitWakeUp_DeepMode(void)
{
	InitWakeUp_Base();
}

void delay(int n)
{
	while (n--)
		;
}

void IOstatus_Base(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE); // 开启GPIOA的�?��?�时�?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE); // 开启GPIOB的�?��?�时�?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE); // 开启GPIOC的�?��?�时�?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE); // 开启GPIOF的�?��?�时�?

	ADC_DeInit(ADC1);

	GPIOA->PUPDR = 0;
	GPIOA->MODER = 0XFFFFFFFF;
	GPIOB->PUPDR = 0;
	GPIOB->MODER = 0XFFFFFFFF;
	GPIOC->PUPDR = 0;
	GPIOC->MODER = 0XFFFFFFFF;
	GPIOF->PUPDR = 0;
	GPIOF->MODER = 0XFFFFFFFF;

	// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	// GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	// GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	// GPIO_Init(GPIOB, &GPIO_InitStructure);
	// GPIO_SetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);

	__delay_ms(100);
}

void IOstatus_NormalMode(void)
{
	IOstatus_Base();
}

void IOstatus_RTCMode(void)
{
	IOstatus_Base();
}

void IOstatus_DeepMode(void)
{
	IOstatus_Base();
}

void IORecover_RTCMode(void)
{
	MCU_RESET();
}

void IORecover_NormalMode(void)
{
	// TIM_Cmd(TIM3, ENABLE);	//用于App_SleepTest()函数
	MCU_RESET(); // 由于直接走下去�?�致各�?�因为现场破坏无法进入�?�常工作模式，完美的解决办法�?复位再来�?
}

void IORecover_DeepMode(void)
{
	MCU_RESET();
}

// wkup不用�?
void Sys_StandbyMode(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外�?�时钟，这句话是否需要？030不需要也能进入休�?
	// RCC_APB2PeriphResetCmd(0X01FC,DISABLE);				//复位所有IO�?   //TODO
	PWR_WakeUpPinCmd(PWR_WakeUpPin_1, ENABLE); // 使能唤醒管脚功能，PWR_CSR�?
											   // 该引脚会�?强制配置为下拉输入，意味着不需要配�?了？

	PWR_ClearFlag(PWR_FLAG_WU); // Clear WUF bit in Power Control/Status register (PWR_CSR)
								// 清PWR_CR相关便能清除PWR_CSR
	PWR_EnterSTANDBYMode();		// 进入待命（STANDBY）模式，PWR_CR    _PDDS
								// SCB->SCR设置为SLEEPDEEP = 1
}

// 030也是这样�?
void Sys_StopMode(void)
{
	// RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

// 如果信号到了，没法唤醒，单片机假死状态�?
// 就是以下这�?�话执�?�出�?题了，�?�部晶振出问�?
#if (defined _HSE_8M_PLL_48M) || (defined _HSE_12M_PLL_48M)
	RCC_HSEConfig(RCC_HSE_ON); // 起来后会�?切换回HSI
	while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
		;				// 等待 HSE 准�?�就�?
	RCC_PLLCmd(ENABLE); // 使能 PLL
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		;									   // 等待 PLL 准�?�就�?
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK); // 选择PLL作为系统时钟�?
	while (RCC_GetSYSCLKSource() != 0x08)
		; // 等待PLL�?选择为系统时钟源
#endif
}

void SleepDeal_Continue(void)
{
	UINT8 u8FlashWriteOK_flag = 0;
	static UINT8 s_u8SleepModeSelect = NORMAL_MODE;

	if (Sleep_Mode.bits.b1TestSleep)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1OverCurSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1OverVdeltaSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1CBCSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L1)
	{
		s_u8SleepModeSelect = HICCUP_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L2)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L3)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1VcellOVP)
	{
		// s_u8SleepModeSelect = HICCUP_MODE;
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1VcellUVP)
	{
		// s_u8SleepModeSelect = HICCUP_MODE;
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L1)
	{
		s_u8SleepModeSelect = HICCUP_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L2)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L3)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}

	WakeDisplaySocCache_Write(g_stCellInfoReport.SocElement.u16Soc);

	switch (s_u8SleepModeSelect)
	{
	case NORMAL_MODE:
		BootFlag_Write(FLASH_NORMAL_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;
		break;
	case HICCUP_MODE:
		BootFlag_Write(FLASH_HICCUP_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;
		break;
	case DEEP_MODE:
		BootFlag_Write(FLASH_DEEP_SLEEP_VALUE);
		u8FlashWriteOK_flag = 1;
		break;
	default:
		break;
	}

	if (u8FlashWriteOK_flag)
	{
		App_AFEshutdown();

		MCU_RESET();
	}
}

void SleepDeal_OverCurrent(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1OverCurSleep)
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 留下位置，后�?�?一次后进入需要延时则这里�?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > SleepInitOC)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{										// 如果检测到没问题，则退出休�?
		Sleep_Mode.bits.b1OverCurSleep = 0; // 放到switch�?句�?�面，FIRST和HICCUP两个都有�?
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_VcellOVP(void)
{
}

void SleepDeal_VcellUVP(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1VcellUVP)
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 直接进去
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > 0)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{ // 如果检测到没问题，则退出休�?
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 直接回到�?一次，force�?有一次，不是打嗝休眠模式
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Vdelta(void)
{
#if 0
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	
	if(!Sleep_Mode.bits.b1OverVdeltaSleep) {	//加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return ;
	}
#endif
}

void SleepDeal_Forced(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1ForceToSleep_L1 && Sleep_Mode.bits.b1ForceToSleep_L2 && Sleep_Mode.bits.b1ForceToSleep_L3)
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 直接进去
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > 0)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{ // 如果检测到没问题，则退出休�?
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 直接回到�?一次，force�?有一次，不是打嗝休眠模式
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_CBC(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1CBCSleep)
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 留下位置，后�?�?一次后进入需要延时则这里�?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > SleepInitCBC)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{									// 如果检测到没问题，则退出休�?
		Sleep_Mode.bits.b1CBCSleep = 0; // 放到switch�?句�?�面，FIRST和HICCUP两个都有�?
		// System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; 		//在这里�?�原�?否更好？
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Normal_L1(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 核心
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

	if (su8_SleepExtComCnt != RTC_ExtComCnt)
	{
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (OtherElement.u16Sleep_TimeRTC == 0)
		{
			// �?0时默�?RTC不进入休�?
		}
		else
		{
			if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeRTC * 60)
			{
				// if(++s_u32SleepFirstCnt >= 5) {			//这个，�??一次个后面都是一�?
				s_u32SleepFirstCnt = 0;
				s_u8SleepStatus = HICCUP;
				Sleep_Status = SLEEP_HICCUP_CONTINUE;
			}
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeRTC * 60)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_VNormal)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
	// s_u32SleepFirstCnt = 0;		//还没调好L1不进入休眠�?
}

void SleepDeal_Normal_L2(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

	if (su8_SleepExtComCnt != RTC_ExtComCnt)
	{
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeNormal * 60)
		{
			// if(++s_u32SleepFirstCnt >= 3) {			//这个，�??一次个后面都是一�?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeNormal * 60)
		{
			// if(++s_u32SleepHiccupCnt >= 1) {
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	// if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow || g_stCellInfoReport.u16VCellMin > OtherElement.u16Sleep_VNormal)
	if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow)
	{ // 触发条件才跳�?，别的时间不跳转
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Normal_L3(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	// static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

#if 0
	if(su8_SleepExtComCnt != RTC_ExtComCnt) {
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if(s_u32SleepFirstCnt)s_u32SleepFirstCnt = 0;
		if(s_u32SleepHiccupCnt)s_u32SleepHiccupCnt = 0;
	}
#endif

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeVlow * 60)
		{
			// if(++s_u32SleepFirstCnt >= 1) {			//这个，�??一次个后面都是一�?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeVlow * 60)
		{
			// if(++s_u32SleepHiccupCnt >= 1) {
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin >= OtherElement.u16Sleep_Vlow)
	{ // 触发条件才跳�?，别的时间不跳转
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

// 这个地方，IO控制策略要改一下，起来延时1s再打开管子会不会更好？不过现象貌似直接打开没问�?
// 这个作为主循�?，�?�果开头判�?出现了别的错�?，则跳出主循�?，去执�?�别�?
// 关于这里和IO控制主函数的逻辑�?题，A，最开头关于Sleep的return�?题。B，休眠起�?IO�?否立刻打开的问�?
void SleepDeal_Normal_Select(void)
{
	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 核心
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return;
	}

	if (SleepDeal_IsIdleCurrent())
	{
		if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow)
		{
			Sleep_Mode.bits.b1NormalSleep_L3 = 1;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L3;
		}
		else
		{ // ���е���RTC�������䣬����ͨ�͹��Ľ���
			Sleep_Mode.bits.b1NormalSleep_L2 = 1;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L2;
		}
	}
	else
	{
		// ����г�ŵ��������������
	}
}

// 架构决定要改一改，不然后期人员�?难维护了
void SleepDeal_Shift(void)
{
	if (Sleep_Mode.bits.b1TestSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_TEST;
	}
	else if (Sleep_Mode.bits.b1OverCurSleep != 0)
	{
		// Sleep_Status = SLEEP_HICCUP_CONTINUE;			//架构已改，先跳到相关函数，再进入休眠
		Sleep_Status = SLEEP_HICCUP_OVERCUR;
	}
	else if (Sleep_Mode.bits.b1OverVdeltaSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_OVDELTA;
	}
	else if (Sleep_Mode.bits.b1CBCSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_CBC;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L1 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L2 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L3 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}

	else if (Sleep_Mode.bits.b1VcellOVP != 0)
	{
		Sleep_Status = SLEEP_HICCUP_VCELLOVP;
	}
	else if (Sleep_Mode.bits.b1VcellUVP != 0)
	{
		Sleep_Status = SLEEP_HICCUP_VCELLUVP;
	}
	else
	{ // 没有以上各�?�保护直接进入主�?�?
		Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
	}
}

void SleepDeal_Test(void)
{
	static UINT16 s_u16HaltTestCnt = 0;
	if (!Sleep_Mode.bits.b1TestSleep)
	{ // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return;
	}

	if (++s_u16HaltTestCnt >= 2)
	{ // 10s——Test
		s_u16HaltTestCnt = 0;
		Sleep_Status = SLEEP_HICCUP_CONTINUE;
	}
}

static void SleepStartup_WaitForWakeup(void)
{
	while (1)
	{
		do
		{
			Sys_StopMode();
		} while (!IsSleepWakeupValid());
#ifdef __FUNC__LED__
		if (s_sleep_wakeup_by_di1)
		{
			s_sleep_wakeup_by_di1 = 0;
			if (!LedBar_HandleWakePreviewBeforeBoot())
			{
				continue;
			}
		}
#endif
		break;
	}
}

void IsSleepStartUp(void)
{
	UINT16 sleep_flag;

	sleep_flag = BootFlag_Read();
	switch (sleep_flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		BootFlag_Clear();
		Init_RTC();
		IOstatus_RTCMode();
		InitWakeUp_RTCMode();
		break;

	case FLASH_NORMAL_SLEEP_VALUE:
		BootFlag_Clear();
		IOstatus_NormalMode();
		InitWakeUp_NormalMode();
		break;

	case FLASH_DEEP_SLEEP_VALUE:
		BootFlag_Clear();
		IOstatus_DeepMode();
		InitWakeUp_DeepMode();
		break;

	case FLASH_SLEEP_RESET_VALUE:
		return;

	default:
		BootFlag_Clear();
		return;
	}

	SleepStartup_WaitForWakeup();

	switch (sleep_flag)
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		IORecover_RTCMode();
		break;

	case FLASH_NORMAL_SLEEP_VALUE:
		IORecover_NormalMode();
		break;

	case FLASH_DEEP_SLEEP_VALUE:
		IORecover_DeepMode();
		break;

	default:
		break;
	}
}
extern UINT8 gu8_1000msAccClock_Flag;
void App_SleepDeal(void)
{
	static uint8_t force_sleep_delay = 0;

	if (reset_sleep_state)
	{
		reset_sleep_state = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;

		Sleep_Mode.all = 0;
	}

	if (SystemStatus.bits.b1StartUpBMS)
	{ // 开机完毕再进入
		return;
	}
	else
	{
		SystemStatus.bits.b1Status_ToSleep = 1;
	}

	if (Sleep_Mode.bits.b1_ToSleepFlag)
	{
		LogRecord_Flag.bits.Log_Sleep = 1;
		return;
	}

	if (0 == gu8_1000msAccClock_Flag && !Sleep_Mode.bits.b1ForceToSleep_L1 && !Sleep_Mode.bits.b1ForceToSleep_L2 && !Sleep_Mode.bits.b1ForceToSleep_L3)
	{
		return; // 如果�?强制进入休眠的则必须�?点进入休眠，不能�?
	}
	gu8_1000msAccClock_Flag = 0;

	switch (Sleep_Status)
	{
	case SLEEP_HICCUP_SHIFT: // 先跳到这里，再跳到SleepDeal_Continue()，然后进入别的循�?
		SleepDeal_Shift();	 // 主控跳转函数，开机执行一遍没事进入核心循�?函数
		break;
	case SLEEP_HICCUP_NORMAL_SELECT:
		SleepDeal_Normal_Select();
		break;
	case SLEEP_HICCUP_TEST:
		SleepDeal_Test();
		break;
	case SLEEP_HICCUP_OVERCUR:
		SleepDeal_OverCurrent();
		break;
	case SLEEP_HICCUP_OVDELTA:
		SleepDeal_Vdelta(); // �?前压�?过大直接进入休眠不起来，�?�?�?
		break;
	case SLEEP_HICCUP_CBC:
		SleepDeal_CBC();
		break;
	case SLEEP_HICCUP_FORCED:
		SleepDeal_Forced(); // 还没�?
		break;
	// case SLEEP_HICCUP_NORMAL_L1:
	// 	SleepDeal_Normal_L1();
	// 	break;
	case SLEEP_HICCUP_NORMAL_L2:
		SleepDeal_Normal_L2();
		break;
	case SLEEP_HICCUP_NORMAL_L3:
		SleepDeal_Normal_L3();
		break;

	case SLEEP_HICCUP_VCELLOVP:
		SleepDeal_VcellOVP();
		break;
	case SLEEP_HICCUP_VCELLUVP:
		SleepDeal_VcellUVP();
		break;

	case SLEEP_HICCUP_CONTINUE:
		SleepDeal_Continue();
		break;
	default:
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		break;
	}

	if (g_stCellInfoReport.u16VCellMin < 2500)
	{
		++force_sleep_delay;
		if (force_sleep_delay >= 60 * 60)
		{
			entersleep(DEEP_MODE);
		}
	}
	else
	{
		force_sleep_delay = 0;
	}

	if (SLEEP_HICCUP_CONTINUE == Sleep_Status)
	{
		Sleep_Mode.bits.b1_ToSleepFlag = 1;
	}
	else
	{
		Sleep_Mode.bits.b1_ToSleepFlag = 0;
	}
}

void IOstatus_TestMode(void)
{
	IOstatus_NormalMode();
}

void InitWakeUp_TestMode(void)
{
	InitWakeUp_NormalMode();
}

void IORecover_TestMode(void)
{
	MCU_RESET();
}

void Sys_SleepOnExitMode(void)
{
	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // 库函数版�?，�?�置SLEEP ON EXIT位为1
	// SCB->SCR|=1<<1;//寄存器版�?，�?�置SLEEP ON EXIT位为1
	__ASM volatile("wfi");
}

void entersleep(enum _SLEEP_MODE mode)
{
	switch (mode)
	{
	case HICCUP_MODE:
		Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
		// g_sleepModeSelect = HICCUP_MODE;
		break;
	case NORMAL_MODE:
		Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
		break;
	case DEEP_MODE:
		Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
		// g_sleepModeSeect = DEEP_MODE;
		break;
	case NO_SLEEP:
		// g_sleepModeSelect = NO_SLEEP;
		Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
		Sleep_Mode.all = 0;
		break;
	default:
		break;
	}
}
