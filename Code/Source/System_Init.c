#include "main.h"

struct CBC_ELEMENT CBC_Element;

static INT8 fac_us = 0;	 // us
static INT16 fac_ms = 0; // ms

UINT8 g_u81msCnt = 0;
UINT8 g_u810msClockCnt = 0;
UINT8 g_u81msClockCnt = 0;

UINT8 gu8_200msCnt = 0;
UINT8 gu8_200msAccClock_Flag = 0;

void IWDG_HaltConfig(void);

void InitDelay(void)
{
	extern uint32_t us_ticks;

	us_ticks = SystemCoreClock / 1000000U;   // 预计算 1us 对应的 ticks
}

void __delay_us(UINT32 nus)
{
	UINT32 temp;
	SysTick->LOAD = nus * fac_us;			  // 时间加载
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
	} while ((temp & 0x01) && !(temp & (1 << 16))); // 等待时间到达
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

// 这个是非中断方式的延时，倘若使用中断式延时，在中断中使用延时会出现中断嵌套问题，很容易出错
void __delay_ms(UINT16 ms)
{
	UINT32 temp;
	SysTick->LOAD = (UINT32)ms * fac_ms;	  // 时间加载(SysTick->LOAD为24bit)
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
		Feed_IWatchDog;
	} while (temp & 0x01 && !(temp & (1 << 16))); // 等待时间到达

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

void InitIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE); // 开启GPIOA的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE); // 开启GPIOC的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE); // 开启GPIOF的外设时钟

	{
		GPIO_WriteBit(GPIO_M_CTR, PIN_M_CTR, 1);
		GPIO_InitStructure.GPIO_Pin = PIN_M_CTR;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_Init(GPIO_M_CTR, &GPIO_InitStructure);

		GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, 1);
		GPIO_InitStructure.GPIO_Pin = PIN_AD_EN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_Init(GPIO_AD_EN, &GPIO_InitStructure);
	}
	GPIO_InitStructure.GPIO_Pin = PIN_WK_AFE;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIO_WK_AFE, &GPIO_InitStructure);

	GPIO_WriteBit(GPIO_DB_LED1, PIN_DB_LED1, 0);
	GPIO_InitStructure.GPIO_Pin = PIN_DB_LED1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIO_DB_LED1, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_KEY1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);

	lk8625_init();
}

// 使用LSI，38KHz
void Init_IWDG(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外设时钟，待机模式，RTC，看门狗
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);		// 打开独立看门狗寄存器操作权限
	IWDG_SetPrescaler(IWDG_Prescaler_64);				// 预分频系数
	IWDG_SetReload(160);								// 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
														// 800——1.28s，80——128ms
	IWDG_ReloadCounter();								// 喂狗
	IWDG_Enable();										// 使能IWDG
}
