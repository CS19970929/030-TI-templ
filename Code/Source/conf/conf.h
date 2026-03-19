#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"
#include "conf_gpio.h"

#define EEPROM_VALUE_BEGIN_FLAG				0x6777		//Ĭ��0x1133������Լ����?ˢһ�飬���Լ������ٸĻ�0x1133

// #define  wdog_enable
// #define __FUNC__HEAT__
#define __LOAD_REMOVE_SHORT_FUNC__

// #define __VIRTURE_CURRENT__
// #define __FUNC__LED__
// #define __FUNC_RTC__

//#define TERNARYLI		//��Ԫ﮵�أ���ѡ�?
#define LIFEPO			//������﮵�أ���ѡ�?


//#define _DI_SWITCH_SYS_ONOFF	//DI??????
//#define _DI_SWITCH_DSG_ONOFF	//DI?????????????MOS
#define _DI_SWITCH_longKEY_ONOFF

#ifdef __FUNC__HEAT__
#define CHG_LOWTEMP_PARAM   120
#define HEAT_OPEN_CURR      50
#else
#define CHG_LOWTEMP_PARAM   380
#define HEAT_OPEN_CURR      500
#endif // DEBUG

#define   CURR_80A      0
#define   CURR_100A     1
#define   CURR_150A     2
#define   CURR_200A     3
#define   CURR_250A     4


#define   LEVEL_CURR     CURR_150A

#ifdef __FUNC_RTC__
#define __SLEEP_VNORMAL__             	2200
#define	__SLEEP_TIMENORMAL__	          10080	
#define __SLEEP_VLOW__     		          3000
#define	__SLEEP_TIMEVLOW__		          1440
#else
#define __SLEEP_VNORMAL__             	4200
#define	__SLEEP_TIMENORMAL__	          10080	
// #define	__SLEEP_TIMENORMAL__	          (30 * 24 * 60)	
#define __SLEEP_VLOW__     		          3000
#define	__SLEEP_TIMEVLOW__		          1440

#endif

typedef struct 
{
  uint64_t    sys_tick_10ms;
  uint64_t    sys_tick_1ms;

  uint16_t    cov1_cnt;
  uint16_t    cov2_cnt;
  uint16_t    cov3_cnt;

  uint16_t    Bov1_cnt;
  uint16_t    Bov2_cnt;
  uint16_t    Bov3_cnt;

  uint16_t    cuv1_cnt;
  uint16_t    cuv2_cnt;
  uint16_t    cuv3_cnt;

  uint16_t    Buv1_cnt;
  uint16_t    Buv2_cnt;
  uint16_t    Buv3_cnt;

  uint16_t    occ1_cnt;
  uint16_t    occ2_cnt;
  uint16_t    occ3_cnt;

  uint16_t    odc1_cnt;
  uint16_t    odc2_cnt;
  uint16_t    odc3_cnt;

  uint32_t    test_driver_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;
  uint32_t    App_SH367309_Monitor_cnt;
  uint32_t    App_SleepDeal_cnt;
  uint32_t    App_beep_cnt;

  uint32_t    sci1_irq_cnt;
  uint32_t    sci2_irq_cnt;
  uint32_t    sci3_irq_cnt;

  uint16_t    test_afe_write_cnt;
  uint16_t    test_compare_cnt;
  uint16_t    test_compare_exceptioncnt;

  uint16_t    uart1_ore_err;
  uint16_t    uart2_ore_err;
  uint16_t    uart2_err2;
  uint16_t    uart2_err3;
  uint16_t    uart2_err4;

  uint16_t    test_current_cnt;
  uint16_t    test_sci2_err_cnt;

  uint16_t    cnt_PA0_irq;
  // uint16_t cnt_bms1_keyirq;
  uint16_t    bq33100_read_cnt;
  uint16_t    pec_err_cnt;
  
  uint8_t isdebugenable;
	uint16_t CHG;
	uint16_t DSG;

  uint16_t  cnt_10ms1;
  uint16_t  cnt_10ms2;
  uint16_t  cnt_10ms3;
  uint16_t  cnt_10ms4;
  uint16_t  cnt_10ms5;

  uint16_t cnt_10ms_test_iocontrol;
  uint16_t  cnt_10ms;
  uint16_t   test_sizeof_g_tParam;

  uint32_t sleep_veryvlow_cnt ;
	uint32_t sleep_vlow_cnt;
	uint32_t sleep_vnormal_cnt;
	uint32_t afe_comm_err_sleepcnt;
  uint8_t su8_SleepExtComCnt;

}Time_T;

extern Time_T  sys_time;


#endif
