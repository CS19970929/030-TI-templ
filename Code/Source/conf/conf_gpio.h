#ifndef CONF_GPIO_H
#define CONF_GPIO_H

#define __STM32F0__
// #define __STM32F1__

#ifdef __STM32F0__
#include "stm32f0xx.h"
#endif // __STM32F0__
#ifdef __STM32F1__
#include "stm32f10x.h"
#endif // __STM32F1__


// #define GPIO_ADC
// #define PIN_ADC
#define GPIO_WK_H           GPIOB
#define PIN_WK_H            GPIO_Pin_14



#define GPIO_WK_AFE           GPIOF
#define PIN_WK_AFE            GPIO_Pin_7


#define GPIO_AD_NTC           GPIOB
#define PIN_AD_NTC            GPIO_Pin_0

#define GPIO_SLP_BLE           GPIOB
#define PIN_SLP_BLE            GPIO_Pin_1

#define GPIO_DB_LED1           GPIOB
#define PIN_DB_LED1            GPIO_Pin_2

#define GPIO_KEY1           GPIOC
#define PIN_KEY1            GPIO_Pin_13

#define GPIO_HT_CHG           GPIOA
#define PIN_HT_CHG            GPIO_Pin_12


/************************************************/
#define GPIO_GAN1           GPIOA
#define PIN_GAN1            GPIO_Pin_4
#define GPIO_GAN2           GPIOA
#define PIN_GAN2            GPIO_Pin_5
#define GPIO_GAN4           GPIOA
#define PIN_GAN4            GPIO_Pin_6

#define GPIO_SOCL1           GPIOB
#define PIN_SOCL1            GPIO_Pin_5
#define GPIO_SOCL2           GPIOB
#define PIN_SOCL2            GPIO_Pin_12
#define GPIO_SOCL3           GPIOB
#define PIN_SOCL3            GPIO_Pin_13

#define GPIO_INT_WK_CMNT           GPIOB
#define PIN_INT_WK_CMNT            GPIO_Pin_14

#define GPIO_BLE_EN           GPIOB
#define PIN_BLE_EN            GPIO_Pin_15

#define GPIO_LOAD_RM           GPIOB
#define PIN_LOAD_RM            GPIO_Pin_6

#define GPIO_SOCL4           GPIOB
#define PIN_SOCL4            GPIO_Pin_7
#define GPIO_SOCL5           GPIOB
#define PIN_SOCL5            GPIO_Pin_8

#define GPIO_SWT_AD           GPIOB
#define PIN_SWT_AD            GPIO_Pin_9
#define GPIO_SWT_EN           GPIOF
#define PIN_SWT_EN            GPIO_Pin_6

#define GPIO_INT_WK_MCU           GPIOA
#define PIN_INT_WK_MCU            GPIO_Pin_0

#define GPIO_M_CTR           GPIOA
#define PIN_M_CTR            GPIO_Pin_8

#define GPIO_M_STB           GPIOB
#define PIN_M_STB            GPIO_Pin_1

#define GPIO_GAN3           GPIOC
#define PIN_GAN3            GPIO_Pin_13


#endif
