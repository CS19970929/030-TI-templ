#include "lk8625.h"
#include "conf.h"
#include "conf_gpio.h"
#include "stm32f0xx.h"

void lk8625_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    //拉低2-5ms 退出深度休眠
    GPIO_ResetBits(GPIO_WK_H, PIN_WK_H);
    GPIO_InitStructure.GPIO_Pin = PIN_WK_H;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
    GPIO_Init(GPIO_WK_H, &GPIO_InitStructure);
    __delay_ms(10);
    GPIO_SetBits(GPIO_WK_H, PIN_WK_H);

    LK8625_ENTER_SLEEP();
    // GPIO_ResetBits(GPIO_SLP_BLE, PIN_SLP_BLE);
    GPIO_InitStructure.GPIO_Pin = PIN_SLP_BLE;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
    GPIO_Init(GPIO_SLP_BLE, &GPIO_InitStructure);
}

uint8_t at_buff[30];

#if 0
#define debug_uart USART1
#else
#define uart_8625 USART2
#endif
void lk8625_SendAT(char *_Cmd)
{
		uint8_t i;
    // comSendBuf(COM_ESP8266, (uint8_t *)_Cmd, strlen(_Cmd));
    LK8625_EXIT_SLEEP();
    __delay_ms(300);
 
    for (i = 0; i < strlen(_Cmd); i++)
    {
        at_buff[i] = _Cmd[i];
    }

    for (i = 0; i < strlen(_Cmd); i++)
    {
        while (!((uart_8625->ISR) & (1 << 7)))
            ; // 1<<6 也可以
        uart_8625->TDR = at_buff[i];
    }
    __delay_ms(100);

    LK8625_ENTER_SLEEP();
}