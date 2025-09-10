#include "main.h"

__IO UINT16 g_u16ADCValFilter[ADC_NUM]; // 这个位数不能改

INT32 g_u32ADCValFilter2[ADC_NUM]; // ADC数据缓存2，问题解决了，原来是UINT32，在计算过程出错了！
INT32 g_i32ADCResult[ADC_NUM];	   // ADC结果保存

UINT16 g_u16IoutOffsetAD;
UINT16 g_u16Q7BusCurr_CHG;
UINT16 g_u16Q7BusCurr_DSG;

const UINT16 iSheldTemp_10K[LENGTH_TBLTEMP_PORT_10K] =
	{
		// AD		(Temp+40)*10
		3771, 100, //-30
		3683, 150, //-25
		3580, 200, //-20
		3460, 250, //-15
		3323, 300, //-10
		3169, 350, //-5
		3004, 400, // 0
		2820, 450, // 5
		2633, 500, // 10
		2437, 550, // 15
		2241, 600, // 20
		2048, 650, // 25
		1859, 700, // 30
		1679, 750, // 35
		1509, 800, // 40
		1351, 850, // 45
		1204, 900, // 50
		1073, 950, // 55
		953, 1000, // 60
		845, 1050, // 65
		749, 1100, // 70
		664, 1150, // 75
		588, 1200, // 80
		522, 1250, // 85
		463, 1300, // 90
		411, 1350, // 95
		366, 1400, // 100
		326, 1450, // 105
};
/*
问题描述
1，通过 uint16_t ConvData[8]保存DMA搬运的ADC转换数值，但是这个数组数值的顺序总是和ADC不是顺序对应的。
2，比如用7个通道的ADC，当设置ADC_InitStructure.ADC_ScanDirection = ADC_ScanDirection_Backward，
   是对应顺序是：0->0,1->7,2->6…7->1 ；
3，当设置ADC_InitStructure.ADC_ScanDirection = ADC_ScanDirection_Upward，
   是对应顺序是：0->7,1->0,2->1…7->6 。问题原因F0的ADC在使用之前需要校准。
4，这个7位的校准值也是放在ADC_DR中的，它也会触发DMA请求。
5，可以参照F0的ADC-DMA例程，先做ADC校准、然后再设置DMA，再使能ADC的DMA。
*/
// ADC+DMA+TIME触发，当DMA 模式开启时(ADC_CFGR1 寄存器中的DMAEN =1), 每次转换结束时都会产生一个
// DMA 请求。这样就允许把在ADC_DR 寄存器中的转换数据传送到软件指定的目标地址中。
// T15和DMA1的中断不需要放出来，ADC的中断也不需要。debug有用吗？
void InitADC_DMA(void)
{

	DMA_InitTypeDef DMA_InitStruct;
	// NVIC_InitTypeDef  		 NVIC_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); // 开启DMA1外设时钟，用于读取ADC1

	/*
	//DMA中断配置
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;       //选择DMA1通道中断
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                //中断使能
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;                //优先级设为0
	NVIC_Init(&NVIC_InitStructure);
	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);		//使能DMA中断，后面看看是否需要，个人感觉不需要
	*/

	// DMA初始化
	// 按道理SYSCFG_CFGR1的ADC_DMA_RMP位置为0，ADC才和Channel1连在一起，而1时和Channel2连在一起(ADC要么Channel1要么2)
	// 而这个东西我没配置过，reset值为0，所以不用管
	DMA_DeInit(DMA1_Channel1);												 // 选择频道
	DMA_StructInit(&DMA_InitStruct);										 // 初始化DMA结构体
	DMA_InitStruct.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));			 // 配置外设地址
	DMA_InitStruct.DMA_MemoryBaseAddr = (UINT32)(&g_u16ADCValFilter[0]);	 // 设置内存映射地址
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;							 // 数据传输方向，0：从外设读。1：从存储器读
	DMA_InitStruct.DMA_BufferSize = ADC_NUM;								 // 传输次数，DMA缓存数组大小设置
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;			 // 外设地址不变，这个不太懂是哪个外设地址
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;					 // 内存地址增加
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 外设半字传输16位
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;		 // 内存半字传输16位
	DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;							 // 循环模式
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;						 // 高优先级
	DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;								 // 非内存到内存传输
	DMA_Init(DMA1_Channel1, &DMA_InitStruct);
	DMA_Cmd(DMA1_Channel1, ENABLE);
}

void InitADC_GPIO(void)
{

	GPIO_InitTypeDef GPIO_InitStruct;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE); // 开启GPIOA的外设时钟

	// PA4_TTC_EV，PA5_VDC
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5; // ADC_Channel_5对PA5
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStruct); // PA5输入时不用设置速率

	// PB0_TTC_MOS1
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0; // ADC_Channel_5对PA5
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStruct); // PA5输入时不用设置速率
}

void InitADC_TIMER(void)
{

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	// NVIC_InitTypeDef  		 NVIC_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM15, ENABLE); // 时钟15使能

	/*
	NVIC_InitStructure.NVIC_IRQChannel = TIM15_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	TIM_ITConfig(TIM15, TIM_IT_Update, ENABLE);					//使能TIM15中断
	*/

	// 定时器TIM15初始化，用于ADC 10ms采样
	TIM_TimeBaseStructure.TIM_Period = 1000;						// 1000*10 = 10ms
	TIM_TimeBaseStructure.TIM_Prescaler = SystemCoreClock / 100000; // 10us
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM15, &TIM_TimeBaseStructure);
	TIM_SelectOutputTrigger(TIM15, TIM_TRGOSource_Update); // 重点，选择TRGO触发源为计时器更新时间
	TIM_Cmd(TIM15, ENABLE);								   // 使能TIMx
}

// 应该是，Time触发ADC采样，ADC采样完成触发DMA采样
void InitADC_ADC1(void)
{

	ADC_InitTypeDef ADC_InitStruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); // 开启ADC1外设时钟

	// ADC初始化
	ADC_DeInit(ADC1);														   // ADC恢复默认设置
	ADC_StructInit(&ADC_InitStruct);										   // 初始化ADC结构体
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;						   // 配置ADC1在连续模式下分辨率为12bits
	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;						   // 禁止连续模式，连续采样的话，次数由DMA决定，DMA采样完产生中断，停止ADC便可
																			   // 这句话后面再看看，别的解释是采样一次后后续的转换就会永不停歇，不是DMA触发
																			   // 也就是转换一次后，接着进行下一次转换，不断连续。
	ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T15_TRGO;	   // 外部触发设置为TIM15
	ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising; // 上升沿触发，使用计时器
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;						   // ADC数据右对齐
	ADC_InitStruct.ADC_ScanDirection = ADC_ScanDirection_Upward;			   // 多通道AD采样使用，向上扫描0-18通道
	ADC_Init(ADC1, &ADC_InitStruct);
	ADC_ChannelConfig(ADC1, ADC_Channel_4 | ADC_Channel_5 | ADC_Channel_8,
					  ADC_SampleTime_55_5Cycles); // ADC总转换时间=采样时间+12.5个ADC时钟周期(信号量转换时间)
												  // 所以总共55.5+12.5 = 68个ADC周期。时间为 68/8 us
	ADC_OverrunModeCmd(ADC1, ENABLE);			  // 使能数据覆盖模式
	ADC_GetCalibrationFactor(ADC1);				  // ADC校准
	ADC_Cmd(ADC1, ENABLE);						  // ADC使能
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADEN))
		; // 等待ADEN(ADC1->CR)使能。ADC_Cmd(ADC1, ENABLE)。

	ADC_DMARequestModeConfig(ADC1, ADC_DMAMode_Circular); // ADC配置DMA循环模式
	ADC_DMACmd(ADC1, ENABLE);							  // ADC的DMA使能
	ADC_StartOfConversion(ADC1);						  // ADC转换开始
}

void ADC_Current_Smooth(void)
{
}

// 69.3——3.3 = 21
void ADC_Vbus(void)
{
}

void ADC_TTC(void)
{
	INT32 t_i32temp = 0;

	//-------------Environment温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_EV1]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_EV1] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_EV1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_EV1];
	g_i32ADCResult[ADC_TEMP_EV1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_EV1] + 512) >> 10);

	//-------------Environment温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_EV2]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_EV2] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_EV2]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_EV2];
	g_i32ADCResult[ADC_TEMP_EV2] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_EV2] + 512) >> 10);

	//-------------MOS1温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_MOS1]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_MOS1] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_MOS1];
	g_i32ADCResult[ADC_TEMP_MOS1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_MOS1] + 512) >> 10);
}

void InitADC(void)
{
	UINT8 i;
	InitADC_GPIO();
	InitADC_TIMER();
	InitADC_DMA();
	InitADC_ADC1();

	for (i = 0; i < ADC_NUM; i++)
	{
		g_u16ADCValFilter[i] = 0;
		g_i32ADCResult[i] = 0;
		g_u32ADCValFilter2[i] = 0;
	}
}

void App_AnlogCal(void)
{
	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_ADC))
	{
		// return;
	}

	if (0 == g_st_SysTimeFlag.bits.b1Sys1msFlag)
	{
		return;
	}

	// ADC_Vbus();
	ADC_TTC();
}
