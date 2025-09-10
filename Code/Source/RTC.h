#ifndef RTC_H
#define RTC_H

#define BKP_VALUE   0x32F2
#define LSE_START_TIMEOUT   	((uint16_t)0x5000) /*!< Time out for LSE start up */
#define TIME_INIT_TIMEOUT 		((uint16_t)0x5000) 


struct RTC_ELEMENT {
	UINT16  RTC_Time_Year;
	UINT16  RTC_Time_Month;
	UINT16  RTC_Time_Day;
	UINT16  RTC_Time_Hour;
	
	UINT16  RTC_Time_Minute;
	UINT16  RTC_Time_Second;
	UINT16  RTC_Alarm_Year;
	UINT16  RTC_Alarm_Month;
	
	UINT16  RTC_Alarm_Day;
	UINT16  RTC_Alarm_Hour;
	UINT16  RTC_Alarm_Minute;
	UINT16  RTC_Alarm_Second;
};


#define RTC_element_min     {0, 1, 1, 0,\
								0, 0, 0, 0,\
								0, 0, 0, 0}

#define RTC_element_default {18, 10, 31, 23,\
								59, 30, 0, 0,\
								0, 9, 1, 4}

#define RTC_element_max     {99, 12, 31, 23,\
								59, 59, 99, 12,\
								31, 23, 59, 59}

extern RTC_InitTypeDef RTC_InitStructure;
extern RTC_TimeTypeDef RTC_TimeStructure;
extern RTC_DateTypeDef RTC_DateStructure;
extern RTC_AlarmTypeDef RTC_AlarmStructure;
extern struct RTC_ELEMENT RTC_time;


UINT8 RTC_TimeConfig(void);
void RTC_AlarmConfig(void);

void App_RTC(void);
void Init_RTC(void);

#endif	/* RTC_H */


