#include "main.h"

#define SOC_PERSIST_MAGIC                       ((UINT16)0x534F)
#define SOC_PERSIST_VERSION                     ((UINT16)0x1117)
#define SOC_PERSIST_SLOT_WORDS                  ((UINT8)8)
#define SOC_PERSIST_SLOT_COUNT                  ((UINT8)2)

#define SOC_INIT_CONFIDENCE_LOW                 ((UINT8)1)
#define SOC_INIT_CONFIDENCE_MEDIUM              ((UINT8)2)
#define SOC_INIT_CONFIDENCE_HIGH                ((UINT8)3)

#define SOC_RESTORE_REASON_FALLBACK             ((UINT8)1)
#define SOC_RESTORE_REASON_OCV                  ((UINT8)2)
#define SOC_RESTORE_REASON_STORE                ((UINT8)3)
#define SOC_RESTORE_REASON_PARAM                ((UINT8)4)
#define SOC_RESTORE_REASON_MANUAL               ((UINT8)5)

#define SOC_INIT_STRATEGY_NONE                  ((UINT8)0)
#define SOC_INIT_STRATEGY_OCV_DIRECT            ((UINT8)1)

#define SOC_FULL_CHARGE_DETECT_MS               ((UINT32)300000)
#define SOC_AUTO_CALIBRATION_PERIOD_MS          ((UINT32)3600000)
#define SOC_DEFAULT_CAPACITY_AH_X10             ((UINT16)500)
#define SOC_DEFAULT_CYCLE_LIMIT                 ((UINT16)5000)

typedef struct {
    UINT16 magic;
    UINT16 version;
    UINT16 sequence;
    UINT16 soc_01pct;
    UINT16 capacity_ah100;
    UINT16 cycles;
    UINT16 soh_01pct;
    UINT16 checksum;
} SOC_PERSIST_SNAPSHOT;

typedef struct {
    UINT16 soc_01pct;
    UINT16 soh_01pct;
    UINT16 capacity_now_ah100;
    UINT16 capacity_full_ah100;
    UINT16 capacity_factory_ah100;
    UINT16 charge_cycles;
    INT32 pack_current_ma;
    int64_t accumulated_charge_mas;
    int64_t integration_remainder_mas;
    UINT32 system_tick_ms;
    UINT32 last_update_ms;
    UINT32 last_save_ms;
    UINT32 full_charge_start_ms;
    UINT32 last_auto_calibration_ms;
    UINT16 last_saved_soc_01pct;
    UINT16 last_saved_cycles;
    UINT16 persist_sequence;
    UINT8 initialized;
    UINT8 eeprom_loaded;
    UINT8 voltage_ready_at_init;
    UINT8 full_charge_detecting;
    UINT8 startup_confidence;
    UINT8 startup_reason;
    UINT8 startup_strategy;
    UINT8 startup_target_soc;
    UINT8 invalid_voltage_count;
} SOC_MANAGER_STATE;

UINT16 SOC_Table_Set[SOC_TABLE_SIZE];

const UINT16 SOC_Table_Default[SOC_TABLE_SIZE] = {
    4200, 100,
    4000, 100,
    3650, 99,
    3600, 98,
    3500, 96,
    3450, 93,
    3400, 90,
    3360, 85,
    3340, 80,
    3320, 70,
    3300, 60,
    3280, 50,
    3260, 40,
    3240, 30,
    3220, 20,
    3200, 10,
    3150, 4,
    3100, 2,
    3000, 0,
    2800, 0,
    2500, 0,
};

static SOC_MANAGER_STATE s_soc;

static UINT32 SOC_AbsDiffU16(UINT16 a, UINT16 b)
{
    return (a >= b) ? (UINT32)(a - b) : (UINT32)(b - a);
}

static INT32 SOC_AbsI32(INT32 value)
{
    return (value < 0) ? -value : value;
}

static UINT16 SOC_Limit01Pct(INT32 soc)
{
    if (soc < 0)
    {
        return 0;
    }
    if (soc > 1000)
    {
        return 1000;
    }
    return (UINT16)soc;
}

static UINT16 SOC_LimitPercent(UINT16 soc)
{
    return (soc > 100) ? 100 : soc;
}

static UINT16 SOC_GetCapacityAhX10(void)
{
    if (OtherElement.u16Soc_Ah == 0 || OtherElement.u16Soc_Ah == 0xFFFF)
    {
        return SOC_DEFAULT_CAPACITY_AH_X10;
    }
    return OtherElement.u16Soc_Ah;
}

static UINT16 SOC_GetCapacityAh100(void)
{
    return (UINT16)((UINT32)SOC_GetCapacityAhX10() * 10u);
}

static UINT32 SOC_GetCapacityMah(void)
{
    return (UINT32)SOC_GetCapacityAhX10() * 100u;
}

static UINT16 SOC_GetCellCount(void)
{
    if (SeriesNum == 0 || SeriesNum > 32)
    {
        return 16;
    }
    return SeriesNum;
}

static void SOC_LoadDefaultTableIfNeeded(void)
{
    UINT16 i;

    if (SOC_Table_Set[0] != 0 && SOC_Table_Set[1] != 0)
    {
        return;
    }

    for (i = 0; i < SOC_TABLE_SIZE; ++i)
    {
        SOC_Table_Set[i] = SOC_Table_Default[i];
    }
}

static const UINT16 *SOC_GetConfiguredOcvTable(void)
{
    switch (OtherElement.u16Soc_TableSelect)
    {
    case SOC_TABLE_TEST:
        return SOC_Table_Set;
    case SOC_TABLE_TERNARYLI:
        return SocTable_TernaryLi;
    case SOC_TABLE_LIFEPO2:
        return SocTable_LiFePO2;
    case SOC_TABLE_LIFEPO:
    default:
        return SOC_Table_LiFePO;
    }
}

static UINT16 SOC_Estimate01PctByVoltage(UINT16 avg_cell_mv)
{
    UINT16 soc_percent;

    soc_percent = GetEndValuee(SOC_GetConfiguredOcvTable(), SOC_TABLE_SIZE, avg_cell_mv);
    return (UINT16)(SOC_LimitPercent(soc_percent) * 10u);
}

static UINT8 SOC_HasValidVoltage(void)
{
    UINT16 avg;

    avg = SOC_Get_Average_Cell_Voltage();
    return (UINT8)(avg >= 2000 && avg <= 5000);
}

static void SOC_RefreshInputs(void)
{
    SOC_Enhance_Element.u16_VCellMax = g_stCellInfoReport.u16VCellMax;
    SOC_Enhance_Element.u16_VCellMin = g_stCellInfoReport.u16VCellMin;
    SOC_Enhance_Element.u16_Ichg = g_stCellInfoReport.u16Ichg;
    SOC_Enhance_Element.u16_Idsg = g_stCellInfoReport.u16IDischg;
    SOC_Enhance_Element.u16_TempMax = g_stCellInfoReport.u16TempMax;
    SOC_Enhance_Element.u16_TempMin = g_stCellInfoReport.u16TempMin;
    s_soc.pack_current_ma = SOC_Get_Pack_Current_mA();
}

static UINT16 SOC_PersistAddr(UINT8 slot, UINT8 index)
{
    return (UINT16)(E2P_ADDR_E2POS_ENHANCE_SOC +
                    ((UINT16)slot * SOC_PERSIST_SLOT_WORDS + index) * 2u);
}

static UINT16 SOC_PersistChecksum(const SOC_PERSIST_SNAPSHOT *snapshot)
{
    UINT32 sum;

    sum = snapshot->magic;
    sum += snapshot->version;
    sum += snapshot->sequence;
    sum += snapshot->soc_01pct;
    sum += snapshot->capacity_ah100;
    sum += snapshot->cycles;
    sum += snapshot->soh_01pct;
    return (UINT16)(0xA5A5u ^ (UINT16)sum ^ (UINT16)(sum >> 16));
}

static UINT8 SOC_LoadPersistSlot(UINT8 slot, SOC_PERSIST_SNAPSHOT *snapshot)
{
    UINT16 *data;
    UINT8 i;

    data = &snapshot->magic;
    for (i = 0; i < SOC_PERSIST_SLOT_WORDS; ++i)
    {
        data[i] = ReadEEPROM_Word_NoZone(SOC_PersistAddr(slot, i));
    }

    if (snapshot->magic != SOC_PERSIST_MAGIC)
    {
        return 0;
    }
    if (snapshot->version != SOC_PERSIST_VERSION)
    {
        return 0;
    }
    if (snapshot->soc_01pct > 1000 || snapshot->soh_01pct > 1000)
    {
        return 0;
    }
    if (snapshot->checksum != SOC_PersistChecksum(snapshot))
    {
        return 0;
    }

    return 1;
}

static void SOC_ApplySnapshot(const SOC_PERSIST_SNAPSHOT *snapshot)
{
    s_soc.soc_01pct = snapshot->soc_01pct;
    s_soc.soh_01pct = snapshot->soh_01pct;
    s_soc.capacity_full_ah100 = snapshot->capacity_ah100;
    s_soc.capacity_factory_ah100 = SOC_GetCapacityAh100();
    s_soc.charge_cycles = snapshot->cycles;
    s_soc.persist_sequence = snapshot->sequence;
    s_soc.capacity_now_ah100 = (UINT16)(((UINT32)s_soc.capacity_full_ah100 * s_soc.soc_01pct) / 1000u);
}

static void SOC_UpdateCapacityOutputs(void)
{
    s_soc.capacity_factory_ah100 = SOC_GetCapacityAh100();
    if (s_soc.capacity_full_ah100 == 0 || s_soc.capacity_full_ah100 > s_soc.capacity_factory_ah100)
    {
        s_soc.capacity_full_ah100 = s_soc.capacity_factory_ah100;
    }
    s_soc.capacity_now_ah100 = (UINT16)(((UINT32)s_soc.capacity_full_ah100 * s_soc.soc_01pct) / 1000u);
}

static void SOC_SetSoc01Pct(UINT16 soc_01pct, UINT8 reason, UINT8 confidence)
{
    s_soc.soc_01pct = SOC_Limit01Pct(soc_01pct);
    s_soc.integration_remainder_mas = 0;
    s_soc.accumulated_charge_mas = 0;
    s_soc.startup_reason = reason;
    s_soc.startup_confidence = confidence;
    SOC_UpdateCapacityOutputs();
}

static void SOC_Initialize_Default_Values(void)
{
    UINT16 avg_cell_mv;

    avg_cell_mv = SOC_Get_Average_Cell_Voltage();
    if (avg_cell_mv < 2000 || avg_cell_mv > 5000)
    {
        avg_cell_mv = g_stCellInfoReport.u16VCellMax;
    }
    if (avg_cell_mv < 2000 || avg_cell_mv > 5000)
    {
        avg_cell_mv = 3300;
    }

    s_soc.capacity_factory_ah100 = SOC_GetCapacityAh100();
    s_soc.capacity_full_ah100 = s_soc.capacity_factory_ah100;
    s_soc.soh_01pct = 1000;
    s_soc.charge_cycles = OtherElement.u16Soc_Cycle_times;
    s_soc.startup_strategy = SOC_INIT_STRATEGY_OCV_DIRECT;
    s_soc.startup_target_soc = (UINT8)(SOC_Estimate01PctByVoltage(avg_cell_mv) / 10u);
    SOC_SetSoc01Pct((UINT16)s_soc.startup_target_soc * 10u,
                    SOC_RESTORE_REASON_OCV,
                    SOC_HasValidVoltage() ? SOC_INIT_CONFIDENCE_HIGH : SOC_INIT_CONFIDENCE_LOW);
}

static UINT8 SOC_IsSnapshotMismatchedWithVoltage(void)
{
    if (!SOC_HasValidVoltage())
    {
        return 0;
    }
    if (s_soc.soc_01pct == 0 && g_stCellInfoReport.u16VCellMax > 3000)
    {
        return 1;
    }
    if ((s_soc.soc_01pct > 800 && g_stCellInfoReport.u16VCellMax < 3300) ||
        (s_soc.soc_01pct < 200 && g_stCellInfoReport.u16VCellMax > 4000))
    {
        return 1;
    }
    return 0;
}

static void SOC_HandleRefreshCommand(void)
{
    UINT16 flag;

    flag = SOC_Enhance_Element.u16_RefreshData_Flag;
    if (flag == 0)
    {
        return;
    }

    switch (flag)
    {
    case 1:
        (void)SOC_Calibrate_By_Voltage();
        break;
    case 2:
        s_soc.capacity_factory_ah100 = SOC_GetCapacityAh100();
        s_soc.capacity_full_ah100 = s_soc.capacity_factory_ah100;
        s_soc.charge_cycles = OtherElement.u16Soc_Cycle_times;
        (void)SOC_Calibrate_By_Voltage();
        s_soc.startup_reason = SOC_RESTORE_REASON_PARAM;
        s_soc.startup_confidence = SOC_INIT_CONFIDENCE_MEDIUM;
        break;
    case 3:
        SOC_SetSoc01Pct((UINT16)SOC_LimitPercent(SOC_Enhance_Element.u8_SetSocOnce) * 10u,
                        SOC_RESTORE_REASON_MANUAL,
                        SOC_INIT_CONFIDENCE_HIGH);
        break;
    default:
        break;
    }

    (void)SOC_Manager_Save_To_EEPROM();
    SOC_Enhance_Element.u16_RefreshData_Flag = 0;
}

static void SOC_PublishOutputs(void)
{
    UINT16 report_soc;

    SOC_UpdateCapacityOutputs();

    report_soc = (UINT16)((s_soc.soc_01pct + 5u) / 10u);
    if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
    {
        report_soc = 60;
    }
    if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
    {
        report_soc = 0;
    }

    SOC_Enhance_Element.u8_SOC = (UINT8)report_soc;
    SOC_Enhance_Element.u8_SOH = (UINT8)((s_soc.soh_01pct + 5u) / 10u);
    SOC_Enhance_Element.u16_CapacityNow = s_soc.capacity_now_ah100;
    SOC_Enhance_Element.u16_CapacityFull = s_soc.capacity_full_ah100;
    SOC_Enhance_Element.u16_CapacityFactory = s_soc.capacity_factory_ah100;
    SOC_Enhance_Element.u16_Cycle_times = s_soc.charge_cycles;
    SOC_Enhance_Element.u16_SOC_InitOver = s_soc.initialized;
    SOC_Enhance_Element.u16_SOC_CailFaultCnt = s_soc.invalid_voltage_count;
    SOC_Enhance_Element.u8_SOC_OCV_Cali = s_soc.startup_confidence;

    g_stCellInfoReport.SocElement.u16Soc = report_soc;
    g_stCellInfoReport.SocElement.u16Soh = SOC_Enhance_Element.u8_SOH;
    g_stCellInfoReport.SocElement.u16CapacityNow = s_soc.capacity_now_ah100;
    g_stCellInfoReport.SocElement.u16CapacityFull = s_soc.capacity_full_ah100;
    g_stCellInfoReport.SocElement.u16CapacityFactory = s_soc.capacity_factory_ah100;
    g_stCellInfoReport.SocElement.u16Cycle_times = s_soc.charge_cycles;

    System_ErrFlag.u8ErrFlag_SOC_Cail = s_soc.invalid_voltage_count;
    if (s_soc.initialized)
    {
        System_Func_StartUp.bits.b1StartUpFlag_SOC = 0;
    }
}

UINT8 SOC_Manager_Init(void)
{
    UINT16 i;

    memset(&s_soc, 0, sizeof(s_soc));
    SOC_LoadDefaultTableIfNeeded();

    SOC_Enhance_Element.u16_SOC_Ah = SOC_GetCapacityAhX10();
    SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
    SOC_Enhance_Element.u16_SOC_CycleT_Limit = SOC_DEFAULT_CYCLE_LIMIT;
    SOC_Enhance_Element.u16_SOC_TableSelect = OtherElement.u16Soc_TableSelect;
    SOC_Enhance_Element.u16_SOC_100_Vol = OtherElement.u16Soc_V_100;
    SOC_Enhance_Element.u16_SOC_0_Vol = OtherElement.u16Soc_V_0;
    SOC_Enhance_Element.u8_LargeCurFlag_Chg = 0;
    SOC_Enhance_Element.u8_LargeCurFlag_Dsg = 0;

    for (i = 0; i < E2P_AdressNum; ++i)
    {
        SOC_Enhance_Element.SOC_E2P_Adress[i] = (UINT16)(E2P_ADDR_E2POS_ENHANCE_SOC + 2u * i);
    }
    for (i = 0; i < SOC_Size_TableCanSet; ++i)
    {
        SOC_Enhance_Element.SOC_Table_CanSet[i] = SOC_Table_Set[i];
    }

    SOC_RefreshInputs();
    s_soc.voltage_ready_at_init = SOC_HasValidVoltage();
    SOC_Initialize_Default_Values();
    s_soc.eeprom_loaded = SOC_Manager_Load_From_EEPROM();

    if (s_soc.eeprom_loaded)
    {
        s_soc.startup_reason = SOC_RESTORE_REASON_STORE;
        s_soc.startup_confidence = SOC_INIT_CONFIDENCE_MEDIUM;
        s_soc.startup_strategy = SOC_INIT_STRATEGY_NONE;
        if (SOC_IsSnapshotMismatchedWithVoltage())
        {
            SOC_Initialize_Default_Values();
            s_soc.startup_reason = SOC_RESTORE_REASON_OCV;
            s_soc.startup_confidence = SOC_INIT_CONFIDENCE_HIGH;
        }
    }

    s_soc.initialized = 1;
    SOC_PublishOutputs();
    return 1;
}

UINT8 SOC_Manager_Config(UINT16 capacity_ah_x10, UINT16 sense_resistor_mohm, UINT16 parallel_count)
{
    (void)sense_resistor_mohm;
    (void)parallel_count;

    if (capacity_ah_x10 == 0)
    {
        return 0;
    }

    OtherElement.u16Soc_Ah = capacity_ah_x10;
    s_soc.capacity_factory_ah100 = SOC_GetCapacityAh100();
    s_soc.capacity_full_ah100 = s_soc.capacity_factory_ah100;
    SOC_UpdateCapacityOutputs();
    SOC_PublishOutputs();
    return 1;
}

UINT8 SOC_Manager_Load_From_EEPROM(void)
{
    SOC_PERSIST_SNAPSHOT slot0;
    SOC_PERSIST_SNAPSHOT slot1;
    UINT8 valid0;
    UINT8 valid1;

    valid0 = SOC_LoadPersistSlot(0, &slot0);
    valid1 = SOC_LoadPersistSlot(1, &slot1);

    if (!valid0 && !valid1)
    {
        return 0;
    }

    if (valid0 && (!valid1 || (UINT16)(slot0.sequence - slot1.sequence) < 0x8000u))
    {
        SOC_ApplySnapshot(&slot0);
    }
    else
    {
        SOC_ApplySnapshot(&slot1);
    }

    SOC_UpdateCapacityOutputs();
    s_soc.last_saved_soc_01pct = s_soc.soc_01pct;
    s_soc.last_saved_cycles = s_soc.charge_cycles;
    return 1;
}

UINT8 SOC_Manager_Save_To_EEPROM(void)
{
    SOC_PERSIST_SNAPSHOT snapshot;
    UINT16 *data;
    UINT8 slot;
    UINT8 i;

    snapshot.magic = SOC_PERSIST_MAGIC;
    snapshot.version = SOC_PERSIST_VERSION;
    snapshot.sequence = (UINT16)(s_soc.persist_sequence + 1u);
    snapshot.soc_01pct = s_soc.soc_01pct;
    snapshot.capacity_ah100 = s_soc.capacity_full_ah100;
    snapshot.cycles = s_soc.charge_cycles;
    snapshot.soh_01pct = s_soc.soh_01pct;
    snapshot.checksum = SOC_PersistChecksum(&snapshot);

    slot = (UINT8)(snapshot.sequence & 0x0001u);
    data = &snapshot.magic;
    for (i = 0; i < SOC_PERSIST_SLOT_WORDS; ++i)
    {
        if (WriteEEPROM_Word_NoZone(SOC_PersistAddr(slot, i), data[i]) != 0)
        {
            return 0;
        }
    }

    s_soc.persist_sequence = snapshot.sequence;
    s_soc.last_saved_soc_01pct = s_soc.soc_01pct;
    s_soc.last_saved_cycles = s_soc.charge_cycles;
    s_soc.last_save_ms = s_soc.system_tick_ms;
    return 1;
}

void SOC_Manager_Update(void)
{
    UINT8 should_save;

    if (!s_soc.initialized)
    {
        (void)SOC_Manager_Init();
    }

    SOC_RefreshInputs();
    s_soc.system_tick_ms += 200u;

    if (!s_soc.voltage_ready_at_init && SOC_HasValidVoltage() && !s_soc.eeprom_loaded)
    {
        SOC_Initialize_Default_Values();
        s_soc.voltage_ready_at_init = 1;
    }

    if ((UINT32)(s_soc.system_tick_ms - s_soc.last_update_ms) < SOC_UPDATE_PERIOD_MS)
    {
        SOC_PublishOutputs();
        return;
    }

    SOC_Manager_Update_CC2();
    SOC_Auto_Calibration_Check();
    SOC_HandleRefreshCommand();

    should_save = 0;
    if ((UINT32)(s_soc.system_tick_ms - s_soc.last_save_ms) >= SOC_SAVE_MIN_INTERVAL_MS)
    {
        if (SOC_AbsDiffU16(s_soc.soc_01pct, s_soc.last_saved_soc_01pct) >= SOC_SAVE_CHANGE_THRESHOLD_01PCT)
        {
            should_save = 1;
        }
        if (s_soc.charge_cycles != s_soc.last_saved_cycles)
        {
            should_save = 1;
        }
    }
    if ((UINT32)(s_soc.system_tick_ms - s_soc.last_save_ms) >= SOC_SAVE_FORCE_INTERVAL_MS)
    {
        should_save = 1;
    }
    if (should_save)
    {
        (void)SOC_Manager_Save_To_EEPROM();
    }

    s_soc.last_update_ms = s_soc.system_tick_ms;
    SOC_PublishOutputs();
}

void SOC_Manager_Update_CC1(void)
{
    SOC_Manager_Update_CC2();
}

void SOC_Manager_Update_CC2(void)
{
    INT32 current_ma;
    INT32 soc_delta;
    INT32 next_soc;
    int64_t charge_delta_mas;
    int64_t step_mas;
    UINT32 capacity_mah;

    current_ma = s_soc.pack_current_ma;
    if (SOC_AbsI32(current_ma) < SOC_CURRENT_IDLE_THRESHOLD_MA)
    {
        return;
    }

    capacity_mah = SOC_GetCapacityMah();
    if (capacity_mah == 0)
    {
        return;
    }

    charge_delta_mas = (int64_t)current_ma * (int64_t)SOC_UPDATE_PERIOD_MS;
    s_soc.accumulated_charge_mas += (INT32)charge_delta_mas;
    s_soc.integration_remainder_mas += (INT32)charge_delta_mas;

    step_mas = ((int64_t)capacity_mah * 3600LL) / 1000LL;
    if (step_mas <= 0)
    {
        return;
    }

    soc_delta = 0;
    while (s_soc.integration_remainder_mas >= step_mas)
    {
        s_soc.integration_remainder_mas -= step_mas;
        soc_delta++;
    }
    while (s_soc.integration_remainder_mas <= -step_mas)
    {
        s_soc.integration_remainder_mas += step_mas;
        soc_delta--;
    }

    if (soc_delta != 0)
    {
        next_soc = (INT32)s_soc.soc_01pct + soc_delta;
        s_soc.soc_01pct = SOC_Limit01Pct(next_soc);
        SOC_UpdateCapacityOutputs();
    }
}

UINT8 SOC_Calibrate_By_Voltage(void)
{
    UINT16 avg_cell_voltage;

    avg_cell_voltage = SOC_Get_Average_Cell_Voltage();
    if (avg_cell_voltage < 2000 || avg_cell_voltage > 5000)
    {
        s_soc.invalid_voltage_count++;
        return 0;
    }

    SOC_SetSoc01Pct(SOC_Estimate01PctByVoltage(avg_cell_voltage),
                    SOC_RESTORE_REASON_OCV,
                    SOC_INIT_CONFIDENCE_HIGH);
    s_soc.startup_target_soc = (UINT8)(s_soc.soc_01pct / 10u);
    return 1;
}

UINT8 SOC_Calibrate_By_Full_Charge(Current_Channel_t channel)
{
    UINT16 avg_cell_voltage;
    UINT16 full_voltage;
    UINT8 condition;

    (void)channel;

    avg_cell_voltage = SOC_Get_Average_Cell_Voltage();
    if (avg_cell_voltage < 2000 || avg_cell_voltage > 5000)
    {
        s_soc.full_charge_detecting = 0;
        return 0;
    }

    full_voltage = OtherElement.u16Soc_V_100;
    if (full_voltage == 0 || full_voltage == 0xFFFF)
    {
        full_voltage = 3500;
    }

    condition = (UINT8)((SOC_AbsI32(s_soc.pack_current_ma) < 100) &&
                        (avg_cell_voltage >= 3400) &&
                        (g_stCellInfoReport.u16VCellMax >= (UINT16)(full_voltage - 50u)));

    if (condition)
    {
        if (!s_soc.full_charge_detecting)
        {
            s_soc.full_charge_detecting = 1;
            s_soc.full_charge_start_ms = s_soc.system_tick_ms;
        }
        else if ((UINT32)(s_soc.system_tick_ms - s_soc.full_charge_start_ms) >= SOC_FULL_CHARGE_DETECT_MS)
        {
            SOC_SetSoc01Pct(1000, SOC_RESTORE_REASON_OCV, SOC_INIT_CONFIDENCE_HIGH);
            s_soc.charge_cycles++;
            s_soc.full_charge_detecting = 0;
            (void)SOC_Manager_Save_To_EEPROM();
            return 1;
        }
    }
    else
    {
        s_soc.full_charge_detecting = 0;
    }

    return 0;
}

UINT8 SOC_Calibrate_By_Empty_Discharge(Current_Channel_t channel)
{
    UINT16 avg_cell_voltage;
    UINT16 empty_voltage;

    (void)channel;

    avg_cell_voltage = SOC_Get_Average_Cell_Voltage();
    if (avg_cell_voltage < 2000 || avg_cell_voltage > 5000)
    {
        return 0;
    }

    empty_voltage = OtherElement.u16Soc_V_0;
    if (empty_voltage == 0 || empty_voltage == 0xFFFF)
    {
        empty_voltage = 3000;
    }

    if (avg_cell_voltage < 3000 || g_stCellInfoReport.u16VCellMin <= empty_voltage)
    {
        SOC_SetSoc01Pct(0, SOC_RESTORE_REASON_OCV, SOC_INIT_CONFIDENCE_HIGH);
        return 1;
    }

    return 0;
}

void SOC_Auto_Calibration_Check(void)
{
    if ((UINT32)(s_soc.system_tick_ms - s_soc.last_auto_calibration_ms) >= SOC_AUTO_CALIBRATION_PERIOD_MS)
    {
        (void)SOC_Calibrate_By_Voltage();
        s_soc.last_auto_calibration_ms = s_soc.system_tick_ms;
    }

    (void)SOC_Calibrate_By_Full_Charge(CURRENT_CHANNEL_CC2);
    (void)SOC_Calibrate_By_Empty_Discharge(CURRENT_CHANNEL_CC2);
}

UINT16 SOC_Get_Average_Cell_Voltage(void)
{
    UINT16 i;
    UINT16 cell_count;
    UINT16 valid_count;
    UINT32 total_voltage;
    UINT16 vcell;

    cell_count = SOC_GetCellCount();
    valid_count = 0;
    total_voltage = 0;

    for (i = 0; i < cell_count; ++i)
    {
        vcell = g_stCellInfoReport.u16VCell[i];
        if (vcell > 2000 && vcell < 5000)
        {
            total_voltage += vcell;
            valid_count++;
        }
    }

    if (valid_count == 0)
    {
        return 0;
    }

    return (UINT16)(total_voltage / valid_count);
}

INT32 SOC_Get_Pack_Current_mA(void)
{
    if (g_stCellInfoReport.u16Ichg >= g_stCellInfoReport.u16IDischg)
    {
        return (INT32)g_stCellInfoReport.u16Ichg * 100;
    }
    return -((INT32)g_stCellInfoReport.u16IDischg * 100);
}

UINT16 SOC_Calculate_Parallel_Resistance(UINT16 single_resistor_mohm, UINT16 parallel_count)
{
    if (parallel_count == 0)
    {
        return 0;
    }
    return (UINT16)(single_resistor_mohm / parallel_count);
}

UINT8 SOC_GetRealSoc(void)
{
    return (UINT8)((s_soc.soc_01pct + 5u) / 10u);
}

UINT8 SOC_GetDisplaySoc(void)
{
    return SOC_GetRealSoc();
}

UINT8 SOC_GetStartupConfidence(void)
{
    return s_soc.startup_confidence;
}

UINT8 SOC_GetStartupReason(void)
{
    return s_soc.startup_reason;
}

UINT8 SOC_GetStartupStrategy(void)
{
    return s_soc.startup_strategy;
}

UINT8 SOC_GetStartupTargetSoc(void)
{
    return s_soc.startup_target_soc;
}

void InitData_SOC(void)
{
    (void)SOC_Manager_Init();
}

void App_SOC(void)
{
    SOC_Manager_Update();
}
