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

#define M_STB_PORT          GPIOB
#define M_STB_PIN           GPIO_Pin_1

#define M_CMNT_EN_PORT          GPIOB
#define M_CMNT_EN_PIN           GPIO_Pin_1

#define M_CTR_PORT          GPIOA
#define M_CTR_PIN           GPIO_Pin_8
	
#define M_BLE_EN_PORT          GPIOB
#define M_BLE_EN_PIN           GPIO_Pin_15


#endif
