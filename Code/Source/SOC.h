#ifndef SOC_H
#define SOC_H

#include "SocEnhance.h"

#define SOC_TABLE_SIZE                         42
#define SOC_VOL_MIN                           ((UINT16)0)
#define SOC_VOL_MAX                           ((UINT16)5000)
#define SOC_VALUE_MIN                         ((UINT16)0)
#define SOC_VALUE_MAX                         ((UINT16)100)

#define SOC_UPDATE_PERIOD_MS                  ((UINT16)1000)
#define SOC_SAVE_MIN_INTERVAL_MS              ((UINT32)60000)
#define SOC_SAVE_FORCE_INTERVAL_MS            ((UINT32)300000)
#define SOC_SAVE_CHANGE_THRESHOLD_01PCT       ((UINT16)50)
#define SOC_CURRENT_IDLE_THRESHOLD_MA         ((INT32)200)

#define SOC_CALIBRATION_VOLTAGE_HIGH_MV       ((UINT16)4150)
#define SOC_CALIBRATION_VOLTAGE_LOW_MV        ((UINT16)3200)

typedef enum {
    CURRENT_CHANNEL_CC1 = 0,
    CURRENT_CHANNEL_CC2
} Current_Channel_t;

extern UINT16 SOC_Table_Set[SOC_TABLE_SIZE];
extern const UINT16 SOC_Table_Default[SOC_TABLE_SIZE];

UINT8 SOC_Manager_Init(void);
UINT8 SOC_Manager_Config(UINT16 capacity_ah_x10, UINT16 sense_resistor_mohm, UINT16 parallel_count);
UINT8 SOC_Manager_Load_From_EEPROM(void);
UINT8 SOC_Manager_Save_To_EEPROM(void);

void SOC_Manager_Update(void);
void SOC_Manager_Update_CC1(void);
void SOC_Manager_Update_CC2(void);

UINT8 SOC_Calibrate_By_Voltage(void);
UINT8 SOC_Calibrate_By_Full_Charge(Current_Channel_t channel);
UINT8 SOC_Calibrate_By_Empty_Discharge(Current_Channel_t channel);
void SOC_Auto_Calibration_Check(void);

UINT16 SOC_Get_Average_Cell_Voltage(void);
INT32 SOC_Get_Pack_Current_mA(void);
UINT16 SOC_Calculate_Parallel_Resistance(UINT16 single_resistor_mohm, UINT16 parallel_count);

void InitData_SOC(void);
void App_SOC(void);

#endif
