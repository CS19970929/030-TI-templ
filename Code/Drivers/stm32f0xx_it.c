/**
  ******************************************************************************
  * @file    IO_Toggle/stm32f0xx_it.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    23-March-2012
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_it.h"
#include "main.h"
/** @addtogroup STM32F0_Discovery_Peripheral_Examples
  * @{
  */

/** @addtogroup IO_Toggle
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M0 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F0xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f0xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/



//外部中断0服务程序，没用
void EXTI0_1_IRQHandler(void) {
	//delay_ms(10);//消抖
    if(EXTI_GetITStatus(EXTI_Line0) != RESET) {
		//WKUP
		EXTI_ClearITPendingBit(EXTI_Line0);
		ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 1;
	}
	if(EXTI_GetITStatus(EXTI_Line1) != RESET) {
		EXTI_ClearITPendingBit(EXTI_Line1);
	}
}


void EXTI2_3_IRQHandler(void) {
	if(EXTI_GetITStatus(EXTI_Line3) != RESET) {
    EXTI_ClearITPendingBit(EXTI_Line3);
	}
}

void EXTI4_15_IRQHandler(void) {
    if(EXTI_GetITStatus(EXTI_Line12) != RESET) {
		Delay_Base10us(OtherElement.u16CBC_DelayT/100);
		if(MCUI_CBC_DSG == 0) {
			MCUO_MOS_DSG = CLOSE;
			MCUO_RELAY_DSG = CLOSE;
			MCUO_RELAY_PRE = CLOSE;                 
			MCUO_RELAY_MAIN = CLOSE;
			CBC_Element.u8CBC_DSG_ErrFlag = 1;
			//App_MOS_Relay_Ctrl();
			System_ERROR_UserCallback(ERROR_CBC_DSG);
		}
		EXTI_ClearITPendingBit(EXTI_Line12);
	}

	if(EXTI_GetITStatus(EXTI_Line13) != RESET) {
		EXTI_ClearITPendingBit(EXTI_Line13);
	}

	if(EXTI_GetITStatus(EXTI_Line10) != RESET) {
		EXTI_ClearITPendingBit(EXTI_Line10);
	}
  
}


void USART1_IRQHandler(void) {
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
		RTC_ExtComCnt++;
		#if (defined _COMMOM_UPPER_SCI1)
		Sci1_CommonUpper_FaultChk();
		Sci1_CommonUpper_Rx_Deal(&g_stCurrentMsgPtr_SCI1);
		#endif
	}
}


void USART2_IRQHandler(void) {
	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
		RTC_ExtComCnt++;
		#ifdef _COMMOM_UPPER_SCI2
		Sci2_CommonUpper_FaultChk();
		Sci2_CommonUpper_Rx_Deal(&g_stCurrentMsgPtr_SCI2);
		#endif
	}
}

