#ifndef SLEEPDEAL_H
#define SLEEPDEAL_H

typedef enum _SLEEP_CNT {
FIRST = 0, HICCUP
}SLEEP_CNT;

typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, HICCUP_MODE, DEEP_MODE
}SLEEP_MODE;

#define RTC_WT_Protect 		5         	//出现保护进入休眠打嗝时间
#define RTC_WT_Normal 		5         	//正常状态进入休眠打嗝时间
#define RTC_WT_Force 		5         	//外部强制进入休眠打嗝时间

#define SleepInitOC 		10	  		//200ms时基，打嗝模式起来，初始化延时后判断是否再进入休眠
#define SleepInitCBC 		10	  		//200ms时基，打嗝模式起来，初始化延时后判断是否再进入休眠
#define SleepInitNormal1 	100	  		//200ms时基，打嗝模式起来，延时期间判断20s是否再进入休眠
#define SleepInitNormal2 	150	  		//200ms时基，打嗝模式起来，发现电流则延长至30s判断

//以下这个联合体标志位其实可以用一个状态机替代(因任何时间SleepMode只有一种情况而不会出现多种情况)
//但是我决定不改，因为未来项目有可能出现多种情况同时进行
union SLEEP_MODE{
    UINT16   all;
    struct SleepModeFlagBit {
		UINT8 b1TestSleep        	:1;		//b1ForceToSleep_L2可以代替，取消
		UINT8 b1NormalSleep_L1      :1;
		UINT8 b1NormalSleep_L2      :1;
		UINT8 b1NormalSleep_L3      :1;
		
		UINT8 b1ForceToSleep_L1     :1;		//外部操控进入第一级休眠
        UINT8 b1ForceToSleep_L2     :1;		//外部操控进入第二级休眠
		UINT8 b1ForceToSleep_L3     :1;		//外部操控进入第三级休眠
        UINT8 b1ForceToSleep_L1_Out :1;		//外部操控第一级休眠退出打嗝模式标志位

		UINT8 b1OverCurSleep        :1;
        UINT8 b1OverVdeltaSleep     :1;
		UINT8 b1CBCSleep       		:1;
		UINT8 b1VcellOVP			:1;
		
		UINT8 b1VcellUVP			:1;
		UINT8 b1_ToSleepFlag		:1;		//当为1时，1s后进入休眠，别的时间为0
		UINT8 Res2					:2;		//8L必须得反过来才和STM32一致
     }bits;
};


enum SLEEP_STATUS{
	SLEEP_HICCUP_SHIFT = 0,	
	SLEEP_HICCUP_CONTINUE,
	SLEEP_HICCUP_OVERCUR,
	SLEEP_HICCUP_OVDELTA,
	SLEEP_HICCUP_CBC,
	SLEEP_HICCUP_FORCED,
	SLEEP_HICCUP_VCELLOVP,
	SLEEP_HICCUP_VCELLUVP,
	SLEEP_HICCUP_NORMAL_SELECT,
	SLEEP_HICCUP_NORMAL_L1,
	SLEEP_HICCUP_NORMAL_L2,
	SLEEP_HICCUP_NORMAL_L3,
	SLEEP_HICCUP_TEST
};



#define Sleep_min  		{1000, 1, 		1000, 1, 	0, 		0, 		0, 		0}
#define Sleep_default  	{5000, 60, 		1000, 60, 	30,		30,		0, 		0}
#define Sleep_max  		{5000, 5760, 	5000, 1440,	100,	100, 	20000, 	20000}

extern volatile union SLEEP_MODE Sleep_Mode;
extern enum SLEEP_STATUS Sleep_Status;
extern UINT8 RTC_ExtComCnt;


void App_NormalSleepTest(void);
void App_SleepDeal(void);
void App_SleepTest(void);
void IsSleepStartUp(void);

#endif	/* SLEEPDEAL_H */

