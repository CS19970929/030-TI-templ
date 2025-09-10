#ifndef LED_BUZZER_H
#define LED_BUZZER_H

typedef enum _LED_STATUS {
	LED_OPEN = 1, 
	LED_CLOSE = 0
}LED_STATUS;

/*
typedef enum _LED_COMMAND {
	LED_STARTUP = 0,
	LED_FAULT_MONITOR,
	LED_SOC_LOW,
	LED_BATTER_ERR,
	LED_LOAD_ERR,
	LED_RESCUE_PIVOT,
	LED_EMERGENCY,
	LED_RESTART_MPU
}LED_COMMAND;
*/

//还是决定换一种新架构写法，不参照休眠代码的架构
typedef enum _LED_COMMAND {
	LED_STARTUP = 0,
	LED_FAULT_MONITOR,
	LED_RESCUE_PIVOT,
	LED_EMERGENCY,
	LED_RESTART_MPU
}LED_COMMAND;


typedef union _LED_MODE{
    UINT8   all;
    struct LedMode_FlagBit {
		UINT8 b1_SocLow        		:1;	
		UINT8 b1_BatteryErr      	:1;
		UINT8 b1_LoadErr      		:1;
		UINT8 b1_Emergency      	:1;
		
		UINT8 b1_RestartMPU      	:1;
		UINT8 Res					:3;
     }bits;
}LED_MODE;


//驱动模块
#define MCUO_LED_GREEN		(PORT_OUT_GPIOC->bit14)		//LED红灯
#define MCUO_LED_RED		(PORT_OUT_GPIOC->bit15)		//LED绿灯
#define MCUO_BUZZER 		(PORT_OUT_GPIOA->bit12)		//蜂鸣器
#define MCUI_EMERGENCY_KEY 	(PORT_IN_GPIOA->bit0)		//


extern UINT8 gu8_MosOpenFlag;


void InitLED(void);
void APP_LED(void);

#endif	/* LED_BUZZER_H */

