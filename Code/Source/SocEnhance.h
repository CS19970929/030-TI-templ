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

void SOC_IntEnhance_Ctrl(void);


extern UINT16 ReadEEPROM_Word_NoZone(UINT16 addr);
extern UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data);

void soc_factory_param_init_first(void);
void soc_param_lib_init(void);
#endif	/* SOCENHANCE_H */

