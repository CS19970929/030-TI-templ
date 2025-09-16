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

#define GPIO_M_CTR           GPIOB
#define PIN_M_CTR            GPIO_Pin_15

#define GPIO_LOAD_RM           GPIOB
#define PIN_LOAD_RM            GPIO_Pin_6

#define GPIO_WK_AFE           GPIOF
#define PIN_WK_AFE            GPIO_Pin_7

#define GPIO_INT_WK_MCU           GPIOA
#define PIN_INT_WK_MCU            GPIO_Pin_0

#define GPIO_AD_EN           GPIOA
#define PIN_AD_EN            GPIO_Pin_8

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


#endif
