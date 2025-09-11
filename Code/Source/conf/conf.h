#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"

#define __VIRTURE_CURRENT__

typedef struct 
{
  uint64_t    sys_tick_10ms;
  uint64_t    sys_tick_1ms;

  // uint16_t    cov1_cnt;
  // uint16_t    cov2_cnt;
  // uint16_t    cov3_cnt;

  // uint16_t    Bov1_cnt;
  // uint16_t    Bov2_cnt;
  // uint16_t    Bov3_cnt;

  // uint16_t    cuv1_cnt;
  // uint16_t    cuv2_cnt;
  // uint16_t    cuv3_cnt;

  // uint16_t    Buv1_cnt;
  // uint16_t    Buv2_cnt;
  // uint16_t    Buv3_cnt;

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

}Time_T;

extern Time_T  sys_time;


#endif