/*
*********************************************************************************************************
*
*	模块名称 : 时间触发模式
*	文件名称 : Time_Triggered.C
*	版    本 : V1.0
*	说    明 : 时间触发嵌入式模式的实现
*
*	修改记录 :
*		版本号    日期         作者            说明
*       V1.0    2015-10-08   Eric2013    1. ST固件库到V3.6.1版本
*                                        2. BSP驱动包V1.2
*
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "Time_Triggered.h"
#include "stm32f0xx.h"


#define  RETURN_ERROR                 0x00;
#define  RETURN_NORMOL                0x01;
#define  ERROR_SCH_CANOT_DELETE_TASK  0x02;
#define  ERROR_SCH_TOO_MANY_TASKS	  0x03;

tByte Error_code_G;

sTask SCH_task_G[SCH_MAX_TASKS]; /*建立的任务数*/
/*
*********************************************************************************************************
*	函 数 名: SCH_Update(void)
*	功能说明: 调度器的刷新函数，每个时标中断执行一次。在嘀嗒定时器中断里面执行。
*			  当刷新函数确定某个任务要执行的时候，将RunMe加1，要注意的是刷新任务
*			  不执行任何函数，需要运行的任务有调度函数激活。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void SCH_Update(void)
{
	tByte index;
	
	/*注意计数单位是时标，不是毫秒*/
	for(index = 0; index < SCH_MAX_TASKS; index++)
	{	
		/*检测这里是否有任务*/
		if(SCH_task_G[index].pTask)					  
		{
			if(SCH_task_G[index].Delay == 0)
			{	
				/*任务需要运行 将RunMe置1*/
				SCH_task_G[index].RunMe += 1; 
				if(SCH_task_G[index].Period)
				{						  
					/*调度周期性的任务再次执行*/
					SCH_task_G[index].Delay = SCH_task_G[index].Period;									
				}
			}
			else
			{	
				/*还有准备好运行*/
				SCH_task_G[index].Delay -= 1;														
			}

		}		
	}
}

/*
*********************************************************************************************************
*	函 数 名: SCH_Add_Task
*	功能说明: 添加任务。
*	形    参：void (*pFuntion)(void) tWord DELAY tWord PERIOD
*	返 回 值: 返回任务的ID号
*   使用说明：
*	（1）SCH_Add_Task(DOTASK,1000,0) DOTASK是函数的运行地址，1000是1000个时标以后开始运行，只运行一次；
*	（2）Task_ID = SCH_Add_Task(DOTASK,1000,0); 将任务标示符保存 以便以后删除任务 
*	（3）SCH_Add_Task(DOTASK,0,1000); 每个1000个时标周期性的运行一次；
*********************************************************************************************************
*/
tByte SCH_Add_Task(void (*pFuntion)(void),
				              tWord DELAY,
				             tWord PERIOD)
{
	tByte index = 0; /*首先在队列中找到一个空隙，（如果有的话）*/
	
	while((SCH_task_G[index].pTask != 0) && (index <SCH_MAX_TASKS))
	{
		index ++;		
	}
	if(index == SCH_MAX_TASKS)/*超过最大的任务数目 则返错误信息*/
	{
		Error_code_G = ERROR_SCH_TOO_MANY_TASKS;/*设置全局错误变量*/
		return SCH_MAX_TASKS;	
	}
	
	SCH_task_G[index].pTask = pFuntion;	/*运行到这里说明申请的任务块成功*/
	SCH_task_G[index].Delay = DELAY;
	SCH_task_G[index].Period = PERIOD;
	SCH_task_G[index].RunMe =0;
	
	return index;					   /*返回任务的位置，以便于以后删除*/
}

/*
*********************************************************************************************************
*	函 数 名: SCH_Task_Delete
*	功能说明: 删除任务。
*	形    参：tByte index
*	返 回 值: 是否删除成功
*********************************************************************************************************
*/
tByte SCH_Task_Delete(tByte index)
{
	tByte Return_code;
	
	/*这里没有任务*/
	if(SCH_task_G[index].pTask == 0)				   
	{
		/*设置全局错误变量*/
		Error_code_G = ERROR_SCH_CANOT_DELETE_TASK;
		Return_code  = RETURN_ERROR;
	}
	else
	{	
		Return_code  = RETURN_NORMOL;		
	}

	SCH_task_G[index].pTask = 0x0000;
	SCH_task_G[index].Delay = 0;
	SCH_task_G[index].Period = 0;
	SCH_task_G[index].RunMe =0;
	
	return Return_code;		/*返回状态*/
}

/*
*********************************************************************************************************
*	函 数 名: SCH_Dispatch_Tasks
*	功能说明: 在主任务里面执行的调度函数。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void SCH_Dispatch_Tasks(void)
{
	tByte index;
	/*运行下一个任务，如果下一个任务准备就绪的话*/
	for(index = 0; index < SCH_MAX_TASKS; index++)
	{
		if(SCH_task_G[index].RunMe >0)
		{
			/*执行任务 */
			(*SCH_task_G[index].pTask)();    

			/* 执行任务完成后，将RunMe减一 */
			SCH_task_G[index].RunMe -= 1;

			/*如果是单次任务的话，则将任务删除 */	  
			if(SCH_task_G[index].Period == 0) 
			{		
				SCH_Task_Delete(index);
			}
		}
	}			
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
