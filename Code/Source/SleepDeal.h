#ifndef SLEEPDEAL_H
#define SLEEPDEAL_H

typedef enum _SLEEP_CNT {
FIRST = 0, HICCUP
}SLEEP_CNT;

typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, HICCUP_MODE, DEEP_MODE
}SLEEP_MODE;

#define RTC_WT_Protect 		5         	//���ֱ����������ߴ���ʱ��
#define RTC_WT_Normal 		5         	//����״̬�������ߴ���ʱ��
#define RTC_WT_Force 		5         	//�ⲿǿ�ƽ������ߴ���ʱ��

#define SleepInitOC 		10	  		//200msʱ��������ģʽ��������ʼ����ʱ���ж��Ƿ��ٽ�������
#define SleepInitCBC 		10	  		//200msʱ��������ģʽ��������ʼ����ʱ���ж��Ƿ��ٽ�������
#define SleepInitNormal1 	100	  		//200msʱ��������ģʽ��������ʱ�ڼ��ж�20s�Ƿ��ٽ�������
#define SleepInitNormal2 	150	  		//200msʱ��������ģʽ���������ֵ������ӳ���30s�ж�

//��������������־λ��ʵ������һ��״̬�����?(���κ�ʱ��SleepModeֻ��һ�������������ֶ������?)
//�����Ҿ������ģ���Ϊδ����Ŀ�п��ܳ��ֶ�������?ʱ����
union SLEEP_MODE{
    UINT16   all;
    struct SleepModeFlagBit {
		UINT8 b1TestSleep        	:1;		//b1ForceToSleep_L2���Դ��棬ȡ��
		UINT8 b1NormalSleep_L1      :1;
		UINT8 b1NormalSleep_L2      :1;
		UINT8 b1NormalSleep_L3      :1;
		
		UINT8 b1ForceToSleep_L1     :1;		//�ⲿ�ٿؽ����һ������?
        UINT8 b1ForceToSleep_L2     :1;		//�ⲿ�ٿؽ���ڶ�������?
		UINT8 b1ForceToSleep_L3     :1;		//�ⲿ�ٿؽ������������?
        UINT8 b1ForceToSleep_L1_Out :1;		//�ⲿ�ٿص�һ�������˳�����ģʽ��־λ

		UINT8 b1OverCurSleep        :1;
        UINT8 b1OverVdeltaSleep     :1;
		UINT8 b1CBCSleep       		:1;
		UINT8 b1VcellOVP			:1;
		
		UINT8 b1VcellUVP			:1;
		UINT8 b1_ToSleepFlag		:1;		//��Ϊ1ʱ��1s��������ߣ����ʱ��Ϊ0
		UINT8 Res2					:2;		//8L����÷������ź�STM32һ��
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

extern uint8_t reset_sleep_state;


void App_NormalSleepTest(void);
void App_SleepDeal(void);
void App_SleepTest(void);
void IsSleepStartUp(void);
void entersleep(enum _SLEEP_MODE mode);

#endif	/* SLEEPDEAL_H */

