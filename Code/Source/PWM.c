#include "main.h"

UINT32 DACx_OC_REF = 0;

UINT16 u16PeriodCH1 = 0;
UINT16 u16PeriodCH2 = 0;
UINT16 u16PeriodCH3 = 0;
UINT16 u16PeriodCH4 = 0;

void InitPWM_TIME3(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	// 一，初始化需要的引脚
	// PA6_PWM_OC_DSG，PA7_PWM-OC-REF
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // 选择要用的GPIO引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;		   // 这句话搞错导致无法输出波形
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_1);

	// 二，初始化TIM3
	TIM_TimeBaseStructure.TIM_Period = 1000 - 1;						 // 设置在下一个更新事件装入活动的自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_Prescaler = SystemCoreClock / 1000000 - 1; // 设置用来作为TIMx时钟频率除数的预分频值——计数分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;			 /* Time 定时设置为上升沿计算模式*/
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	// 三，配置PWM，正通道，互补通道不配置则默认不会输出
	// CH1
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;			  // 设置为pwm1输出模式
	TIM_OCInitStructure.TIM_Pulse = u16PeriodCH1;				  // 设置占空比时间
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	  // 设置输出极性
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;  // 死区后输出状态
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能该通道输出
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable); // 使能TIM1在CCR1上的预装载寄存器，TIM_SetCompare1()函数可用

	// CH2
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;			  // 设置为pwm1输出模式
	TIM_OCInitStructure.TIM_Pulse = u16PeriodCH2;				  // 设置占空比时间
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	  // 设置输出极性
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;  // 死区后输出状态
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能该通道输出
	TIM_OC2Init(TIM3, &TIM_OCInitStructure);
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable); // 使能TIM1在CCR1上的预装载寄存器，TIM_SetCompare1()函数可用

	/* TIMX 计算器使能*/
	TIM_Cmd(TIM3, ENABLE);

	/* TIMX 主输出使能 */
	// 这句话需要吗？
	TIM_CtrlPWMOutputs(TIM3, ENABLE);
}

void InitPWM(void)
{
	UINT32 u32_DACxTemp;
	UINT32 u32_CS_Res;

	u32_DACxTemp = ((UINT32)OtherElement.u16CS_Cur_DSGmax * 500) / (OtherElement.u16CS_Cur_CHGmax + OtherElement.u16CS_Cur_DSGmax);
	DACx_OC_REF = (u32_DACxTemp > 330 ? 4095 : (u32_DACxTemp << 12) / 330) + (0 << 16); // 通道1和通道2

	u32_CS_Res = ((UINT32)OtherElement.u16Sys_CS_Res * 1000) / OtherElement.u16Sys_CS_Res_Num; // 扩大1000倍
	// PWM-OC-DSG，比较电压，比较阀值，DSG所以是减法
	u16PeriodCH1 = (DACx_OC_REF * 662 >> 12) - (UINT32)OtherElement.u16CBC_Cur_DSG * u32_CS_Res * 4 / 10 / 1000;

	// DAC抬升电压改为PWM输出，PWM-OC-REF
	u16PeriodCH2 = DACx_OC_REF * 1000 / 4096;

	InitPWM_TIME3();
}

// 由于不带DMA，所以相关数值修改，需要调用这个函数
void App_PWM(void)
{
	UINT32 u32_DACxTemp;
	UINT32 u32_CS_Res;

	u32_DACxTemp = ((UINT32)OtherElement.u16CS_Cur_DSGmax * 500) / (OtherElement.u16CS_Cur_CHGmax + OtherElement.u16CS_Cur_DSGmax);
	DACx_OC_REF = (u32_DACxTemp > 330 ? 4095 : (u32_DACxTemp << 12) / 330) + (0 << 16); // 通道1和通道2

	u32_CS_Res = ((UINT32)OtherElement.u16Sys_CS_Res * 1000) / OtherElement.u16Sys_CS_Res_Num; // 扩大1000倍
	// PWM-OC-DSG，比较电压，比较阀值
	u16PeriodCH1 = (DACx_OC_REF * 662 >> 12) - (UINT32)OtherElement.u16CBC_Cur_DSG * u32_CS_Res * 4 / 10 / 1000;

	// DAC抬升电压改为PWM输出，PWM-OC-REF
	u16PeriodCH2 = DACx_OC_REF * 1000 / 4096;

	// TIM_SetAutoreload(TIM4, 1000);				//在运行当中想要改变PWM频率调用
	TIM_SetCompare1(TIM3, u16PeriodCH1); // 在运行当中想要改变PWM占空比调用
	TIM_SetCompare2(TIM3, u16PeriodCH2); // 在运行当中想要改变PWM占空比调用
}
