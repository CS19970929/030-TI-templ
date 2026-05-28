#ifndef LEDBAR_H
#define LEDBAR_H

#include "conf_gpio.h"

typedef enum _LEDBAR_COMMAND {
	LED_BAR_STARTUP = 0,
    LED_BAR_NORMAL,
    LED_BAR_CHG,
    LED_BAR_DSG,
	LED_BAR_FAULT,
} LEDBAR_COMMAND;

extern LEDBAR_COMMAND LedBar_Command;

void LedBar_StartUp(void);
void APP_LedBar(void);
UINT8 LedBar_IsWakePreviewPending(void);
UINT8 LedBar_HandleWakePreviewBeforeBoot(void);
void LedBar_RequestSocTemporary(UINT16 soc, UINT16 duration_100ms);
void LedBar_RequestBootAnimation(UINT16 soc);
void LedBar_RequestShutdownAnimation(void);
void LedBar_SetDischargeDisplay(UINT8 enable);
void LedBar_SetChargeDisplay(UINT8 enable);
void LedBar_SetWaterAlarm(UINT8 enable);
UINT8 LedBar_IsShutdownAnimationActive(void);

#define MCUI_SOC_KEY 		(PORT_IN_GPIOB->bit14)

#define MCUO_SOC_20 		(PORT_OUT_GPIOB->bit8)
#define MCUO_SOC_40 		(PORT_OUT_GPIOB->bit7)
#define MCUO_SOC_60 		(PORT_OUT_GPIOB->bit13)
#define MCUO_SOC_80 		(PORT_OUT_GPIOB->bit12)
#define MCUO_SOC_100 		(PORT_OUT_GPIOB->bit5)

// #define MCUO_SOC_20 		(PORT_OUT_GPIOB->bit5)
// #define MCUO_SOC_40 		(PORT_OUT_GPIOB->bit12)
// #define MCUO_SOC_60 		(PORT_OUT_GPIOB->bit13)
// #define MCUO_SOC_80 		(PORT_OUT_GPIOB->bit7)
// #define MCUO_SOC_100 		(PORT_OUT_GPIOB->bit8)

#define MCUO_SOC_RUN 		(PORT_OUT_GPIOA->bit5)
#define MCUO_SOC_ALARM 		(MCUO_SOC_20)

extern uint8_t sleep_reason;

#endif	/* LEDBAR_H */
