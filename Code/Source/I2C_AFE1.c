#include "main.h"

__IO uint32_t Timeout = LONG_TIMEOUT;

RegisterGroup Registers_AFE1;
struct stBq769x0_Read g_stBq769x0_Read_AFE1;

unsigned char startData_[80]; // 局部，全局变量
unsigned char BufferCRCC[30];

const unsigned int OVPThreshold = 3800;
const unsigned int UVPThreshold = 2100;
const unsigned char SCDDelay = SCD_DELAY_400us;
// const unsigned char SCDThresh = SCD_THRESH_178mV_89mV;
const unsigned char SCDThresh = SCD_THRESH_200mV_100mV;

const unsigned char OCDDelay = OCD_DELAY_1280ms;
const unsigned char OCDThresh = OCD_THRESH_100mV_50mV;
// const unsigned char OCDThresh = OCD_THRESH_50mV_25mV;

const unsigned char OVDelay = OV_DELAY_2s;
const unsigned char UVDelay = UV_DELAY_4s;

const UINT16 iSheldTemp_10K_AFE[LENGTH_TBLTEMP_AFE_10K] = {
	// AD		(Temp+40)*10
	7954, 100,	//-30
	7769, 150,	//-25
	7551, 200,	//-20
	7298, 250,	//-15
	7009, 300,	//-10
	6685, 350,	//-5
	6336, 400,	// 0
	5951, 450,	// 5
	5555, 500,	// 10
	5142, 550,	// 15
	4728, 600,	// 20
	4319, 650,	// 25
	3922, 700,	// 30
	3542, 750,	// 35
	3183, 800,	// 40
	2850, 850,	// 45
	2541, 900,	// 50
	2263, 950,	// 55
	2010, 1000, // 60
	1784, 1050, // 65
	1581, 1100, // 70
	1401, 1150, // 75
	1241, 1200, // 80
	1101, 1250, // 85
	977, 1300,	// 90
	868, 1350,	// 95
	773, 1400,	// 100
	689, 1450,	// 105
};

#ifdef I2C_SIMULATION
// 产生IIC起始信号
void IIC_Start(void)
{
	SDA_OUT(); // sda线输出
	IIC_SDA = 1;
	IIC_SCL = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SDA = 0; // START:when CLK is high,DATA change form high to low
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 0; // 钳住I2C总线，准备发送或接收数据
}

// 产生IIC停止信号
void IIC_Stop(void)
{
	SDA_OUT(); // sda线输出
	IIC_SCL = 0;
	IIC_SDA = 0; // STOP:when CLK is high DATA change form low to high
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SDA = 1; // 发送I2C总线结束信号
	__delay_us(DELAY_US_IIC_AFE1);
}

// 等待应答信号到来
// 返回值：1，接收应答失败
//         0，接收应答成功
UINT8 IIC_Wait_Ack(void)
{
	UINT8 ucErrTime = 0;
	SDA_IN(); // SDA设置为输入
	IIC_SDA = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	while (READ_SDA)
	{
		ucErrTime++;
		if (ucErrTime > 250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL = 0; // 时钟输出0
	__delay_us(DELAY_US_IIC_AFE1);
	return 0;
}
// 产生ACK应答
void IIC_Ack(void)
{
	IIC_SCL = 0;
	SDA_OUT();
	IIC_SDA = 0;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 0;
}
// 不产生ACK应答
void IIC_NAck(void)
{
	IIC_SCL = 0;
	SDA_OUT();
	IIC_SDA = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 1;
	__delay_us(DELAY_US_IIC_AFE1);
	IIC_SCL = 0;
}
// IIC发送一个字节
// 返回从机有无应答
// 1，有应答
// 0，无应答
void IIC_Send_Byte(UINT8 txd)
{
	UINT8 t;
	SDA_OUT();
	IIC_SCL = 0; // 拉低时钟开始数据传输
	for (t = 0; t < 8; t++)
	{
		// IIC_SDA=(txd&0x80)>>7;
		if ((txd & 0x80) >> 7)
			IIC_SDA = 1;
		else
			IIC_SDA = 0;
		txd <<= 1;
		__delay_us(DELAY_US_IIC_AFE1); // 对TEA5767这三个延时都是必须的
		IIC_SCL = 1;
		__delay_us(DELAY_US_IIC_AFE1);
		IIC_SCL = 0;
		__delay_us(DELAY_US_IIC_AFE1);
	}
}

// 读1个字节，ack=1时，发送ACK，ack=0，发送nACK
UINT8 IIC_Read_Byte(unsigned char ack)
{
	unsigned char i, receive = 0;
	SDA_IN(); // SDA设置为输入
	for (i = 0; i < 8; i++)
	{
		IIC_SCL = 0;
		__delay_us(DELAY_US_IIC_AFE1);
		IIC_SCL = 1;
		receive <<= 1;
		if (READ_SDA)
			receive++;
		__delay_us(DELAY_US_IIC_AFE1);
	}
	if (!ack)
		IIC_NAck(); // 发送nACK
	else
		IIC_Ack(); // 发送ACK
	return receive;
}

int I2CSendBytes(unsigned char I2CSlaveAddress, unsigned char *DataBuffer, unsigned int ByteCount, unsigned int *SentByte)
{
	unsigned int NumberOfBytesSent = 0;
	unsigned char *DataPointer;
	DataPointer = DataBuffer;

	IIC_Start(); // 钳住两根线

	IIC_Send_Byte(I2CSlaveAddress << 1); // 发送AFE地址
	if (1 == IIC_Wait_Ack())
	{
		*SentByte = NumberOfBytesSent;
		// System_ERROR_UserCallback(AFE1_ERROR);		//在这里用这个函数没必要，不然很多地方都要调用很麻烦。
		return 1; // return 1在最终的地方调用便可
	}

	for (NumberOfBytesSent = 0; NumberOfBytesSent < ByteCount; NumberOfBytesSent++)
	{
		IIC_Send_Byte(*DataPointer); // 发送低地址
		if (1 == IIC_Wait_Ack())
		{
			*SentByte = NumberOfBytesSent;
			// System_ERROR_UserCallback(AFE1_ERROR);
			return 1;
		}
		DataPointer++;
	}
	IIC_Stop(); // 产生一个停止条件
	*SentByte = NumberOfBytesSent;
	__delay_ms(1);
	return 0;
}

int I2CReadBytes(unsigned char I2CSlaveAddress, unsigned char *DataBuffer, unsigned int ExpectedByteNumber, unsigned int *NumberOfReceivedBytes)
{
	unsigned char *DataPointer;
	unsigned int *NumberOfReceivedBytesPointer;
	unsigned int i;

	NumberOfReceivedBytesPointer = NumberOfReceivedBytes;
	*NumberOfReceivedBytesPointer = 0;
	DataPointer = DataBuffer;

	IIC_Start();

	IIC_Send_Byte((I2CSlaveAddress << 1) | I2C_RW_R); // 发送写命令
	if (1 == IIC_Wait_Ack())
	{
		// System_ERROR_UserCallback(AFE1_ERROR);
		return 1;
	}

	for (i = 0; i < ExpectedByteNumber; i++)
	{
		if ((i + 1) < ExpectedByteNumber)
		{
			*DataPointer = IIC_Read_Byte(1);
		}
		else
		{
			*DataPointer = IIC_Read_Byte(0);
		}
		DataPointer++;
	}

	IIC_Stop(); // 产生一个停止条件
	return 0;
}
#endif

#ifdef I2C_SYSTEM
void Init_I2C(void)
{

	I2C_InitTypeDef I2C_InitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;
	/* I2C configuration */

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE); // 开启I2C2外设时钟

	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable; // 0: 模拟噪声滤波器开启
	I2C_InitStruct.I2C_DigitalFilter = 0x00;				   // 数字滤波器关闭
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStruct.I2C_Timing = 0x00901D2B; // 这句话我不管先，是一个excel计算出来的，8MHz
	// I2C_InitStruct.I2C_Timing = 0x30E3363D;							//48MHz
	/* I2C Peripheral Enable */
	I2C_Cmd(I2C2, ENABLE);
	/* Apply I2C configuration after enabling it */
	I2C_Init(I2C2, &I2C_InitStruct);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	/*
	GPIO_PinAFConfig(GPIOF , GPIO_Pin_6, GPIO_AF_1);		//GPIOF无复用功能，默认I2C
	GPIO_PinAFConfig(GPIOF ,GPIO_Pin_7, GPIO_AF_1);
	*/
}

uint32_t TIMEOUT_UserCallback(void)
{
	/* Block communication and all processes */

	System_ERROR_UserCallback(ERROR_AFE1);
	return 1;
}

/*
这次调试I2C产生问题汇总
A，IO端口可能有问题，第一次模拟I2C发现SCL端口坏了，不能输出高电平
B，I2C_TransferHandling没了解清楚有什么用，这个不仅可以产生start信号，还同时输出地址(看手册！这里困扰了很久！也没动手用示波器观察！)
C，输出地址不能自动左移动，必须自己手动左移！不能相信别人，要用示波器去观察输出波形是否是自己需要的！
D，I2C_AutoEnd_Mode，I2C_Generate_Start_Write，I2C_Generate_Start_Read了解好，例如写完读完相应字节能自动产生END,ACK信号。reload模式就不是之类的
E，貌似地址左移1位就可以了，不需要加读写控制位，I2C_Generate_Start_Write，I2C_Generate_Start_Read会自动加，但是就算自己加了也不影响。
*/

int I2CSendBytes(unsigned char I2CSlaveAddress, unsigned char *DataBuffer, unsigned int ByteCount, unsigned int *SentByte)
{

	unsigned int NumberOfBytesSent = 0;
	unsigned char *DataPointer;
	DataPointer = DataBuffer;

	Timeout = LONG_TIMEOUT;
	Feed_IWatchDog;
	while (I2C_GetFlagStatus(I2C_AFE, I2C_FLAG_BUSY) != RESET)
	{ // 等待总线不忙
		if ((Timeout--) == 0)
		{
			*SentByte = NumberOfBytesSent;
			return TIMEOUT_UserCallback();
		}
	}

	I2C_TransferHandling(I2C_AFE, (I2CSlaveAddress << 1) | I2C_RW_W, ByteCount, I2C_AutoEnd_Mode, I2C_Generate_Start_Write);

	Feed_IWatchDog;
	while (ByteCount--)
	{
		Timeout = LONG_TIMEOUT;
		while (I2C_GetFlagStatus(I2C_AFE, I2C_ISR_TXIS) == RESET)
		{ // 出现在发送中断，则发
			if ((Timeout--) == 0)
			{ // 等待可写数据进写缓冲寄存器
				*SentByte = NumberOfBytesSent;
				I2C_GenerateSTOP(I2C_AFE, ENABLE);
				return TIMEOUT_UserCallback();
			}
		}
		I2C_SendData(I2C_AFE, *DataPointer);
		DataPointer++;
	}

	/* Wait until STOPF flag is set */
	Timeout = LONG_TIMEOUT;
	Feed_IWatchDog;
	while (I2C_GetFlagStatus(I2C_AFE, I2C_ISR_STOPF) == RESET)
	{
		if ((Timeout--) == 0)
		{
			*SentByte = NumberOfBytesSent;
			I2C_GenerateSTOP(I2C_AFE, ENABLE);
			return TIMEOUT_UserCallback();
		}
	}
	I2C_ClearFlag(I2C_AFE, I2C_ICR_STOPCF);

	*SentByte = NumberOfBytesSent;
	return 0;
}

int I2CReadBytes(unsigned char I2CSlaveAddress, unsigned char *DataBuffer, unsigned int ExpectedByteNumber, unsigned int *NumberOfReceivedBytes)
{
	// unsigned long int DelayCounter = 0;
	unsigned char *DataPointer;
	unsigned int *NumberOfReceivedBytesPointer;

	NumberOfReceivedBytesPointer = NumberOfReceivedBytes;
	*NumberOfReceivedBytesPointer = 0; // 目前没用到
	DataPointer = DataBuffer;

	Timeout = LONG_TIMEOUT;
	Feed_IWatchDog;
	while (I2C_GetFlagStatus(I2C_AFE, I2C_FLAG_BUSY) != RESET)
	{ // 等待总线不忙
		if ((Timeout--) == 0)
		{ // 因为PIC代码是先写再读，写函数有stop标志把TC标志位去掉
			return TIMEOUT_UserCallback();
		}
	}

	I2C_TransferHandling(I2C_AFE, (I2CSlaveAddress << 1) | I2C_RW_R, ExpectedByteNumber, I2C_AutoEnd_Mode, I2C_Generate_Start_Read);
	// I2C_TransferHandling(I2C2, (I2CSlaveAddress<<1), ExpectedByteNumber,  I2C_AutoEnd_Mode, I2C_Generate_Start_Read);
	Feed_IWatchDog;
	while (ExpectedByteNumber)
	{
		Timeout = LONG_TIMEOUT;
		while (I2C_GetFlagStatus(I2C_AFE, I2C_ISR_RXNE) == RESET)
		{ // 收到非空，则读
			if ((Timeout--) == 0)
			{
				return TIMEOUT_UserCallback();
			}
		}
		*DataPointer = I2C_ReceiveData(I2C_AFE);
		DataPointer++;
		ExpectedByteNumber--;
	}

	Timeout = LONG_TIMEOUT;
	Feed_IWatchDog;
	while (I2C_GetFlagStatus(I2C_AFE, I2C_ISR_STOPF) == RESET)
	{
		if ((Timeout--) == 0)
		{
			return TIMEOUT_UserCallback();
		}
	}
	I2C_ClearFlag(I2C_AFE, I2C_ICR_STOPCF);

	return 0;
}
#endif

int I2CWriteBlockWithCRC(unsigned char I2CSlaveAddress, unsigned char StartAddress, unsigned char *Buffer, unsigned char Length)
{
	unsigned char *Pointer;
	int i;
	unsigned int SentByte = 0;
	int result;

	Pointer = BufferCRCC;
	*Pointer = I2CSlaveAddress << 1;
	Pointer++;
	*Pointer = StartAddress;
	Pointer++;
	*Pointer = *Buffer;
	Pointer++;
	*Pointer = CRC8(BufferCRCC, 3, CRC_KEY);

	for (i = 1; i < Length; i++)
	{
		Pointer++;
		Buffer++;
		*Pointer = *Buffer;
		*(Pointer + 1) = CRC8(Pointer, 1, CRC_KEY);
		Pointer++;
	}

	result = I2CSendBytes(I2CSlaveAddress, BufferCRCC + 1, 2 * Length + 1, &SentByte);

	// free(BufferCRC);
	BufferCRCC[0] = 255;

	return result;
}

int I2CReadBlockWithCRC(unsigned char I2CSlaveAddress, unsigned char Register, unsigned char *Buffer, unsigned char Length)
{
	unsigned char TargetRegister = Register;
	unsigned int SentByte = 0;
	// unsigned char *ReadData = NULL;		//去掉NULL可以吧
	unsigned char *ReadData;
	unsigned int ReadDataCount = 0;
	unsigned char CRCInput[2];

	unsigned char CRCC = 0;
	int ReadStatus = 0;
	int WriteStatus = 0;
	int i;

	ReadData = startData_;

	WriteStatus = I2CSendBytes(I2CSlaveAddress, &TargetRegister, 1, &SentByte);

	ReadStatus = I2CReadBytes(I2CSlaveAddress, ReadData, 2 * Length, &ReadDataCount);

	if (ReadStatus != 0 || WriteStatus != 0)
	{
		// free(StartData);
		startData_[0] = 255;

		return 1;
	}

	CRCInput[0] = (I2CSlaveAddress << 1) + 1;
	CRCInput[1] = *ReadData;

	CRCC = CRC8(CRCInput, 2, CRC_KEY);

	ReadData++;
	if (CRCC != *ReadData)
	{
		// free(StartData);
		startData_[0] = 255;
		return 1;
	}
	else
		*Buffer = *(ReadData - 1);

	for (i = 1; i < Length; i++)
	{
		ReadData++;
		CRCC = CRC8(ReadData, 1, CRC_KEY);
		ReadData++;
		Buffer++;

		if (CRCC != *ReadData)
		{
			// free(StartData);
			startData_[0] = 255;

			return 1;
		}
		else
			*Buffer = *(ReadData - 1);
	}

	// free(StartData);
	startData_[0] = 255;

	return 0;
}

int I2CWriteRegisterByteWithCRC(unsigned char I2CSlaveAddress, unsigned char Register, unsigned char Data)
{
	unsigned char DataBuffer[4];
	unsigned int SentByte = 0;

	DataBuffer[0] = I2CSlaveAddress << 1;
	DataBuffer[1] = Register;
	DataBuffer[2] = Data;
	DataBuffer[3] = CRC8(DataBuffer, 3, CRC_KEY);

	return (I2CSendBytes(I2CSlaveAddress, DataBuffer + 1, 3, &SentByte));
}

int I2CReadRegisterByteWithCRC(unsigned char I2CSlaveAddress, unsigned char Register, unsigned char *Data)
{
	unsigned char TargetRegister = Register;
	unsigned int SentByte = 0;
	unsigned char ReadData[2];
	unsigned int ReadDataCount = 0;
	unsigned char CRCInput[2];
	unsigned char CRCC = 0;
	int ReadStatus = 0;
	int WriteStatus = 0;

	WriteStatus = I2CSendBytes(I2CSlaveAddress, &TargetRegister, 1, &SentByte);

	ReadStatus = I2CReadBytes(I2CSlaveAddress, ReadData, 2, &ReadDataCount);

	if (ReadStatus != 0 || WriteStatus != 0)
	{
		return 1;
	}

	CRCInput[0] = (I2CSlaveAddress << 1) + 1;
	CRCInput[1] = ReadData[0];

	CRCC = CRC8(CRCInput, 2, CRC_KEY);

	if (CRCC != ReadData[1])
		return 1;

	*Data = ReadData[0];
	return 0;
}

int GetADCGainOffset(unsigned char I2CSlaveAddress)
{
	int result;
	result = I2CReadRegisterByteWithCRC(I2CSlaveAddress, ADCGAIN1, &(Registers_AFE1.ADCGain1.ADCGain1Byte));
	result += I2CReadRegisterByteWithCRC(I2CSlaveAddress, ADCGAIN2, &(Registers_AFE1.ADCGain2.ADCGain2Byte));
	result += I2CReadRegisterByteWithCRC(I2CSlaveAddress, ADCOFFSET, &(Registers_AFE1.ADCOffset));
	return result;
}

int ConfigureBqMaximo(unsigned char I2CSlaveAddress)
{
	int result = 0;
	unsigned char bqMaximoProtectionConfig[8];

	// result = I2CWriteBlockWithOutCRC(I2CSlaveAddress, SYS_CTRL1, &(Registers.SysCtrl1.SysCtrl1Byte), 8);
	// result += I2CReadBlockWithOutCRC(I2CSlaveAddress, SYS_CTRL1, bqMaximoProtectionConfig, 8);

	result = I2CWriteBlockWithCRC(I2CSlaveAddress, SYS_CTRL1, &(Registers_AFE1.SysCtrl1.SysCtrl1Byte), 8);
	result += I2CReadBlockWithCRC(I2CSlaveAddress, SYS_CTRL1, bqMaximoProtectionConfig, 8);

#if 1
	if ((bqMaximoProtectionConfig[0] != Registers_AFE1.SysCtrl1.SysCtrl1Byte) || (bqMaximoProtectionConfig[1] != Registers_AFE1.SysCtrl2.SysCtrl2Byte) || (bqMaximoProtectionConfig[2] != Registers_AFE1.Protect1.Protect1Byte) || (bqMaximoProtectionConfig[3] != Registers_AFE1.Protect2.Protect2Byte) || (bqMaximoProtectionConfig[4] != Registers_AFE1.Protect3.Protect3Byte) || (bqMaximoProtectionConfig[5] != Registers_AFE1.OVTrip) || (bqMaximoProtectionConfig[6] != Registers_AFE1.UVTrip) || (bqMaximoProtectionConfig[7] != Registers_AFE1.CCCfg))
	{
		result = 1;
	}
#endif
	return result;
}

// 这个模拟前端保护，会产生alert信号，只要不用模拟前端本身的CHG，DSG引脚控制不会产生任何问题
// 使用，就把这个引脚清除1信号。
int InitialisebqMaximo(unsigned char I2CSlaveAddress)
{
	int result = 0;

	// 休眠带电，RTC唤醒不需要操作，不然会关管子
	// 必须要刚开机的时候，拿出标志位之后，要改回来，不然扛着RTC的标志位，还没进入休眠，别的地方就断电，或者另外的途径进入休眠
	// 这里就出问题了。采集不到电压电流
	gu8_WakeUp_Type = FlashReadOneHalfWord(FLASH_ADDR_WAKE_TYPE);
	if (gu8_WakeUp_Type == FLASH_VALUE_WAKE_RTC)
	{
		FlashWriteOneHalfWord(FLASH_ADDR_WAKE_TYPE, FLASH_VALUE_WAKE_OTHER);
		// return result;
	}

	// MOS状态不能变
	I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, &(Registers_AFE1.SysCtrl2.SysCtrl2Byte));

	Registers_AFE1.SysCtrl1.SysCtrl1Byte = 0;
	Registers_AFE1.SysCtrl1.SysCtrl1Bit.ADC_EN = 1;
	Registers_AFE1.SysCtrl1.SysCtrl1Bit.TEMP_SEL = 1;

	// Registers_AFE1.SysCtrl2.SysCtrl2Byte = 0;
	// Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON = 1;		//强制打开一下下
	// Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON = 1;		//强制打开一下下
	Registers_AFE1.SysCtrl2.SysCtrl2Bit.CC_EN = 1; // 电流读取允许

	Registers_AFE1.CCCfg = 0x19;

	//	Registers_AFE1.Protect1.Protect1Bit.RSNS = 1;
	//	Registers_AFE1.Protect1.Protect1Bit.SCD_DELAY = SCDDelay;    //70us
	//	Registers_AFE1.Protect1.Protect1Bit.SCD_THRESH = SCDThresh;  //40A

	//	Registers_AFE1.Protect2.Protect2Bit.OCD_DELAY = OCDDelay;    //8ms
	//	Registers_AFE1.Protect2.Protect2Bit.OCD_THRESH = OCDThresh;  //20A

	Registers_AFE1.Protect3.Protect3Bit.OV_DELAY = OVDelay; // 2s
	Registers_AFE1.Protect3.Protect3Bit.UV_DELAY = UVDelay; // 4s

	result = GetADCGainOffset(I2CSlaveAddress);

	g_stBq769x0_Read_AFE1.f32Gain = (365 + ((Registers_AFE1.ADCGain1.ADCGain1Byte & 0x0C) << 1) + ((Registers_AFE1.ADCGain2.ADCGain2Byte & 0xE0) >> 5)) / 1000.0;
	g_stBq769x0_Read_AFE1.i16Gain = 365 + ((Registers_AFE1.ADCGain1.ADCGain1Byte & 0x0C) << 1) + ((Registers_AFE1.ADCGain2.ADCGain2Byte & 0xE0) >> 5);
	Registers_AFE1.OVTrip = (unsigned char)((((unsigned short)((OVPThreshold - Registers_AFE1.ADCOffset) / g_stBq769x0_Read_AFE1.f32Gain + 0.5) - OV_THRESH_BASE) >> 4) & 0xFF);
	Registers_AFE1.UVTrip = (unsigned char)((((unsigned short)((UVPThreshold - Registers_AFE1.ADCOffset) / g_stBq769x0_Read_AFE1.f32Gain + 0.5) - UV_THRESH_BASE) >> 4) & 0xFF);
	result = ConfigureBqMaximo(I2CSlaveAddress);

	return result;
}

UINT8 App_AFEshutdown(void)
{
	UINT8 data = 0;
	// I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1,0x04,&data);
	// data &= 0xfc;
	I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, 0x04, data);
	data |= 1;
	I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, 0x04, data);
	data = 0;
	data |= 2;
	I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, 0x04, data);
	return 0;
}

// 初始化IIC
void InitAFE1_F6F7(void)
{
	I2C_InitTypeDef I2C_InitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;
	/* I2C configuration */

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE); // 开启I2C2外设时钟

	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable; // 0: 模拟噪声滤波器开启
	I2C_InitStruct.I2C_DigitalFilter = 0x00;				   // 数字滤波器关闭
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	// I2C_InitStruct.I2C_Timing = 0x00901D2B;						//这句话我不管先，是一个excel计算出来的，8MHz
	I2C_InitStruct.I2C_Timing = 0x30E3363D; // 48MHz
	/* I2C Peripheral Enable */
	I2C_Cmd(I2C2, ENABLE);
	/* Apply I2C configuration after enabling it */
	I2C_Init(I2C2, &I2C_InitStruct);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	InitialisebqMaximo(DEVICE_ADDR_AFE1);
}

// 初始化IIC
void InitAFE1(void)
{
	I2C_InitTypeDef I2C_InitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;
	// RCC_I2CCLKConfig(RCC_I2C1CLK_SYSCLK);		//I2C1需要这个
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

	/* I2C configuration */
	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable; // 0: 模拟噪声滤波器开启
	I2C_InitStruct.I2C_DigitalFilter = 0x00;				   // 数字滤波器关闭
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStruct.I2C_Timing = 0x00901D2B; // 这句话我不管先，是一个excel计算出来的，8MHz
	// I2C_InitStruct.I2C_Timing = 0x30E3363D;							//48MHz

	/* I2C Peripheral Enable */
	I2C_Cmd(I2C2, ENABLE);
	/* Apply I2C configuration after enabling it */
	I2C_Init(I2C2, &I2C_InitStruct);

	/* Connect PXx to I2C_SCL*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*!< Configure sEE_I2C pins: SDA */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_1); // 这个AF选项找芯片手册非reg版
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_1); // 只有PA和PB才会有

	InitialisebqMaximo(DEVICE_ADDR_AFE1);

	InitShortCur();
}

int UpdateVoltageFromBqMaximo(unsigned char I2CSlaveAddress)
{
	int Result = 0, i = 0;
	// unsigned char *pRawADCData = NULL;
	unsigned char *pRawADCData;
	unsigned int iTemp = 0;
	long lTemp1 = 0;
	INT32 iTemp2 = 0;

	Result = I2CReadBlockWithCRC(I2CSlaveAddress, VC1_HI_BYTE, &(Registers_AFE1.VCell1.VCell1Byte.VC1_HI), 40);

	pRawADCData = &Registers_AFE1.VCell1.VCell1Byte.VC1_HI;
	for (i = 0; i < 20; i++)
	{
		if (i < 15)
		{ // Vcell
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0x3fff;
			lTemp1 = ((long)iTemp * g_stBq769x0_Read_AFE1.i16Gain) / 1000;
			lTemp1 += (signed char)(Registers_AFE1.ADCOffset);
			UPDNLMT16(lTemp1, 65535, 0);
			g_stBq769x0_Read_AFE1.u16VCell[i] = (UINT16)lTemp1;
		}
		else if (i == 15)
		{ // Vbat
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0xffff;
			lTemp1 = ((long)iTemp * g_stBq769x0_Read_AFE1.i16Gain * 4) / 1000;
			lTemp1 += (signed char)(Registers_AFE1.ADCOffset * SeriesNum);
			UPDNLMT16(lTemp1, 100000, 0);
			g_stBq769x0_Read_AFE1.u32VBat = lTemp1;
		}
		else if (i < 19)
		{ // Temp
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0x3fff;
			iTemp = GetEndValue(iSheldTemp_10K_AFE, (UINT16)LENGTH_TBLTEMP_AFE_10K, (UINT16)iTemp);
			/*
			g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16] = (((((INT32)iTemp) << 10) - g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16]) >> 3)\
				+ (g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16]);
			g_stBq769x0_Read_AFE1.u16TempBat[i - 16] = (UINT16)((g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16] + 512) >> 10);
			*/
			g_stBq769x0_Read_AFE1.u16TempBat[i - 16] = (UINT16)iTemp;
		}
		else
		{ // CC
			iTemp2 = ((INT32)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0xffff;
			// g_stBq769x0_Read.i32Current = (INT16)(iTemp2 * 8440 /5000);
			g_stBq769x0_Read_AFE1.u16Current = (UINT16)(iTemp2);
		}
		pRawADCData += 2;
	}

	return Result;
}

// 分为0-3part，解决10ms时基问题，分2part能解决200ms时基，但是不能解决10ms时基
int UpdateVoltageFromBqMaximo_Partition(unsigned char I2CSlaveAddress, UINT8 Part)
{
	int Result = 0, i = 0;
	// unsigned char *pRawADCData = NULL;
	unsigned char *pRawADCData;
	unsigned int iTemp = 0;
	long lTemp1 = 0;
	INT32 iTemp2 = 0;

	Result = I2CReadBlockWithCRC(I2CSlaveAddress, VC1_HI_BYTE + Part * 10, &(Registers_AFE1.VCell1.VCell1Byte.VC1_HI) + Part * 10, 10);

	pRawADCData = &(Registers_AFE1.VCell1.VCell1Byte.VC1_HI) + Part * 10;
	for (i = Part * 5; i < Part * 5 + 5; i++)
	{
		if (i < 15)
		{ // Vcell
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0x3fff;
			lTemp1 = ((long)iTemp * g_stBq769x0_Read_AFE1.i16Gain) / 1000;
			lTemp1 += (signed char)(Registers_AFE1.ADCOffset);
			UPDNLMT16(lTemp1, 65535, 0);
			g_stBq769x0_Read_AFE1.u16VCell[i] = (UINT16)lTemp1;
		}
		else if (i == 15)
		{ // Vbat
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0xffff;
			lTemp1 = ((long)iTemp * g_stBq769x0_Read_AFE1.i16Gain * 4) / 1000;
			lTemp1 += (signed char)(Registers_AFE1.ADCOffset * SeriesNum);
			UPDNLMT16(lTemp1, 100000, 0);
			g_stBq769x0_Read_AFE1.u32VBat = lTemp1;
		}
		else if (i < 19)
		{ // Temp
			iTemp = ((unsigned int)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0x3fff;
			iTemp = GetEndValue(iSheldTemp_10K_AFE, (UINT16)LENGTH_TBLTEMP_AFE_10K, (UINT16)iTemp);
			/*
			g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16] = (((((INT32)iTemp) << 10) - g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16]) >> 3)\
				+ (g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16]);
			g_stBq769x0_Read_AFE1.u16TempBat[i - 16] = (UINT16)((g_stBq769x0_Read_AFE1.i32TempBatFilter[i - 16] + 512) >> 10);
			*/
			g_stBq769x0_Read_AFE1.u16TempBat[i - 16] = (UINT16)iTemp;
		}
		else
		{ // CC
			iTemp2 = ((INT32)(*pRawADCData << 8) + *(pRawADCData + 1)) & 0xffff;
			// g_stBq769x0_Read.i32Current = (INT16)(iTemp2 * 8440 /5000);
			g_stBq769x0_Read_AFE1.u16Current = (UINT16)(iTemp2);
		}
		pRawADCData += 2;
	}

	return Result;
}
