#ifndef SYSTEM_H
#define SYSTEM_H

typedef struct _16_Bits_Struct {
    UINT16 bit0 : 1;
    UINT16 bit1 : 1;
    UINT16 bit2 : 1;
    UINT16 bit3 : 1;
    UINT16 bit4 : 1;
    UINT16 bit5 : 1;
    UINT16 bit6 : 1;
    UINT16 bit7 : 1;
    UINT16 bit8 : 1;
    UINT16 bit9 : 1;
    UINT16 bit10 : 1;
    UINT16 bit11 : 1;
    UINT16 bit12 : 1;
    UINT16 bit13 : 1;
    UINT16 bit14 : 1;
    UINT16 bit15 : 1;
} Bits_16_TypeDef;

#define PORT_OUT_GPIOA    ((Bits_16_TypeDef *)(&(GPIOA->ODR)))
#define PORT_OUT_GPIOB    ((Bits_16_TypeDef *)(&(GPIOB->ODR)))
#define PORT_OUT_GPIOC    ((Bits_16_TypeDef *)(&(GPIOC->ODR)))
#define PORT_OUT_GPIOF    ((Bits_16_TypeDef *)(&(GPIOF->ODR)))

#define PORT_IN_GPIOA    ((Bits_16_TypeDef *)(&(GPIOA->IDR)))
#define PORT_IN_GPIOB    ((Bits_16_TypeDef *)(&(GPIOB->IDR)))
#define PORT_IN_GPIOC    ((Bits_16_TypeDef *)(&(GPIOC->IDR)))
#define PORT_IN_GPIOF    ((Bits_16_TypeDef *)(&(GPIOF->IDR)))



#define MCUO_DEBUG_LED1 	(PORT_OUT_GPIOB->bit2)		//LED1
//#define MCUO_DEBUG_LED2 	PDout(3)		//PB3，LED2

//电源模块
#define MCUO_PWSV_STB 		(PORT_OUT_GPIOB->bit1)		//
#define MCUO_PWSV_LDO		(PORT_OUT_GPIOB->bit5)		//
#define MCUO_PWSV_CTR		(PORT_OUT_GPIOB->bit15)		//
#define MCUO_BEL_EN		    (PORT_OUT_GPIOB->bit15)		//
#define MCUO_PW_RS485_EN	(PORT_OUT_GPIOB->bit7)		//


//驱动模块
#define MCUO_RELAY_PRE		(PORT_OUT_GPIOB->bit13)		//预放预充继电器，防止打开瞬间大电流
#define MCUO_RELAY_MAIN		(PORT_OUT_GPIOB->bit13)		//主继电器
#define MCUO_RELAY_CHG 		(PORT_OUT_GPIOB->bit13)		//PB13，充电RELAY
#define MCUO_RELAY_DSG 		(PORT_OUT_GPIOB->bit14)		//PB14，放电RELAY

#define MCUO_MOS_PRE 		(PORT_OUT_GPIOA->bit8)		//PB13，充电MOS
#define MCUO_MOS_CHG 		(PORT_OUT_GPIOB->bit13)		//PB13，充电MOS
#define MCUO_MOS_DSG 		(PORT_OUT_GPIOB->bit14)		//PB14，放电MOS
#define MCUO_SD_DRV_CHG 	(PORT_OUT_GPIOA->bit11)		//驱动信号，类似扫地机

#define MCUI_ENI_DI1		(PORT_IN_GPIOC->bit13)		//

#define MCUO_RELAY_HEAT 	(PORT_OUT_GPIOA->bit12)		//PA12，加热继电器
#define MCUO_RELAY_COOL		(PORT_OUT_GPIOA->bit12)		//PE11，制冷继电器，先用放电继电器


//AFE模块
#define MCUO_WAKEUP_AFE 	(PORT_OUT_GPIOF->bit7)		//WAKEUP_AFE
//#define MCUO_SLEEP_AFE 	PBout(7)		//PB7，930,940模拟前端无法关，所以只能断电
#define MCUO_AFE_ALARM 	    (PORT_OUT_GPIOA->bit1)

//其它模块
#define MCUO_CB_6_16 		(PORT_OUT_GPIOB->bit7)		//CB6均衡，覆盖类型
#define MCUO_E2PR_WP		(PORT_OUT_GPIOA->bit15)		//EEPROM写保护
#define MCUI_CBC_DSG 		(PORT_IN_GPIOB->bit12)		//PB12



typedef enum NVIC_ONOFF {
	NVIC_INIT = 0,
	NVIC_OPEN ,
	NVIC_CLOSE,	
}NVIC_OnOFF;


union SYS_TIME {			//TODO
    UINT16 all;
    struct StatusSysTimeFlagBit {
        UINT8 b1Sys10msFlag1        :1;
        UINT8 b1Sys10msFlag2        :1;
        UINT8 b1Sys10msFlag3        :1;
        UINT8 b1Sys10msFlag4        :1;
		
        UINT8 b1Sys10msFlag5        :1;
		//UINT8 b1Sys20msFlag        	:1;
		UINT8 b1Sys1msFlag        	:1;	
		UINT8 b1Sys50msFlag        	:1;
		UINT8 b1Sys100msFlag       	:1;

		UINT8 b1Sys200msFlag1       :1;
		UINT8 b1Sys200msFlag2       :1;
		UINT8 b1Sys200msFlag3       :1;
		UINT8 b1Sys200msFlag4       :1;
		
		UINT8 b1Sys200msFlag5       :1;
		UINT8 b1Sys1000msFlag1      :1;
		UINT8 b1Sys1000msFlag2      :1;
		UINT8 b1Sys1000msFlag3      :1;
     }bits;
};


struct CBC_ELEMENT {
	UINT8 u8CBC_CHG_ErrFlag;	//出现CBC保护标志位
	UINT8 u8CBC_CHG_Cnt;		//出现充电CBC的次数
	UINT8 u8CBC_DSG_ErrFlag;	//出现CBC保护标志位
	UINT8 u8CBC_DSG_Cnt;		//出现放电CBC的次数
};


#define Feed_IWatchDog IWDG_ReloadCounter()


extern UINT8 gu8_200msAccClock_Flag;

extern struct CBC_ELEMENT CBC_Element;
extern volatile union SYS_TIME g_st_SysTimeFlag;

void InitClock(void);
void InitTimer(void);
void InitIO(void);
void InitDelay(void);
void Init_IWDG(void);

void __delay_us(UINT32 nus);
void __delay_ms(UINT16 ms);
void App_SysTime(void);

#endif	/* SYSTEM_H */


