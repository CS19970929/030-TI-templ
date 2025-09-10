#ifndef I2C_AFE1_H
#define I2C_AFE1_H

#define FLAG_TIMEOUT         ((uint32_t)0x1000)
//#define LONG_TIMEOUT         ((uint32_t)(10 * FLAG_TIMEOUT))
//原来*10，会出现上位机所有数据都乱的情况，现在修改，就没了，具体原因不明
#define LONG_TIMEOUT         ((uint32_t)(FLAG_TIMEOUT))

#define I2C_SYSTEM

#define I2C_RW_W	0
#define I2C_RW_R	1

#define I2C_AFE		I2C2
#define DELAY_US_IIC_AFE1	6

//IO方向设置
#define SDA_IN()  {GPIOF->MODER&=0xFFFF3FFF;GPIOF->MODER|=(UINT32)0<<14;}		//默认值为00，既不上拉也不下拉输入。
#define SDA_OUT() {GPIOF->MODER&=0xFFFF3FFF;GPIOF->MODER|=(UINT32)1<<14;}		//关于输出类型和输出速度，在默认值的Init函数配好，回来立刻生效

//IO操作函数	 
#define IIC_SCL    PORT_OUT_GPIOF->bit6 	//SCL
#define IIC_SDA    PORT_OUT_GPIOF->bit7		//SDA
#define READ_SDA   (uint16_t)(GPIOF->IDR&GPIO_Pin_7)  //输入SDA 



enum AfeTsChannelArray
{
	AFE_TS_1 = 0,
	AFE_TS_2,
	AFE_TS_3,
	
	AFE_TS_NUM
};


struct stBq769x0_Read{									/* AD Read	*/
	UINT16		u16VCell[15];                   // mv
	float		f32Gain;
	INT16		i16Gain;
	INT8		i8Offset;
	UINT16		u16TempBat[AFE_TS_NUM];
	INT32		i32TempBatFilter[AFE_TS_NUM];
	UINT32		u32VBat;                        // mv
	UINT16      u16Current;                     // mA
};

#define LENGTH_TBLTEMP_AFE_10K    ((UINT16)56)

extern const unsigned int OVPThreshold;
extern const unsigned int UVPThreshold;
extern const unsigned char SCDDelay;
extern const unsigned char SCDThresh;
extern const unsigned char OCDDelay;
extern const unsigned char OCDThresh;
extern const unsigned char OVDelay;
extern const unsigned char UVDelay;


extern RegisterGroup Registers_AFE1;
extern struct stBq769x0_Read g_stBq769x0_Read_AFE1;


int I2CWriteRegisterByteWithCRC(unsigned char I2CSlaveAddress, unsigned char Register, unsigned char Data);
int I2CReadRegisterByteWithCRC(unsigned char I2CSlaveAddress, unsigned char Register, unsigned char *Data);

void InitAFE1(void);
int InitialisebqMaximo(unsigned char I2CSlaveAddress);
int UpdateVoltageFromBqMaximo(unsigned char I2CSlaveAddress);
int I2CWriteBlockWithCRC(unsigned char I2CSlaveAddress, unsigned char StartAddress, unsigned char *Buffer, unsigned char Length);
int UpdateVoltageFromBqMaximo_Partition(unsigned char I2CSlaveAddress, UINT8 Part);
UINT8 App_AFEshutdown(void);

#endif	/* I2C_AFE1_H */

