#ifndef SOCENHANCE_H
#define SOCENHANCE_H

// #include "stm32f10x.h"
#include "stm32f0xx.h"


#define SOC_Size_TableCanSet 	(UINT16)42
#define SOC_Size_LiFePO 		(UINT16)42
#define SOC_Size_TernaryLi 		(UINT16)42
#define SOC_Size_LiFePO2 		(UINT16)42

enum SOC_TABLE_SELECT {
	SOC_TABLE_TEST = 0,
	SOC_TABLE_LIFEPO,
	SOC_TABLE_TERNARYLI,
	SOC_TABLE_LIFEPO2
};

extern const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO];
extern const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi];
extern const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2];


#define E2P_AdressNum 			(UINT16)16

struct SOC_ENHANCE_ELEMENT {
	//ֻ��Ҫ��ֵһ�εĲ���
	UINT16 u16_SOC_Ah;				//Ah*10
	UINT16 u16_SOC_CycleT_Ever;		//�Ѿ�ѭ������
	UINT16 u16_SOC_CycleT_Limit;	//��ؿ�ѭ������
    UINT16 u16_SOC_TableSelect;		//0:�ɵ������顣1:�������(��������)��2:��Ԫ�3:�������2(������)����Ϊ42����ֵ��
    UINT16 u16_SOC_DsgVcell_Limit;	//mV�������ŵ�ﵽ�ĵ�����С��ѹֵ����ֵ+200��ԭ
    UINT16 u16_SOC_0_Vol;			//mV��SOCΪ0ʱ�ĵ�ѹ
    UINT16 u16_SOC_100_Vol;			//mV��SOCΪ100ʱ�ĵ�ѹ    
	UINT16 SOC_E2P_Adress[E2P_AdressNum];			//��Ҫ16��EEPROM��ַ���������
	UINT16 SOC_Table_CanSet[SOC_Size_TableCanSet];	//�ⲿ���ƿ����OCV����
	UINT8 u8_SetSocOnce;			//���Ҫ�޸�SOC��ֵ��
	UINT8 u8_LargeCurFlag_Chg;		//�ⲿʹ�ô������ĩ�˺�����ı�־λ�����ĩ�˴������磬0.5C���ϣ�����1������Ĭ��Ϊ0��
									//��һ��Ҫ��1�����ĩ��ʵ��û������100%������˼����1��
	UINT8 u8_LargeCurFlag_Dsg;		//�ⲿʹ�ô������ĩ�˺����ŵı�־λ�����ĩ�˴�����ŵ磬0.5C���ϣ�����1������Ĭ��Ϊ0��
									//��һ��Ҫ��1�����ĩ��ʵ��û������0%������˼����1��
									//���ѭ����λ��ǲ��ܹ������100%����ɵ���

	//��Ҫ����ϸ�ֵ�Ĳ���
	UINT16 u16_VCellMax;			//mV
	UINT16 u16_VCellMin;        	//mV����6��(ֻ��6��)�͵�16���������������
	UINT16 u16_Ichg;				//A*10
	UINT16 u16_Idsg;				//A*10

	//�ܻ�ȡ�����Ϣ�Ĳ���
	UINT16 u16_SOC_InitOver;		//SOC������ʼ����ϱ�־λ��1:��ʼ����ɡ�0:��û��ʼ����
	UINT16 u16_SOC_CailFaultCnt;	//���ֹ���������ҪOCV�ٴ�У׼������
	UINT8 u8_SOC;					//Soc��ֵ
	UINT8 u8_SOH;					//��ؽ���״̬
	UINT16 u16_CapacityNow;			//Ah*100����ǰʣ������
	UINT16 u16_CapacityFull; 		//Ah*100����������
	UINT16 u16_CapacityFactory;		//Ah*100����������������Ҫ
	UINT16 u16_Cycle_times;			//ѭ������
	//��������Ϊ���Ի�ȡ��Ϣ����
	UINT8 u8_SOC_OCV_Cali;			//SocУ׼ֵ
	UINT8 u8_n_CoulombicEff;		//����Ч��
	UINT8 u8_n_InnerCorrect;		//�ڻ�У׼����

	//��Ϣ������
	UINT16 u16_RefreshData_Flag;	//ˢ�����ݱ�־λ��X:��Ҫˢ�£�0:����Ҫ��ˢ�º�������0
									//1��SOC���ݱ���ˢ�¡�
									//2��SOC���㹦��(��Ϊѭ������ˢ��Ϊ0)��
									//3��SOCֵ�ⲿ������
									//���ܱ��档
};

extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

struct SOC_CALCULATE_ELEMENT {	
	//InitSOC_IntEnhance赋值类型
	UINT32  u32CapFactory;  	//电池初始总容量(出厂容量)As*10 =        Ah*3600*10
	UINT32  u32CycleT_Limit;    //可循环次数
	//以下置零
	UINT32	u32CapChange;		//电池容量变化	   As*10，叠加类型
	UINT8   u8OCV_Cali_Flag;    //开路电压法可使用标志
	UINT8   u8CHG_AHCalcu_Flag;	//充电安时积分可使用标志
	UINT8   u8DSG_AHCalcu_Flag;	//放电安时积分可使用标志
	
	//InitSOC_IntEnhance赋值，其后SOC_Update_StartUp再次赋值类型
	UINT8   u8SOC_Now;          //当前电池SOC     0—100 为相对容量百分比
	UINT32  u32CapNow;		 	//电池剩余总容量As*10
	UINT8	u8DSG_SOC_Int;		//循环次数只算放电量，已放电量积累量百分比，90%算一个循环		
	UINT32  u32Cycle_times;     //循环次数*100，本来只打算用用一个变量直接叠加去处理，但是太损耗EEPROM发现不行
	UINT32  u32CapFull;	 		//电池衰减后总容量As*10(SOH)，我的显示SOH要改一改，算错了

	//运行过程长期修改类型
	UINT8   u8SOC_Old;          //初始SOC    0-100 为相对容量百分比
	//UINT8   u8a_BurnIn;         //老化因素α的修正系数，系数乘以100
	//UINT8   u8b_CapC;      		//电池容量修正因子δ，与充放电循环次数相关δ = f(Cycle_times)
	UINT8	u8_DataUpdateOK;	//更新记录
	UINT32  u32CapFull_Cal_As;	//长期运行，更新容量，As*10

	float    delata_cap;
	float acc_cap_delta;
	float    silent_power;
};


struct SOC_ENHANCE_E2PROM_PAR {
	UINT16  u16_SOC_E2P0;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P1;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P2;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P3;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改

	UINT16  u16_SOC_Temp;			//记录哪个SOC是最新的
	UINT16  u16_DsgSOC_Int0;		//记录已放电量积累量百分比
	UINT16  u16_DsgSOC_Int1;		//记录已放电量积累量百分比
	UINT16  u16_DsgSOC_Temp;		//记录哪个电量积累量是最新的

	UINT16  u16_Cycle_Times;		//记录循环次数
	UINT16  Res1;					//上一次做的任务，虽然取消了，但是位置不能变，原版升级问题
	UINT16  Res5;					//上一次的纠正系数
	UINT16 	u16_SeriousFaultFlag;	//严重错误标志位保存

	UINT16  u16CapFull_Cal_Ah;		//Ah*10
	UINT16  Res2;					//Res2
	UINT16  Res3;					//Res3
	UINT16 	Res4;					//Res4
};

enum CHG_CURVE_STATUS {
	CHG_CURVE_STARTUP = 0,
	CHG_CURVE_BEGIN,
	CHG_CURVE_CONSTANT_CUR,
	CHG_CURVE_CONSTANT_VOR,
	CHG_CURVE_TRICKLE_CUR,
	CHG_CURVE_OVER,
	CHG_CURVE_ERROR_DEAL
};

enum SOC_CALI_STATE {
	SOC_CALI_DATA_INIT = 0,
	SOC_CALI_STARTUP,
	SOC_CALI_STATE_TRANSFER,
	SOC_CALI_CONT_CHG,
	SOC_CALI_CONT_DSG,
};

enum CAP_FULL_STATE {
	CAP_FULL_INIT = 0,
	CAP_FULL_STARTUP,
	CAP_FULL_CALCU,
	CAP_FULL_SUCCESS,
	CAP_FULL_FAIL,
};


enum EEPROM_COMMAND {
	EEPROM_DATA_REFRESH = 0,
	EEPROM_DATA_READ
};



extern UINT16 ChgValue;
extern UINT16 DsgValue;

UINT16 InverterChgCurve(void);
UINT16 InverterDsgCurve(void);

void SOC_OCV_Ctrl(UINT8 TimeBase_200ms);
void SOC_IntEnhance_Ctrl(UINT8 TimeBase_200ms);


extern UINT16 ReadEEPROM_Word_NoZone(UINT16 addr);
extern UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data);

#endif	/* SOCENHANCE_H */

