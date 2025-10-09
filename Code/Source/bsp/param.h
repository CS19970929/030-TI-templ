/*
*********************************************************************************************************
*
*	ģ������ : Ӧ�ó������ģ��
*	�ļ����� : param.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2012-2013, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __PARAM_H
#define __PARAM_H

#include "Flash.h"
#include "main.h"

/* ����2�к�ֻ��ѡ����һ */
// #define PARAM_SAVE_TO_EEPROM			/* �����洢���ⲿ��EEPROM (AT24C128) */
#define PARAM_SAVE_TO_FLASH		/* �����洢��CPU�ڲ�Flash */

#ifdef PARAM_SAVE_TO_EEPROM
	#define PARAM_ADDR		0			/* ��������ַ */
#endif

#ifdef PARAM_SAVE_TO_FLASH
	#define PARAM_ADDR		FLASH_ADDR_TEST_BMS_PARAM			/* 0x0800C000 �м��16KB����������Ų��� */
	//#define PARAM_ADDR	 ADDR_FLASH_SECTOR_11		/* 0x080E0000 Flash���128K����������Ų��� */
#endif

#define PARAM_VER			0x1234					/* �����汾 */

#if 0
/* ȫ�ֲ��� */
typedef struct
{
	uint32_t ParamVer;			/* �������汾���ƣ������ڳ�������ʱ�������Ƿ�Բ��������������� */

	/* LCD�������� */
	uint8_t ucBackLight;

	/* ������У׼���� */
	//{
		uint8_t TouchDirection;	/* ��Ļ���� 0-3  0��ʾ������1��ʾ����180�� 2��ʾ���� 3��ʾ����180�� */
		
		uint8_t XYChange;		/* X, Y �Ƿ񽻻��� 1��ʾi�л���0��ʾ���л�  */
		
		uint16_t usAdcX1;	/* ���Ͻ� */
		uint16_t usAdcY1;
		uint16_t usAdcX2;	/* ���½� */
		uint16_t usAdcY2;
		uint16_t usAdcX3;	/* ���½� */
		uint16_t usAdcY3;
		uint16_t usAdcX4;	/* ���Ͻ� */
		uint16_t usAdcY4;
		
		uint16_t usLcdX1;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdY1;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdX2;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdY2;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdX3;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdY3;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdX4;	/* У׼ʱ����Ļ���� */
		uint16_t usLcdY4;	/* У׼ʱ����Ļ���� */	
	//}
	
	/* uip ip ��ַ���� */
	uint8_t uip_ip[4];			/* ����IP��ַ */
	uint8_t uip_net_mask[4];	/* �������� */
	uint8_t uip_gateway[4];	/* Ĭ������ */

	/* lwip ip ��ַ���� */
	uint8_t lwip_ip[4];			/* ����IP��ַ */
	uint8_t lwip_net_mask[4];	/* �������� */
	uint8_t lwip_gateway[4];	/* Ĭ������ */

	/* ���������� */
	uint8_t ucRadioMode;		/* AM �� FM */
	uint8_t ucRadioListType;		/* ��̨�б����͡��人������ȫ�� */
	uint8_t ucIndexFM;			/* ��ǰFM��̨���� */
	uint8_t ucIndexAM;			/* ��ǰ��̨���� */
	uint8_t ucRadioVolume;		/* ���� */
	uint8_t ucSpkOutEn;			/* ���������ʹ�� */
	
	uint8_t Addr485;
	uint32_t Baud485;
}
PARAM_T;
#endif

typedef struct
{
	uint16_t ParamVer;			/* �������汾���ƣ������ڳ�������ʱ�������Ƿ�Բ��������������� */

	struct PRT_E2ROM_PARAS    protect;
	struct OTHER_ELEMENT 	  other;
	struct HEAT_COOL_ELEMENT  heat;

	// /* uip ip ��ַ���� */
	// uint8_t uip_ip[4];			/* ����IP��ַ */
	// uint8_t uip_net_mask[4];	/* �������� */
	// uint8_t uip_gateway[4];	/* Ĭ������ */

	// /* lwip ip ��ַ���� */
	// uint8_t lwip_ip[4];			/* ����IP��ַ */
	// uint8_t lwip_net_mask[4];	/* �������� */
	// uint8_t lwip_gateway[4];	/* Ĭ������ */

	// /* ���������� */
	// uint8_t ucRadioMode;		/* AM �� FM */
	// uint8_t ucRadioListType;		/* ��̨�б����͡��人������ȫ�� */
	// uint8_t ucIndexFM;			/* ��ǰFM��̨���� */
	// uint8_t ucIndexAM;			/* ��ǰ��̨���� */
	// uint8_t ucRadioVolume;		/* ���� */
	// uint8_t ucSpkOutEn;			/* ���������ʹ�� */
	
	// uint8_t Addr485;
	// uint32_t Baud485;
}
PARAM_T;

extern PARAM_T g_tParam;

void LoadParam(void);
void SaveParam(void);

#endif
