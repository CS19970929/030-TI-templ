#ifndef __RTC_SLEEP__
#define __RTC_SLEEP__

#include "conf.h"

typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, 
HICCUP_MODE,
DEEP_MODE,
NO_SLEEP
}SLEEP_MODE;


#define enumToStr(WEEEK)  #WEEEK

extern bool is_wakeup;

void rtc_sleep(void);

uint8_t get_rtc_soc(void);
void set_rtc_soc(uint8_t _soc);

// void set_irq_wksource(enum irqWakeup irq);
void set_irq_wksource(uint8_t irq);


bool isVol_low_sleep(void);
void entersleep(enum _SLEEP_MODE mode);

void sleep(void);



#endif
