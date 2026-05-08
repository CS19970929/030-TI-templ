#ifndef SOCENHANCE_H
#define SOCENHANCE_H

#include "stm32f0xx.h"

#define SOC_Size_TableCanSet                 ((UINT16)42)
#define SOC_Size_LiFePO                      ((UINT16)42)
#define SOC_Size_TernaryLi                   ((UINT16)42)
#define SOC_Size_LiFePO2                     ((UINT16)42)

enum SOC_TABLE_SELECT {
    SOC_TABLE_TEST = 0,
    SOC_TABLE_LIFEPO,
    SOC_TABLE_TERNARYLI,
    SOC_TABLE_LIFEPO2
};

extern const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO];
extern const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi];
extern const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2];

#define E2P_AdressNum                        ((UINT16)16)

struct SOC_ENHANCE_ELEMENT {
    UINT16 u16_SOC_Ah;               /* Ah * 10 */
    UINT16 u16_SOC_CycleT_Ever;
    UINT16 u16_SOC_CycleT_Limit;
    UINT16 u16_SOC_TableSelect;
    UINT16 u16_SOC_DsgVcell_Limit;
    UINT16 u16_SOC_0_Vol;
    UINT16 u16_SOC_100_Vol;
    UINT16 SOC_E2P_Adress[E2P_AdressNum];
    UINT16 SOC_Table_CanSet[SOC_Size_TableCanSet];
    UINT8  u8_SetSocOnce;
    UINT8  u8_LargeCurFlag_Chg;
    UINT8  u8_LargeCurFlag_Dsg;

    UINT16 u16_VCellMax;
    UINT16 u16_VCellMin;
    UINT16 u16_Ichg;                 /* A * 10 */
    UINT16 u16_Idsg;                 /* A * 10 */
    UINT16 u16_TempMax;              /* (Temp + 40) * 10 */
    UINT16 u16_TempMin;              /* (Temp + 40) * 10 */

    UINT16 u16_SOC_InitOver;
    UINT16 u16_SOC_CailFaultCnt;
    UINT8  u8_SOC;
    UINT8  u8_SOH;
    UINT16 u16_CapacityNow;          /* Ah * 100 */
    UINT16 u16_CapacityFull;         /* Ah * 100 */
    UINT16 u16_CapacityFactory;      /* Ah * 100 */
    UINT16 u16_Cycle_times;
    UINT8  u8_SOC_OCV_Cali;
    UINT8  u8_n_CoulombicEff;
    UINT8  u8_n_InnerCorrect;

    UINT16 u16_RefreshData_Flag;     /* 1: voltage refresh, 2: parameter reset, 3: manual SOC */
};

extern struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

extern UINT16 ChgValue;
extern UINT16 DsgValue;

UINT16 InverterChgCurve(void);
UINT16 InverterDsgCurve(void);

void SOC_IntEnhance_Ctrl(UINT8 TimeBase_200ms);
UINT8 SOC_GetRealSoc(void);
UINT8 SOC_GetDisplaySoc(void);
UINT8 SOC_GetStartupConfidence(void);
UINT8 SOC_GetStartupReason(void);
UINT8 SOC_GetStartupStrategy(void);
UINT8 SOC_GetStartupTargetSoc(void);
UINT16 GetEndValuee(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat);

extern UINT16 ReadEEPROM_Word_NoZone(UINT16 addr);
extern UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data);

#endif
