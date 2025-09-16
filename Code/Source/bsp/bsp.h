/*
*********************************************************************************************************
*
*	ģ������ : BSPģ��
*	�ļ����� : bsp.h
*	˵    �� : ���ǵײ�����ģ�����е�h�ļ��Ļ����ļ��� Ӧ�ó���ֻ�� #include bsp.h ���ɣ�
*			  ����Ҫ#include ÿ��ģ��� h �ļ�
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_H_
#define _BSP_H

#define STM32_V4
//#define STM32_X2

/* ����Ƿ����˿������ͺ� */
#if !defined (STM32_V4) && !defined (STM32_X2)
	#error "Please define the board model : STM32_X2 or STM32_V4"
#endif

/* ���� BSP �汾�� */
#define __STM32F1_BSP_VERSION		"1.1"

/* CPU����ʱִ�еĺ��� */
//#define CPU_IDLE()		bsp_Idle()

/* ����ȫ���жϵĺ� */
#define ENABLE_INT()	__set_PRIMASK(0)	/* ʹ��ȫ���ж� */
#define DISABLE_INT()	__set_PRIMASK(1)	/* ��ֹȫ���ж� */

#define BSP_SET_GPIO_1(gpio, pin)   gpio->BSRR = pin
#define BSP_SET_GPIO_0(gpio, pin)   gpio->BSRR = (uint32_t)(pin) << 16U

#define DEBUG_LINE() 																												\
  BSP_Printf("Log: [%s:%s] line = %d\n", __FILE__, __func__, __LINE__)
#define DEBUG_INFO(fmt, ...)                                                \
  BSP_Printf("Log: [%s:%s] line = %d\n" fmt "\n", __FILE__, __func__, __LINE__, \
         ##__VA_ARGS__)


/* ���������ڵ��Խ׶��Ŵ� */
#define BSP_Printf		printf
// #define BSP_Printf(...)

#include "stm32f0xx.h"
//#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//todo �ظ���
// #ifndef TRUE
// 	#define TRUE  1
// #endif

// #ifndef FALSE
// 	#define FALSE 0
// #endif

//#include "bsp_led.h"
// #include "bsp_timer.h"
// #include "bsp_key.h"
// #include "bsp_beep.h"
// #include "bsp_i2c_gpio.h"
// #include "bsp_i2c_gpio1.h"
// #include "bsp_i2c_eeprom_24xx.h"

#include "bsp_cpu_flash.h"
#include "param.h"

/* �ṩ������C�ļ����õĺ��� */
void bsp_Init(void);
void bsp_Idle(void);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
