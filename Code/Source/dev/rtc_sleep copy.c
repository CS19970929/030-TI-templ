#include "main.h"

#define LOG_TAG "rtc_sleep"

void rtc_sleep(void);
void test_dealError(void);
static void clear_ocv_state_rtcing(void);

static bool rtc_monitor(void);
static bool isException(void);
bool updataAFEData_once(void);
static void report_wkup_sig(void);
static bool isErr_enterRTC(void);

static void before_wakeup(uint32_t *_sleep_cnt);
static void before_rtcsleep(void);
static bool rtc_monitor_sh367309(void);
static bool updataData_rtc_sh3x(void);
static bool updataData_rtc_bq7x(void);
static bool isHaveCurrent_bq7x(void);
static bool rtc_monitor_bq7x(void);
static bool update_rtc_soc(uint32_t *_sleep_cnt);
static void doWork_rtcing(uint32_t *_sleep_cnt);

uint16_t cnt_uart3_irq = 0;
uint16_t cnt_bms1_keyirq = 0;
uint16_t cnt_bms2_keyirq = 0;
uint16_t cnt_bms3_keyirq = 0;
uint16_t cnt_PA0_irq = 0;
uint16_t cnt_uart1_irq = 0;
uint16_t cnt_485_can_irq = 0;

typedef struct
{
    uint32_t rtc_sleepTime;
    bool rtc_ocv_success;
} infoRTC_T;

infoRTC_T g_rtcInfo;

typedef struct
{
    uint8_t soc_disp;

    uint8_t soc_real;
    uint8_t soc_min;

    uint8_t soc_max;

    uint8_t soc_mean;
} SOC_T;

static SOC_T soc_befor_sleep;
static SOC_T soc_update_wakeup;

static enum _SLEEP_MODE g_sleepModeSelect = NO_SLEEP;
bool is_wakeup = false;

void print_vcell(void)
{
    uint8_t i;
    for (i = 0; i < g_tParam.other.u16Sys_SeriesNum; i++)
    {
        log_i("cell%d  %d\n", i, g_stCellInfoReport.u16VCell[i]);
    }
    log_i("vcelltotle %d", g_stCellInfoReport.u16VCellTotle);
}

static bool isVol_cuv(void)
{
    uint8_t i;

    for (i = 0; i < g_tParam.other.u16Sys_SeriesNum; i++)
    {
        log_i("cell%d  %d\n", i, g_stCellInfoReport.u16VCell[i]);

        if (g_stCellInfoReport.u16VCell[i] < g_tParam.protect.u16VcellUvp_Third)
        {
            log_i("i = %d, vol= %d\n", i, g_stCellInfoReport.u16VCell[i]);
            break;
        }
    }
    if (i == g_tParam.other.u16Sys_SeriesNum)
    {
        return false;
    }

    set_irq_wksource(cuv_wake);

    log_w("cuv fault\n");

    return true;
}

static bool isVol_cov(void)
{
    uint8_t i;
    uint8_t j;

    for (i = 0; i < g_tParam.other.u16Sys_SeriesNum; i++)
    {
        // log_d("cell%d  %d\n", i, g_stCellInfoReport.u16VCell[i]);
        if (g_stCellInfoReport.u16VCell[i] > g_tParam.protect.u16VcellOvp_Third)
        {
            log_i("i = %d, vol= %d\n", i, g_stCellInfoReport.u16VCell[i]);
            break;
        }
    }
    // for (j = 0; j < g_tParam.other.u16Sys_SeriesNum; j++)
    // {
    //     log_w("cell%d  %d\n", i, g_stCellInfoReport2.u16VCell[j]);

    //     if (g_stCellInfoReport2.u16VCell[j] > g_tParam.protect.u16VcellOvp_Third)
    //     {
    //         log_w("j = %d, vol= %d\n", j, g_stCellInfoReport2.u16VCell[j]);
    //         break;
    //     }
    // }
    // log_w("i = %d, snum= %d\n", i, g_tParam.other.u16Sys_SeriesNum);
    // if (i == g_tParam.other.u16Sys_SeriesNum && j == g_tParam.other.u16Sys_SeriesNum)
    if (i == g_tParam.other.u16Sys_SeriesNum)
    {
        return false;
    }

    set_irq_wksource(cov_wake);
    log_w("cov fault\n");

    return true;
}

// void cpu_frequency_conf(uint8_t Mhz)
void cpu_frequency_conf(void)
{
    SystemInit();

    RCC_HSEConfig(RCC_HSE_ON);
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
    {
    }
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
    {
    }
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08)
    {
    }
}

bool isHaveCurrent(void)
{
    bool isCURR = false;

#if AFE_TYPE == bq76xx_afe
    isCURR = isHaveCurrent_bq7x();
#elif AFE_TYPE == sh36xx
    isCURR = isHaveCurrent_sh3x();
#else
#error "error!!!"
#endif

    return isCURR;
}

bool isHaveCurrent_sh3x(void)
{
#if (AFE_TYPE == sh36xx)

    bool isCURR = false;
    uint16_t current = SH367309_Read_AFE1.u16Current;

    log_i("ichg %d\n", g_stCellInfoReport.u16Ichg);
    log_i("dsg %d\n", g_stCellInfoReport.u16IDischg);
#if 0
	if(ModulusSub(current, su16_OffsetValue) < 1)
#endif

    if (g_stCellInfoReport.u16Ichg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    }
    if (g_stCellInfoReport.u16IDischg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);
    }

    return isCURR;
#endif
}

static bool isHaveCurrent_bq7x(void)
{
#if AFE_TYPE == bq76xx_afe
    bool isCURR = false;

    uint16_t current = g_stBq769x0_Read_AFE1.u16Current;

#if defined(_DEBUG_)
    if (g_stCellInfoReport.u16Ichg)
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    if (g_stCellInfoReport.u16IDischg)
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);

    if (g_stCellInfoReport.u16Ichg >= g_tParam.other.u16Sleep_VirCur_Chg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
    }
    if (g_stCellInfoReport.u16IDischg >= g_tParam.other.u16Sleep_VirCur_Dsg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
    }
#else
    if (g_stCellInfoReport.u16Ichg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    }
    if (g_stCellInfoReport.u16IDischg)
    {
        isCURR = true;
        set_irq_wksource(current_wake);
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);
    }
#endif

    return isCURR;
#endif
}

void entersleep(enum _SLEEP_MODE mode)
{
    switch (mode)
    {
    case HICCUP_MODE:
        Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
        g_sleepModeSelect = HICCUP_MODE;
        break;
    case NORMAL_MODE:

        break;
    case DEEP_MODE:
        Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
        g_sleepModeSelect = DEEP_MODE;
#ifdef __FUNC__LED__
        // set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG
        break;
    case NO_SLEEP:
        g_sleepModeSelect = NO_SLEEP;
        Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
        Sleep_Mode.all = 0;
        break;
    default:
        break;
    }
}

#if (AFE_TYPE == bq76xx_afe)

static UINT8 AFE_SleepMode_Judge(void)
{
    UINT8 result = 0;

    if (g_stCellInfoReport.u16VCellMin <= g_tParam.other.u16Sleep_Vlow && !g_stCellInfoReport.u16Ichg)
    {
        result = 1;
        log_e("cuv sleep");
    }
    else if (System_ERROR_UserCallback(ERROR_STATUS_AFE1))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_AFE1));
    }
    else if (System_ERROR_UserCallback(ERROR_STATUS_AFE2))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_AFE2));
    }
    else if ((System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG)))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_CBC_DSG));
    }
    // else if (g_stCellInfoReport.unMdlFault_Third.all)
    // {
    //     result = 2;
    //     log_e("in faulting");
    // }
    else
    {
        result = 0;
    }

    return result;
}

#elif (AFE_TYPE == sh36xx)

static UINT8 AFE_SleepMode_Judge(void)
{
    UINT8 result = 0;

    if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
    {
        if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all || SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
        {
            log_e("error can not enter rtc");
            result = 1;
        }
        else
        {
            result = 0;
        }
    }
    else
    {
        log_a("err mtp comm");
        result = 2;
    }

    return result;
}

#endif

void BQ769x0_SleepMode_Ctrl(void)
{
    static UINT8 su8_StartUp_Flag = 0;
    static UINT8 su8_SleepExtComCnt = 0;
    static UINT16 su16_RTC2_100msTCnt = 0;
    static uint32_t deepsleep_cnt = 0;

    UINT8 u8_CurComDelay_Flag = 0;

    // todo ç»Ÿä¸€rtc_sleep()å’ŒApp_SleepDeal()è¿‡æ”¾ä¼‘çœ 
    if (AFE_SleepMode_Judge() == 1)
    {
        su16_RTC2_100msTCnt = 0;
        // print_vcell();
        if (++deepsleep_cnt >= (uint32_t)g_tParam.other.u16Sleep_TimeVlow * 60)
        {
            entersleep(DEEP_MODE);
        }
        log_w("%d s enter deep sleep", (60 * g_tParam.other.u16Sleep_TimeVlow - deepsleep_cnt));
        return;
    }
    // else if (AFE_SleepMode_Judge() == 2)
    // {
    //     if(g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
    //     {
    //         log_a("err");
    //     }
    //     return;
    // }
    else
    {
        deepsleep_cnt = 0;
    }

    switch (su8_StartUp_Flag)
    {
    case 0:
        su8_StartUp_Flag = 1;
        break;
    case 1:
        if (isErr_enterRTC())
        {
            u8_CurComDelay_Flag = 1;
        }
        else if (su8_SleepExtComCnt != RTC_ExtComCnt)
        {
            su8_SleepExtComCnt = RTC_ExtComCnt;
            u8_CurComDelay_Flag = 1;
        }

        if (u8_CurComDelay_Flag)
        {
            su16_RTC2_100msTCnt = 0;
        }
        else
        {
            if (AFE_SleepMode_Judge() == 0)
            {
                if (++su16_RTC2_100msTCnt >= g_tParam.other.time_enter_rtc)
                // if (++su16_RTC2_100msTCnt >= ENTER_RTC_TIME)
                {
                    su16_RTC2_100msTCnt = 0;

                    entersleep(HICCUP_MODE);
                }
                // log_w("%d s enter rtc mode1", (ENTER_RTC_TIME - su16_RTC2_100msTCnt));
                log_w("%d s enter rtc mode1", (g_tParam.other.time_enter_rtc - su16_RTC2_100msTCnt));
            }
            else
            {
                log_a("err");
            }
        }
        break;
    default:
        break;
    }
}

void sleep(void)
{
    // if (System_OnOFF_Func.bits.b1OnOFF_Cool)
    //     rtc_sleep();
    // else
    App_SleepDeal();

    if ((Sleep_Mode.all & 0x00ff))
    {
        LogRecord_Flag.bits.Log_Sleep = 1;
        LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
        SleepDeal_Continue();
    }
}

void print_irq_cnt(void)
{
#if 0
    log_w("*********************");
    log_e("cnt_uart1_irq %d", cnt_uart1_irq);
    log_e("cnt_485_can_irq %d", cnt_485_can_irq);
    log_e("cnt_uart3_irq %d", cnt_uart3_irq);
    log_e("cnt_bms1_keyirq %d", cnt_bms1_keyirq);
    log_e("cnt_bms2_keyirq %d", cnt_bms2_keyirq);
    log_e("cnt_bms3_keyirq %d", cnt_bms3_keyirq);
    log_e("cnt_PA0_irq %d", cnt_PA0_irq);
#endif
}

void disable_int(void)
{
    // DISABLE_INT();
#if 1
#if defined(UART1_WAKEUP_ENABLE)
    exti_conf(EXTI_Line7, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(UART3_WAKEUP_ENABLE)
    exti_conf(EXTI_Line3, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(RS485_CAN_WAKEUP_ENABLE)
    exti_conf(EXTI_Line14, EXTI_Trigger_Rising, DISABLE);
#endif

#ifdef __STM32F0__
    RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
#endif // __STM32F0__
#ifdef __STM32F1__
    RTC_ITConfig(RTC_FLAG_ALR, DISABLE);
#endif // __STM32F1__
#endif
}

void rtc_sleep(void)
{
    if (!gu8_1000msAccClock_Flag)
        return;
    gu8_1000msAccClock_Flag = 0;

    if (System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM))
    {
        ReadEEPROM_Byte(0);
    }

    BQ769x0_SleepMode_Ctrl();
    static uint8_t state_sleep = 0;
    static uint32_t sleep_cnt = 0;

    switch (state_sleep)
    {
    case 0:
    {
        if (g_sleepModeSelect == HICCUP_MODE)
        {
            // Sleep_Mode.bits.b1_ToSleepFlag = 1;
            // LogRecord_Flag.bits.Log_Sleep = 1;
            // USART_DeInit(USART1);
            state_sleep = 1;
            break;
        }
        if (g_sleepModeSelect == DEEP_MODE)
        {
            Sleep_Mode.bits.b1_ToSleepFlag = 1;
            LogRecord_Flag.bits.Log_Sleep = 1;
            state_sleep = 1;
            break;
        }
        break;
    }
    case 1:
    {
        if (Sleep_Mode.bits.b1_ToSleepFlag)
        {
            return;
        }
        switch (g_sleepModeSelect)
        {
        case NORMAL_MODE:
            log_e("enter rtc mode2\n");
            break;
        case HICCUP_MODE:
        {
            before_rtcsleep();
            //  IOstatus_RTCMode();
            //  InitWakeUp_RTCMode();
        _rtcsleep:
            Init_RTC();
            // RTC_WKTimeConfig();
            IOstatus_RTCMode();
            InitWakeUp_RTCMode();

            is_rtc_wakekup = false;
            set_irq_wksource(NO_IRQ);

            Feed_IWatchDog;
            Sys_StopMode();
            Feed_IWatchDog;

            disable_int();
            // deal_wakeup();
            InitSci();
            print_irq_cnt();

            if (is_rtc_wakekup)
            {
                Init();

                ++sleep_cnt;
                g_rtcInfo.rtc_sleepTime = sleep_cnt * g_tParam.other.time_sleep_rtcing;
                log_e("sleep time: %d sec\n", g_rtcInfo.rtc_sleepTime);
                // getdata_and_analyse()
                if (isException())
                {
                    goto error;
                }
                else
                {
                    doWork_rtcing(&sleep_cnt);

                    goto _rtcsleep;
                }
            }
        error:
            // todo
            //  deal_exception_and_record();
            is_rtc_wakekup = false;
            Init();

            state_sleep = 0;
            entersleep(NO_SLEEP);

            report_wkup_sig();

            before_wakeup(&sleep_cnt);
            sleep_cnt = 0;
        }
        break;
        case DEEP_MODE:
            // DEEP_SLEEP:
            if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_DEEP_SLEEP_VALUE))
            {
                // App_LogRecord();
                LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);

                log_w("deep sleep\n");
                MCU_RESET();
                break;
            }
        default:
            break;
        }
    }
    default:
        break;
    }
}

static bool rtc_monitor(void)
{
    bool result = false;

#if (AFE_TYPE == bq76xx_afe)
    result = rtc_monitor_bq7x();
#elif (AFE_TYPE == sh36xx)
    result = rtc_monitor_sh367309();
#else
#error "error!!!"
#endif

    return result;
}

static bool rtc_monitor_bq7x(void)
{
#if AFE_TYPE == bq76xx_afe
    bool result = false;

    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, &(Registers_AFE1.SysCtrl2.SysCtrl2Byte));
    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, &(Registers_AFE1.SysStatus.StatusByte));

    if (!Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON || !Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON)
    {
        set_irq_wksource(chg_dsg_close);
        log_e("chg dsg off\n");

        result = true;
    }

    return result;
#endif
    return false;
}

static bool rtc_monitor_sh367309(void)
{
#if AFE_TYPE == sh36xx

    bool result = false;
    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        // g_stCellInfoReport.u16BalanceFlag1 = SH367309_Reg_Store.u8_MTP_BALANCEL;
        // g_stCellInfoReport.u16BalanceFlag2 = SH367309_Reg_Store.u8_MTP_BALANCEH;
        // SystemStatus.bits.b1Status_MOS_PRE = SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET;
        SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
        SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;

        // TemperatureCheck();
        // Fault_ChangeToMCU();
        if (!SystemStatus.bits.b1Status_MOS_CHG)
        {
            result = true;
            log_w("CHG close\n");
            set_irq_wksource(chg_dsg_close);
        }
        if (!SystemStatus.bits.b1Status_MOS_DSG)
        {
            result = true;
            log_w("DSG close\n");
            set_irq_wksource(chg_dsg_close);
        }
    }
    return result;

#endif
}

bool isException(void)
{
    // todo rtcÆðÀ´¶Áafe±£»¤×´Ì¬ 2¡¢ocvÂß¼­ ´óµçÁ÷ ÑÓÊ±ocv
    if (!updataAFEData_once())
    {
        return true;
    }
    // todo read AFE status and to deal logi
    if (isHaveCurrent() || rtc_monitor() || isVol_cuv() || isVol_cov())
    // if (isHaveCurrent() || isVol_cuv() || isVol_cov())
    {
        return true;
    }
    // TOTEST
    // else if (AFE_SleepMode_Judge() == 1)
    // {
    //     log_e("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½");

    //     return true;
    // }

    return false;
}

bool updataAFEData_once(void)
{
#if (AFE_TYPE == bq76xx_afe)

    return updataData_rtc_bq7x();

#elif (AFE_TYPE == sh36xx)

    return updataData_rtc_sh3x();

#else

#error "error"

#endif

    // return true;
}

// todo ï¿½ï¿½Òªï¿½ï¿½ï¿½ï¿½afeï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½
static bool updataData_rtc_bq7x(void)
{
#if (AFE_TYPE == bq76xx_afe)
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1))
    {
        log_e("IIC1 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
#if (SNum > 16)
    if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    {
        log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        return false;
    }
#endif
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();

    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();

    log_i("temp1 %d, temp2 %d, mos temp %d", (g_stCellInfoReport.u16Temperature[0] - 400) / 10, (g_stCellInfoReport.u16Temperature[1] - 400) / 10, (g_stCellInfoReport.u16Temperature[MOS_TEMP1] - 400) / 10);

    return true;

#endif
}

static bool updataData_rtc_sh3x(void)
{
#if (AFE_TYPE == sh36xx)
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo())
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
    // if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    // {
    // 	log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // return false;
    // }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();

#ifdef __test__
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
#endif

    return true;

#endif
}

void get_soc(SOC_T *soc, uint16_t vcell_min, uint16_t vcell_max, uint16_t vcell_mean)
{
}

static void before_wakeup(uint32_t *_sleep_cnt)
{
#define SOC_ERROR 10

    clear_ocv_state_rtcing();

    su32_Interval_S_Tcnt += g_rtcInfo.rtc_sleepTime;

    if (g_rtcInfo.rtc_ocv_success)
    {
        log_e("rtc soc ocv success %d old disp_soc %d, real soc %d\n", soc_update_wakeup.soc_disp, soc_befor_sleep.soc_disp, soc_befor_sleep.soc_real);

        if (g_rtcInfo.rtc_sleepTime > LONG_LONG_RTCSLEEP)
        {
            set_soc_param(soc_update_wakeup.soc_disp, 11, 1);
            // log_e("sleep_cnt %d, rtc soc ocv success %d old disp_soc %d, real soc %d\n", *_sleep_cnt, soc_update_wakeup.soc_disp, soc_befor_sleep.soc_disp, soc_befor_sleep.soc_real);
            // log_e("rtc soc ocv success %d old disp_soc %d, real soc %d\n", soc_update_wakeup.soc_disp, soc_befor_sleep.soc_disp, soc_befor_sleep.soc_real);
        }
        else
        {
            set_soc_param(soc_update_wakeup.soc_disp, 11, 0);
        }
    }
    else
    {
        log_e("no update soc");
    }
    log_e("thi rtc sleep time %d sec, from last event all sleep %d sec", g_rtcInfo.rtc_sleepTime, su32_Interval_S_Tcnt);

    print_soc(false, 0);
    log_w("**************************before_wakeup***************************");
}

static void before_rtcsleep(void)
{
    g_rtcInfo.rtc_ocv_success = false;

    extern void err_flag_reset(void);
    err_flag_reset();
    // SOC_OCV_Fix2_var_reset();
    // todo before sleep clear all cnt
    log_e("**************************before_rtcsleep***************************");
    soc_befor_sleep.soc_disp = get_dispsoc();
    soc_befor_sleep.soc_real = get_soc_real();
    print_soc(false, 0);
}

static uint8_t array_soc[5];

void print_soc(bool prinf_ocv_soc, uint8_t ocv_soc)
{
    if (prinf_ocv_soc)
        log_w("ocv_soc: %d, real soc: %d, disp soc: %d", ocv_soc, get_soc_real(), get_dispsoc());
    else
        log_w("real soc: %d, disp soc: %d", get_soc_real(), get_dispsoc());
}

void print_bms_info(void)
{
    // todo
}

// todo ç­–ç•¥1ã€é•¿æ—¶é—´å¾…æœºç›´æŽ¥æ ? 2ã€åªæ ‡å‡†realï¼Œdispå……æ”¾ç”µé€¼è¿‘
#define N 7

#if 1
static uint8_t rtc_ocv_state = 0;
static uint8_t ocv_cnt_rtcing = 0;

static void clear_ocv_state_rtcing(void)
{
    rtc_ocv_state = 0;
    ocv_cnt_rtcing = 0;
}
// todo æ»‘åŠ¨æ»¤æ³¢
bool update_rtc_soc(uint32_t *_sleep_cnt)
{
    static uint8_t ocv_soc_record[10];
    static uint8_t arrSoc_rtc[N] = {0, 0, 0, 0, 0};

    // if (g_stCellInfoReport.u16Ichg > 2 || g_stCellInfoReport.u16IDischg > 2)
    // {

    // 	// return;
    // }
    if (g_stCellInfoReport.u16VCellMin > OCV_VOL_ENABLE)
    {
        rtc_ocv_state = 0;
        ocv_cnt_rtcing = 0;
        return true;
    }
    switch (rtc_ocv_state)
    {
    case 0:
    {
        if (g_rtcInfo.rtc_sleepTime < RTC_SOC_OCV_TIME)
            return true;
        else
        {
            rtc_ocv_state = 1;
        }
        break;
    }
    case 1:
    {
        {
            arrSoc_rtc[ocv_cnt_rtcing] = get_soc_from_openVol_onlyDec_new(g_stCellInfoReport.u16VCellMin);
            log_w("vcellmin %d arr_soc[%d] %d", g_stCellInfoReport.u16VCellMin, ocv_cnt_rtcing, arrSoc_rtc[ocv_cnt_rtcing]);

            if (++ocv_cnt_rtcing >= N)
            {
                ocv_cnt_rtcing = 0;
                rtc_ocv_state = 1;

                // extern uint8_t get_ocv_cali(void);
                extern uint8_t get_ocv_cali(uint8_t *arr_soc);

                soc_update_wakeup.soc_disp = get_ocv_cali(arrSoc_rtc);
                print_soc(true, soc_update_wakeup.soc_disp);

                g_rtcInfo.rtc_ocv_success = true;
            }
        }
        break;
    }
    default:
        break;
    }
}
#endif

void set_irq_wksource(uint8_t irq)
{
    g_irq_t = (enum irqWakeup)irq;
}

static void report_wkup_sig(void)
{
    switch (g_irq_t)
    {
    case uart1_irq:
        log_e(enumToStr(uart1_irq));
        break;
    case uart2_irq:
        // log_e("uart2_irq");
        log_e(enumToStr(uart2_irq));
        break;
    case uart3_irq:
        // log_e("uart3_irq");
        log_e(enumToStr(uart3_irq));
        break;
    case PA0_irq:
        // log_e("PA0_irq");
        log_e(enumToStr(PA0_irq));
        break;
    case bms_keyirq:
        //    case bms2_keyirq:
        //    case bms3_keyirq:
        // log_e("bms_keyirq");
        log_e(enumToStr(bms_keyirq));
        break;
    case soc_key:
        // log_e("soc_key_irq");
        log_e(enumToStr(soc_key));
        break;
    case CHG_IRQ:
        // log_e("CHG_IRQ");
        log_e(enumToStr(CHG_IRQ));
        break;
    case current_wake:
        // log_e("g_test %d, exception wakeup", g_irq_t);
        log_e(enumToStr(current_wake));
        break;
    case chg_dsg_close:
        // log_e("g_test %d, exception wakeup", g_irq_t);
        log_e(enumToStr(chg_dsg_close));
        break;
    case error_wake:
        // log_e("g_test %d, exception wakeup", g_irq_t);
        log_e(enumToStr(error_wake));
        break;
    case cuv_wake:
        log_e(enumToStr(cuv_wake));
        break;
    case cov_wake:
        log_e(enumToStr(cov_wake));
        break;
    case rs485_irq:
        log_e(enumToStr(rs485_irq));
        break;
    // case :
    //     log_e(enumToStr(error_wake));
    // case error_wake:
    //     log_e(enumToStr(error_wake));
    default:
        // todo fixme no def bug
        log_e("no def");
        log_e("g_test %d, exception wakeup", g_irq_t);
        break;
    }

    g_irq_t = NO_IRQ;
}

static bool isErr_enterRTC(void)
{
    // if ((g_stCellInfoReport.u16Ichg > 10) || (g_stCellInfoReport.u16IDischg > 10))
    if ((g_stCellInfoReport.u16Ichg) || (g_stCellInfoReport.u16IDischg))
    {
        log_e("CHG or DSG");
        return true;
    }
    // else if (g_stCellInfoReport.unMdlFault_Third.all)
    // {
    //     log
    //     return true;

    // }
    else if (SystemStatus.bits.b1Status_Heat)
    {
        log_e("Heating");
        return true;
    }
    else if (g_enBalanceState == BALANCE_ST_ODD_ON || g_enBalanceState == BALANCE_ST_EVEN_ON)
    {
        log_e("Balancing");
        return true;
    }
#ifdef __same_door__
    else if (!SystemStatus.bits.b1Status_MOS_CHG || !SystemStatus.bits.b1Status_MOS_DSG)
    {
        log_e("mos close");
        return true;
    }
#else
    else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1)
    {
        log_e("diff door and is CHGING");
        return true;
    }
#endif
    else
    {
        return false;
    }
}

void rtc_normal_sleep(uint32_t *_sleep_cnt)
{
#if 0
                    // if (sleep_cnt / 3 >= g_tParam.other.u16Sleep_TimeNormal)
                    if (sleep_cnt * g_tParam.other.time_sleep_rtcing / 60 >= g_tParam.other.u16Sleep_TimeNormal)
                    // if (sleep_cnt * 20 / 60 >= g_tParam.other.u16Sleep_TimeNormal)
                    {
                        log_e("enter normal sleep");
                        before_wakeup(&sleep_cnt);
                        // entersleep(DEEP_MODE);
                        LogRecord_Flag.bits.Log_Sleep = 1;

                        goto DEEP_SLEEP;
                    }
                    // run_idle_and_record();
#endif
}
static void doWork_rtcing(uint32_t *_sleep_cnt)
{
    update_rtc_soc(_sleep_cnt);

    rtc_normal_sleep(_sleep_cnt);
    // log_w("continue rtc, sleep %ds, %d min sleep\n", TIME_SLEEP_RTCING, g_tParam.other.u16Sleep_TimeNormal - *_sleep_cnt * TIME_SLEEP_RTCING / 60);
    log_w("continue rtc, sleep %ds, %d min sleep\n", g_tParam.other.time_sleep_rtcing, g_tParam.other.u16Sleep_TimeNormal - *_sleep_cnt * g_tParam.other.time_sleep_rtcing / 60);
}

void EXTI_ENTRANCE_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        set_irq_wksource(PA0_irq);
        EXTI_ClearITPendingBit(EXTI_Line0);
        ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 1;
    }
    if (EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
    if (EXTI_GetITStatus(EXTI_Line2) != RESET)
    {
        // ++cnt_bms1_keyirq;
        set_irq_wksource(CHG_IRQ);

        EXTI_ClearITPendingBit(EXTI_Line2);
    }
    if (EXTI_GetITStatus(EXTI_Line3) != RESET)
    {
        // ++cnt_bms2_keyirq;
        // set_irq_wksource(bms2_keyirq);
        EXTI_ClearITPendingBit(EXTI_Line3);
    }
    if (EXTI_GetITStatus(EXTI_Line6) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line6);
    }
    if (EXTI_GetITStatus(EXTI_Line7) != RESET)
    {
#ifdef UART1_WAKEUP_ENABLE
        ++cnt_uart1_irq;
        set_irq_wksource(uart1_irq);

        EXTI_ClearITPendingBit(EXTI_Line7);
#endif
    }
    if (EXTI_GetITStatus(EXTI_Line8) != RESET)
    {
        // set_irq_wksource(rs485_irq);
        EXTI_ClearITPendingBit(EXTI_Line8);
    }
    if (EXTI_GetITStatus(EXTI_Line9) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line9);
    }
    if (EXTI_GetITStatus(EXTI_Line10) != RESET)
    {
        // set_irq_wksource(PA0_irq);
        EXTI_ClearITPendingBit(EXTI_Line10);
    }
    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line11);
    }
    if (EXTI_GetITStatus(EXTI_Line12) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line12);
    }

    if (EXTI_GetITStatus(EXTI_Line13) != RESET)
    {
        set_irq_wksource(bms_keyirq);
        EXTI_ClearITPendingBit(EXTI_Line13);
    }
    if (EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
#ifdef RS485_CAN_WAKEUP_ENABLE
        ++cnt_485_can_irq;
        set_irq_wksource(rs485_irq);
        EXTI_ClearITPendingBit(EXTI_Line14);
#endif
    }
}