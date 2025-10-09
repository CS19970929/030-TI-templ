/*
*********************************************************************************************************
*
*	ģ������ : ʱ�䴥��ģʽ
*	�ļ����� : Time_Triggered.h
*	��    �� : V1.0
*	˵    �� : ʱ�䴥��Ƕ��ʽģʽͷ�ļ�
*
*	�޸ļ�¼ :
*		�汾��    ����         ����            ˵��
*       V1.0    2015-10-08   Eric2013    1. ST�̼��⵽V3.6.1�汾
*                                        2. BSP������V1.2
*
*	Copyright (C), 2015-2020, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __TIME_TRIGGERED_H
#define __TIME_TRIGGERED_H


#define	 SCH_MAX_TASKS     10


typedef unsigned char    tByte;
typedef unsigned int     tWord; 

typedef  struct
{
	void (*pTask)();	   		 /*ָ�������ָ�������һ��*void(void)*������*/
	
	tWord Delay;  		   		 /*��ʱ��ʱ�֪꣩����һ������������*/
	
	tWord Period;		   		 /*��������֮��ļ��*/
	
	tByte RunMe;		   		 /*��������Ҫ���е�ʱ���ɵ�������1*/
	
}sTask;

void   SCH_Update(void);
tByte  SCH_Add_Task(void (*pFuntion)(void),
				               tWord DELAY,
				              tWord PERIOD);
tByte SCH_Task_Delete(tByte TASK_INDEX);
void  SCH_Dispatch_Tasks(void); 

extern sTask SCH_task_G[SCH_MAX_TASKS];  /*������������*/ 

#endif


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
