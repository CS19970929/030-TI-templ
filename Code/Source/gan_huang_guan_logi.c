#include "main.h"

#define GAN1_OFF_SLEEP_TICKS_10MS ((UINT16)50)
#define GAN2_OFF_SLEEP_TICKS_10MS ((UINT16)200)
#define GAN3_SOC_TICKS_10MS ((UINT16)100)
#define GAN3_POWER_TICKS_10MS ((UINT16)300)
#define WATER_SLEEP_TICKS_10MS ((UINT16)12000)
#define CHARGER_LOST_TICKS_10MS ((UINT16)100)
#define CHARGE_CURRENT_MIN_0P1A ((UINT16)2)

static UINT16 s_gan1_off_ticks = 0;
static UINT16 s_gan2_off_ticks = 0;
static UINT16 s_water_ticks = 0;
static UINT16 s_charger_lost_ticks = 0;
static UINT16 s_gan3_hold_ticks = 0;
static UINT8 s_charge_latched = 0;
static UINT8 s_sleep_requested = 0;
static UINT8 s_gan3_prev = 0;
static UINT8 s_gan3_long_handled = 0;
static UINT8 s_gan3_soc_handled = 0;
static UINT8 s_gan3_wait_release = 1;

static void Gan_SetDriverKeep(void);
static void Gan_SetDriverClose(void);

bool is_open_gan1(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN1, PIN_GAN1);
}

bool is_open_gan2(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN2, PIN_GAN2);
}

bool is_open_gan3(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN3, PIN_GAN3);
}

bool is_open_gan4(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN4, PIN_GAN4);
}

bool is_water_in(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_SWT_AD, PIN_SWT_AD);
}

bool is_charger_online(void)
{
    return 1 == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
}

UINT8 ganhuangguan_IsChargeLatched(void)
{
    return s_charge_latched;
}

static UINT16 Gan_GetChargeCurrentThreshold(void)
{
    UINT16 threshold = OtherElement.u16Sleep_VirCur_Chg;

    if (threshold < CHARGE_CURRENT_MIN_0P1A)
    {
        threshold = CHARGE_CURRENT_MIN_0P1A;
    }

    return threshold;
}

static UINT8 Gan_IsChargeCurrentActive(void)
{
    return (UINT8)(g_stCellInfoReport.u16Ichg > Gan_GetChargeCurrentThreshold());
}

static UINT8 Gan_IsSocFull(void)
{
    return (UINT8)(g_stCellInfoReport.SocElement.u16Soc >= 100);
}

static UINT8 Gan_IsPackAtSoc100Voltage(void)
{
    if (OtherElement.u16Soc_V_100 == 0)
    {
        return 0;
    }

    return (UINT8)(g_stCellInfoReport.u16VCellMax >= OtherElement.u16Soc_V_100);
}

static UINT8 Gan_IsChargerLostCondition(UINT8 charge_current_active)
{
    return (UINT8)((!charge_current_active) &&
                   (!Gan_IsSocFull()) &&
                   (!Gan_IsPackAtSoc100Voltage()));
}

static void Gan_RequestSleep(void)
{
    if (s_sleep_requested)
    {
        return;
    }

    s_sleep_requested = 1;
    Gan_SetDriverClose();
    LedBar_SetChargeDisplay(0);
    LedBar_SetDischargeDisplay(0);
    LedBar_SetWaterAlarm(0);
    sleep_reason = 2;
    entersleep(DEEP_MODE);
}

static void Gan_SetDriverKeep(void)
{
    Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_KEEP_MODE;
}

static void Gan_SetDriverClose(void)
{
    Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE;
}

static void Gan_ProcessWater(UINT8 gan1_on, UINT8 gan2_on)
{
    Gan_SetDriverClose();
    LedBar_SetChargeDisplay(0);
    LedBar_SetDischargeDisplay(0);
    LedBar_SetWaterAlarm(1);
    GPIO_WriteBit(GPIO_SWT_EN, PIN_SWT_EN, Bit_RESET);

    if (!gan1_on || !gan2_on)
    {
        Gan_RequestSleep();
        return;
    }

    if (s_water_ticks < WATER_SLEEP_TICKS_10MS)
    {
        ++s_water_ticks;
    }

    if (s_water_ticks >= WATER_SLEEP_TICKS_10MS)
    {
        Gan_RequestSleep();
    }
}

static void Gan_ClearWater(void)
{
    s_water_ticks = 0;
    LedBar_SetWaterAlarm(0);
    GPIO_WriteBit(GPIO_SWT_EN, PIN_SWT_EN, Bit_SET);
}

static UINT8 Gan_ProcessCharge(UINT8 gan1_on)
{
    UINT8 charge_current_active = Gan_IsChargeCurrentActive();

    if (gan1_on && (is_charger_online() || charge_current_active))
    {
        s_charge_latched = 1;
        s_charger_lost_ticks = 0;
    }

    if (!s_charge_latched)
    {
        return 0;
    }

    if (!gan1_on)
    {
        s_charge_latched = 0;
        LedBar_SetChargeDisplay(0);
        Gan_RequestSleep();
        return 1;
    }

    Gan_SetDriverKeep();
    LedBar_SetDischargeDisplay(0);

    if (charge_current_active || Gan_IsSocFull() || Gan_IsPackAtSoc100Voltage())
    {
        s_charger_lost_ticks = 0;
        LedBar_Command = LED_BAR_CHG;
        LedBar_SetChargeDisplay(1);
    }
    else
    {
        UINT8 charger_lost_condition = Gan_IsChargerLostCondition(charge_current_active);

        LedBar_Command = LED_BAR_NORMAL;
        LedBar_SetChargeDisplay(0);
        if (charger_lost_condition && s_charger_lost_ticks < CHARGER_LOST_TICKS_10MS)
        {
            ++s_charger_lost_ticks;
        }
        if (!charger_lost_condition)
        {
            s_charger_lost_ticks = 0;
        }
        if (s_charger_lost_ticks >= CHARGER_LOST_TICKS_10MS)
        {
            s_charge_latched = 0;
            Gan_RequestSleep();
        }
    }

    return 1;
}

static void Gan_ProcessTopKey(UINT8 gan1_on, UINT8 gan2_on, UINT8 charge_active)
{
    UINT8 gan3_on = is_open_gan3() ? 1 : 0;

    if (charge_active || LedBar_IsShutdownAnimationActive())
    {
        if (!gan3_on)
        {
            s_gan3_hold_ticks = 0;
            s_gan3_prev = 0;
            s_gan3_long_handled = 0;
            s_gan3_soc_handled = 0;
            s_gan3_wait_release = 0;
        }
        return;
    }

    if (s_gan3_wait_release)
    {
        if (!gan3_on)
        {
            s_gan3_wait_release = 0;
        }
        s_gan3_prev = gan3_on;
        return;
    }

    if (gan3_on)
    {
        if (!s_gan3_prev)
        {
            s_gan3_hold_ticks = 0;
            s_gan3_long_handled = 0;
            s_gan3_soc_handled = 0;
        }

        if (s_gan3_hold_ticks < 0xFFFF)
        {
            ++s_gan3_hold_ticks;
        }

        if (!s_gan3_soc_handled && s_gan3_hold_ticks >= GAN3_SOC_TICKS_10MS)
        {
            s_gan3_soc_handled = 1;
            LedBar_RequestSocTemporary(g_stCellInfoReport.SocElement.u16Soc, 30);
        }

        if (!s_gan3_long_handled && s_gan3_hold_ticks >= GAN3_POWER_TICKS_10MS)
        {
            s_gan3_long_handled = 1;
            s_gan3_wait_release = 1;

            if (gan1_on && gan2_on)
            {
                sleep_reason = 1;
                LedBar_RequestShutdownAnimation();
            }
        }
    }
    else
    {
        s_gan3_hold_ticks = 0;
        s_gan3_long_handled = 0;
        s_gan3_soc_handled = 0;
    }

    s_gan3_prev = gan3_on;
}

static void Gan_ProcessDischarge(UINT8 gan1_on, UINT8 gan2_on)
{
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_SetChargeDisplay(0);

    if (!gan1_on)
    {
        LedBar_SetDischargeDisplay(0);
        if (s_gan1_off_ticks < GAN1_OFF_SLEEP_TICKS_10MS)
        {
            ++s_gan1_off_ticks;
        }
        if (s_gan1_off_ticks >= GAN1_OFF_SLEEP_TICKS_10MS)
        {
            Gan_RequestSleep();
        }
        return;
    }

    s_gan1_off_ticks = 0;

    if (!gan2_on)
    {
        Gan_SetDriverKeep();
        if (s_gan2_off_ticks < GAN2_OFF_SLEEP_TICKS_10MS)
        {
            ++s_gan2_off_ticks;
        }
        if (s_gan2_off_ticks >= GAN2_OFF_SLEEP_TICKS_10MS)
        {
            LedBar_SetDischargeDisplay(0);
            Gan_RequestSleep();
        }
        return;
    }

    s_gan2_off_ticks = 0;
    Gan_SetDriverKeep();
    LedBar_SetDischargeDisplay(1);
}

void ganhuangguan_Logi(void)
{
    UINT8 gan1_on = is_open_gan1() ? 1 : 0;
    UINT8 gan2_on = is_open_gan2() ? 1 : 0;
    UINT8 charge_path_active = 0;

    if (s_sleep_requested)
    {
        Gan_SetDriverClose();
        return;
    }

    if (is_water_in())
    {
        s_charge_latched = 0;
        Gan_ProcessWater(gan1_on, gan2_on);
        return;
    }

    Gan_ClearWater();

    charge_path_active = Gan_ProcessCharge(gan1_on);
    Gan_ProcessTopKey(gan1_on, gan2_on, charge_path_active);

    if (charge_path_active)
    {
        return;
    }

    Gan_ProcessDischarge(gan1_on, gan2_on);
}
