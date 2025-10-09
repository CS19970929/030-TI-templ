#ifndef IODRIVERS_030_H
#define IODRIVERS_030_H

#include "stm32f0xx.h"

typedef enum _DRIVERS_STATUS {
	CLOSE_MODE = 0,
	OPEN_MODE = 1
}DriversStatus;

typedef enum _FORCE_STATUS {
	FORCE_KEEP_MODE = 0,
	FORCE_OPEN_MODE,
	FORCE_CLOSE_MODE,
}FORCE_STATUS;

typedef enum GPIO_TYPE {
	GPIO_PreCHG = 0,
	GPIO_CHG,
	GPIO_DSG,
	GPIO_MAIN,
}GPIO_Type;


typedef union DRIVER_ONOFF_ETC {
    UINT16 all;
    struct OnOFF_Result {
		UINT8 b1Status_ToSleep				:1; 	//
		DriversStatus b1Status_MOS_PRE      :1;		//Ԥ��MOS�ܹ���״̬
		DriversStatus b1Status_MOS_CHG      :1;		//���MOS�ܹ���״̬
		DriversStatus b1Status_MOS_DSG      :1;		//�ŵ�MOS�ܹ���״̬
		
		DriversStatus b1Status_Relay_PRE    :1;		//Ԥ��̵�������״̬
		DriversStatus b1Status_Relay_CHG    :1;		//�ֿڳ��̵�������״̬
		DriversStatus b1Status_Relay_DSG    :1;		//�ֿڷŵ�̵�������״̬
		DriversStatus b1Status_Relay_MAIN   :1;		//ͬ�����̵�������״̬
		
		UINT8 b1_FuncOFF_OV					:1;		//������ѹ��������IO���ܹرգ������ڽӴ�����������
		UINT8 b1_FuncOFF_UV					:1;		//������ѹ��������IO���ܹرգ������ڽӴ��������������Ծٵ�·Ҳ��
		UINT8 b1_FuncOFF_Ocp_Ichg			:1;		//������������������IO���ܹر�
		UINT8 b1_FuncOFF_Ocp_Idsg			:1;		//�����ŵ������������IO���ܹر�
		
		UINT8 b1_FuncOFF_Ocp_Imain			:1;		//������������(ֻ��һ�����Ӵ����汾ʹ��)����IO���ܹر�
													//��ȡ�����ϸ��ճ�ŵ������������
		UINT8 b1_FuncOFF_Vdelta				:1;		//ѹ����󱣻�����IO���ܹر�
		UINT8 Res   						:2;		//
    }bits;
}Driver_OnOFF_Etc;

typedef union _DRIVER_FORCE_EXTERNAL {
    UINT16 all;
    struct Force_Result {
		FORCE_STATUS b2_DriverOFF_Flag		:2; 	//�ⲿǿ�ƹر�������־λ
		FORCE_STATUS b2_Force_MOS_PRE		:2;		//�ⲿǿ�ƿ���Ԥ��MOS�ܣ���ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		FORCE_STATUS b2_Force_MOS_CHG      	:2;		//�ⲿǿ�ƿ��Ƴ��MOS�ܣ���ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		FORCE_STATUS b2_Force_MOS_DSG      	:2;		//�ⲿǿ�ƿ��Ʒŵ�MOS�ܣ���ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		
		FORCE_STATUS b2_Force_Relay_PRE    	:2;		//�ⲿǿ�ƿ���Ԥ��̵�������ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		FORCE_STATUS b2_Force_Relay_CHG    	:2;		//�ⲿǿ�ƿ��Ʒֿڳ��̵�������ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		FORCE_STATUS b2_Force_Relay_DSG    	:2;		//�ⲿǿ�ƿ��Ʒֿڷŵ�̵�������ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
		FORCE_STATUS b2_Force_Relay_MAIN   	:2;		//�ⲿǿ�ƿ���ͬ�����̵�������ǿ��OPEN����ǿ��CLOSE��Ĭ�Ͻ������ΪFORCE_KEEP_MODE
    }bits;
}DRIVER_FORCE_EXT;

typedef union FAULT_FLAG {
    UINT16 all;
    struct Fault_Flag_UNION {
		UINT8 b1CellOvp 		:1; 	//
		UINT8 b1CellUvp 		:1; 	//
		UINT8 b1BatOvp			:1; 	//
		UINT8 b1BatUvp			:1; 	//
		
		UINT8 b1IchgOcp 		:1; 	//
		UINT8 b1IdischgOcp		:1; 	//
		UINT8 b1CellChgOtp		:1; 	//
		UINT8 b1CellDischgOtp	:1; 	//
		
		UINT8 b1CellChgUtp		:1; 	//
		UINT8 b1CellDischgUtp	:1; 	//
		UINT8 b1VcellDeltaBig	:1; 	//
		UINT8 b1TempDeltaBig	:1; 	//���û�У�Res����
		
		UINT8 b1SocLow			:1; 	//
		UINT8 b1TmosOtp 		:1; 	//
		UINT8 b1PackOvp 		:1; 	//��������Ҫ
		UINT8 b1PackUvp 		:1; 	//
     }bits;
}Fault_Flag;


typedef struct DRIVER_ELEMENT {
	//ֻ��Ҫ��ֵһ�εĲ���
	UINT16 u16_VirCur_Chg;				//A*10
	UINT16 u16_VirCur_Dsg;				//A*10
	UINT16 u16_PreChg_Time;				//���������ٸ�����
	UINT16 u16_PreChg_Duty;				//ռ�ձȣ�10,20,30����100��
	UINT16 u16_PreChg_Period;			//���ڣ�100msΪ��λ��1Ϊ100ms��10Ϊ1s
	UINT8 u8_DriverCtrl_Right;			//��������Ȩ�����Ϊ0������lib�ļ�����IO�Ŀ��ض��������Ϊ1����lib�ļ�ֻ�ṩIO���ص���Ϣ���ⲿ���п���
										//���ã�Ĭ�ϲ���0�ͺá��о����ԡ�����ⲿ��ʱ������̫���������ʱ������(Ԥ�Ź������µȵ�)

	UINT16 u16_10msForceOpenT_Ovp;		//����Щ��ѹ֮��������Ҫǿ�ƴ�ʱ�����ã�ԭ��30sǿ�ƴ򿪣��������Ӵ������������ڿ��裬Ĭ�ϻ���д30s
	UINT16 u16_10msForceOpenT_Uvp;		//����Щ��ѹ֮��������Ҫǿ�ƴ�ʱ�����ã�ԭ��30sǿ�ƴ򿪣��������Ӵ������������ڿ��裬Ĭ�ϻ���д30s
										//Ovp�����Ӵ��������У�Uvp�����Ӵ����������Ծٵ�·��
	//UINT16 u16_10msForceOpenT_Ocp;	//�����Ȳ�����ȥ��ǿ��30s�������������������

	//��Ҫ����ϸ�ֵ�Ĳ���
	Fault_Flag Fault_Flag;
	UINT16 u16_CurChg;					//A*10
	UINT16 u16_CurDsg;					//A*10


	//�ܻ�ȡ�����Ϣ�Ĳ���
	Driver_OnOFF_Etc MosRelay_Status;	//���Ǿ���ʡ�ռ䣬Ŀ����8L��103������


	//��Ϣ������
	UINT8 u8_FuncOFF_Flag; 				//Ҫ��رչ��ܱ�־λ��1����ع��ܱ�־λ��Ҫ�ر�; 0�����رա�
										//�����־λ��if(!=)��ֵ�ķ�ʽ��
	//UINT8 u8_DriverOFF_Flag;			//�ⲿǿ�ƹر�������־λ��ת�Ƶ����·�λ
	DRIVER_FORCE_EXT DriverForceExt;	//�ⲿǿ�ƿ���������־λ��ȫ�ֿ��ƺ;ֲ��������ƶ���
}DriverElement;


extern DriverElement Driver_Element;


//GPIO��Ϊ���������2MHz��GPIO_Type��4����ʽ
void InitDrivers_GPIO(GPIO_TypeDef* GPIOx, UINT16 GPIO_Pin_x, GPIO_Type GpioType);

void Drivers_RelaySameDoor_NoPreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);
void Drivers_RelaySameDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);
void Drivers_RelayDiffDoor_NoPreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);
void Drivers_RelayDiffDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);
void Drivers_MosSameDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);
void Drivers_MosBootStrap_Cir(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl);

void Drivers_MosSameDoor_NoPreChg(UINT8 OnOFF_Ctrl);

#endif	/* IODRIVERS_030_H */

