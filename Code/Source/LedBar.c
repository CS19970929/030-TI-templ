#include "main.h"
#include "Flash.h"
#include "gan_huang_guan_logi.h"

uint8_t sleep_reason = 0;

extern UINT8 WakeDisplayState_Read(UINT16 *mode, UINT16 *soc);

LEDBAR_COMMAND LedBar_Command = LED_BAR_NORMAL;

#define LEDBAR_MASK_ALL ((UINT8)0x1F)
#define LEDBAR_SOC_TEMP_TICKS_100MS ((UINT16)30)
#define LEDBAR_CHG_BLINK_TICKS_100MS ((UINT8)5)
#define LEDBAR_WATER_BLINK_TICKS_100MS ((UINT8)2)
#define LEDBAR_ANIM_STEP_TICKS_100MS ((UINT8)1)
#define LEDBAR_PREBOOT_SOC_TRIGGER_TICKS_10MS ((UINT16)1)
#define LEDBAR_PREBOOT_POWERON_TICKS_10MS ((UINT16)200)
#define LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS ((UINT16)300)
#define LEDBAR_PREBOOT_RELEASE_TICKS_10MS ((UINT16)10)
#define LEDBAR_WATER_PREBOOT_SLEEP_TICKS_10MS ((UINT16)12000)
#define LEDBAR_WATER_PREBOOT_BLINK_TICKS_10MS ((UINT16)50)

typedef enum _LEDBAR_UI_MODE
{
    LED_UI_NORMAL = 0,
    LED_UI_SOC_TEMP,
    LED_UI_BOOT_ANIM_ON,
    LED_UI_BOOT_ANIM_OFF,
    LED_UI_SHUTDOWN_ANIM
} LEDBAR_UI_MODE;

static LEDBAR_UI_MODE s_led_ui_mode = LED_UI_NORMAL;
static UINT8 s_anim_step = 0;
static UINT8 s_anim_step_ticks = 0;
static UINT16 s_ui_ticks = 0;
static UINT16 s_temp_soc = 0;
static UINT16 s_temp_duration_ticks = LEDBAR_SOC_TEMP_TICKS_100MS;
static UINT8 s_discharge_display_enable = 0;
static UINT8 s_charge_display_enable = 0;
static UINT8 s_water_alarm_enable = 0;
static UINT8 s_water_alarm_on = 0;
static UINT8 s_water_alarm_ticks = 0;
static UINT8 s_charge_blink_on = 1;
static UINT8 s_charge_blink_ticks = 0;
static UINT8 s_shutdown_animation_active = 0;

void LedBar_Show_Fault(void);

static UINT8 LedBar_ClampSoc(UINT16 soc)
{
    if (soc > 100)
    {
        return 100;
    }

    return (UINT8)soc;
}

static UINT8 LedBar_GetSocMaskFromValue(UINT16 soc)
{
    UINT8 mask = 0;
    UINT8 soc_clamped = LedBar_ClampSoc(soc);

    mask |= 0x01;
    if (soc_clamped >= 20)
    {
        mask |= 0x02;
    }
    if (soc_clamped >= 40)
    {
        mask |= 0x04;
    }
    if (soc_clamped >= 60)
    {
        mask |= 0x08;
    }
    if (soc_clamped >= 80)
    {
        mask |= 0x10;
    }

    return mask;
}

static UINT8 LedBar_GetLiveSocMask(void)
{
    return LedBar_GetSocMaskFromValue(g_stCellInfoReport.SocElement.u16Soc);
}

static void LedBar_SetByMask(UINT8 mask)
{
    MCUO_SOC_20 = (mask & 0x01) ? 1 : 0;
    MCUO_SOC_40 = (mask & 0x02) ? 1 : 0;
    MCUO_SOC_60 = (mask & 0x04) ? 1 : 0;
    MCUO_SOC_80 = (mask & 0x08) ? 1 : 0;
    MCUO_SOC_100 = (mask & 0x10) ? 1 : 0;
}

static void LedBar_SetAllOff(void)
{
    LedBar_SetByMask(0);
}

static void LedBar_InitOutputPins(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Pin |= GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

UINT8 LedBar_IsWakePreviewPending(void)
{
    UINT16 wake_mode = WAKE_DISPLAY_MODE_NONE;
    UINT16 wake_soc = 0;

    if (!WakeDisplayState_Read(&wake_mode, &wake_soc))
    {
        return 0;
    }

    (void)wake_soc;
    return (UINT8)((wake_mode == WAKE_DISPLAY_MODE_SOC_PREVIEW) ||
                   (wake_mode == WAKE_DISPLAY_MODE_WATER_ALARM));
}

void LedBar_RequestSocTemporary(UINT16 soc, UINT16 duration_100ms)
{
    if (s_water_alarm_enable || s_led_ui_mode == LED_UI_SHUTDOWN_ANIM)
    {
        return;
    }

    s_temp_soc = soc;
    s_temp_duration_ticks = duration_100ms;
    if (s_temp_duration_ticks == 0)
    {
        s_temp_duration_ticks = LEDBAR_SOC_TEMP_TICKS_100MS;
    }
    s_ui_ticks = 0;
    s_led_ui_mode = LED_UI_SOC_TEMP;
    LedBar_SetByMask(LedBar_GetSocMaskFromValue(s_temp_soc));
}

void LedBar_RequestBootAnimation(UINT16 soc)
{
    if (s_water_alarm_enable)
    {
        return;
    }

    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_shutdown_animation_active = 0;
    s_temp_soc = soc;
    s_led_ui_mode = LED_UI_BOOT_ANIM_ON;
    LedBar_SetAllOff();
}

void LedBar_RequestShutdownAnimation(void)
{
    if (s_led_ui_mode == LED_UI_SHUTDOWN_ANIM)
    {
        return;
    }

    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_shutdown_animation_active = 1;
    s_led_ui_mode = LED_UI_SHUTDOWN_ANIM;
    LedBar_SetByMask(LEDBAR_MASK_ALL);
}

void LedBar_SetDischargeDisplay(UINT8 enable)
{
    s_discharge_display_enable = enable ? 1 : 0;
}

void LedBar_SetChargeDisplay(UINT8 enable)
{
    s_charge_display_enable = enable ? 1 : 0;
    if (!s_charge_display_enable)
    {
        s_charge_blink_on = 1;
        s_charge_blink_ticks = 0;
    }
}

void LedBar_SetWaterAlarm(UINT8 enable)
{
    if (enable)
    {
        s_water_alarm_enable = 1;
        GPIO_WriteBit(GPIO_SWT_EN, PIN_SWT_EN, Bit_RESET);
        return;
    }

    if (s_water_alarm_enable)
    {
        s_water_alarm_on = 0;
        s_water_alarm_ticks = 0;
        LedBar_SetAllOff();
    }
    s_water_alarm_enable = 0;
}

UINT8 LedBar_IsShutdownAnimationActive(void)
{
    return s_shutdown_animation_active;
}

static void LedBar_RunSocTemp(void)
{
    LedBar_SetByMask(LedBar_GetSocMaskFromValue(s_temp_soc));

    if (++s_ui_ticks < s_temp_duration_ticks)
    {
        return;
    }

    s_ui_ticks = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    LedBar_SetAllOff();
}

static void LedBar_RunBootAnimOn(void)
{
    if (++s_anim_step_ticks < LEDBAR_ANIM_STEP_TICKS_100MS)
    {
        return;
    }
    s_anim_step_ticks = 0;

    if (s_anim_step < 5)
    {
        ++s_anim_step;
        LedBar_SetByMask((UINT8)((1U << s_anim_step) - 1U));
        return;
    }

    s_anim_step = 0;
    s_led_ui_mode = LED_UI_BOOT_ANIM_OFF;
    LedBar_SetAllOff();
}

static void LedBar_RunBootAnimOff(void)
{
    if (++s_anim_step_ticks < LEDBAR_ANIM_STEP_TICKS_100MS)
    {
        return;
    }
    s_anim_step_ticks = 0;

    s_anim_step = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    s_discharge_display_enable = 1;
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_SetByMask(LedBar_GetLiveSocMask());
}

static void LedBar_RunShutdownAnim(void)
{
    if (++s_anim_step_ticks < LEDBAR_ANIM_STEP_TICKS_100MS)
    {
        return;
    }
    s_anim_step_ticks = 0;

    if (s_anim_step < 5)
    {
        ++s_anim_step;
        LedBar_SetByMask((UINT8)(LEDBAR_MASK_ALL >> s_anim_step));
        return;
    }

    s_anim_step = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    s_shutdown_animation_active = 0;
    s_discharge_display_enable = 0;
    s_charge_display_enable = 0;
    LedBar_SetAllOff();
    entersleep(DEEP_MODE);
}

static void LedBar_RunWaterAlarm(void)
{
    if (++s_water_alarm_ticks < LEDBAR_WATER_BLINK_TICKS_100MS)
    {
        return;
    }

    s_water_alarm_ticks = 0;
    s_water_alarm_on = s_water_alarm_on ? 0 : 1;
    LedBar_SetByMask(s_water_alarm_on ? LEDBAR_MASK_ALL : 0);
}

static void LedBar_ShowCharge(void)
{
    UINT8 i;
    UINT8 stage;
    UINT8 mask = 0;
    UINT16 soc = g_stCellInfoReport.SocElement.u16Soc;

    if (++s_charge_blink_ticks >= LEDBAR_CHG_BLINK_TICKS_100MS)
    {
        s_charge_blink_ticks = 0;
        s_charge_blink_on = s_charge_blink_on ? 0 : 1;
    }

    if (soc >= 99)
    {
        LedBar_SetByMask(LEDBAR_MASK_ALL);
        return;
    }

    stage = (UINT8)(LedBar_ClampSoc(soc) / 20);
    if (stage > 4)
    {
        stage = 4;
    }

    for (i = 0; i < stage; ++i)
    {
        mask |= (UINT8)(1U << i);
    }

    if (s_charge_blink_on)
    {
        mask |= (UINT8)(1U << stage);
    }

    LedBar_SetByMask(mask);
}

void LedBar_Show_Normal(void)
{
    if (g_stCellInfoReport.unMdlFault_Third.all & 0x2FFA ||
        System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) ||
        System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
    {
        LedBar_Show_Fault();
        return;
    }

    LedBar_SetByMask(LedBar_GetLiveSocMask());
}

void LedBar_Show_CHG(void)
{
    LedBar_ShowCharge();
}

void LedBar_Show_DSG(void)
{
    LedBar_SetByMask(LedBar_GetLiveSocMask());
}

void LedBar_Show_Fault(void)
{
    MCUO_SOC_20 = !MCUO_SOC_20;
    MCUO_SOC_40 = 0;
    MCUO_SOC_60 = 0;
    MCUO_SOC_80 = 0;
    MCUO_SOC_100 = 0;
}

void LedBar_Show_Sleep(void)
{
    LedBar_SetAllOff();
}

static void LedBar_RunBootAnimationBlocking(UINT16 soc)
{
    UINT8 step;

    for (step = 1; step <= 5; ++step)
    {
        LedBar_SetByMask((UINT8)((1U << step) - 1U));
        __delay_ms((UINT16)LEDBAR_ANIM_STEP_TICKS_100MS * 100U);
    }

    LedBar_SetAllOff();
    // __delay_ms((UINT16)LEDBAR_ANIM_STEP_TICKS_100MS * 100U);
    __delay_ms(500);

    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    s_discharge_display_enable = 1;
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_SetByMask(LedBar_GetSocMaskFromValue(soc));
}

static UINT8 LedBar_HandleWakeSocPreviewAfterReset(UINT16 wake_soc)
{
    UINT16 hold_ticks = 0;
    UINT16 show_ticks = 0;

    LedBar_SetByMask(LedBar_GetSocMaskFromValue(wake_soc));

    while (1)
    {
        if (is_open_gan3())
        {
            if (hold_ticks < 0xFFFF)
            {
                ++hold_ticks;
            }

            if (hold_ticks >= LEDBAR_PREBOOT_POWERON_TICKS_10MS)
            {
                if (is_open_gan1() && is_open_gan2())
                {
                    LedBar_RunBootAnimationBlocking(wake_soc);
                    // ganhuangguan_WaitGan3ReleaseBeforeLongPress();

                    return 1;
                }

                LedBar_SetAllOff();
                return 0;
            }
        }

        if (++show_ticks >= LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
        {
            LedBar_SetAllOff();
            return 0;
        }

        __delay_ms(10);
    }
}

static void LedBar_HandleWakeWaterAlarmAfterReset(void)
{
    if (is_water_in())
    {
        LedBar_SetWaterAlarm(1);
        LedBar_SetByMask(LEDBAR_MASK_ALL);
        return;
    }

    LedBar_SetWaterAlarm(0);
    LedBar_SetAllOff();
}

// UINT8 LedBar_HandleWakePreviewBeforeBoot(void)
// {
//     UINT16 wake_mode = WAKE_DISPLAY_MODE_NONE;
//     UINT16 wake_soc = 0;

//     LedBar_InitOutputPins();
//     if (!WakeDisplayState_Read(&wake_mode, &wake_soc))
//     {
//         wake_soc = 0;
//     }

//     (void)wake_mode;
//     return LedBar_HandleWakeSocPreviewAfterReset(wake_soc);
// }

void LedBar_RunBootAnimOn_test(void)
{
    LedBar_RequestBootAnimation(g_stCellInfoReport.SocElement.u16Soc);
}

void LedBar_RunShutdownAnim_test(void)
{
    LedBar_RequestShutdownAnimation();
}

void LedBar_StartUp(void)
{
    UINT16 wake_mode = WAKE_DISPLAY_MODE_NONE;
    UINT16 wake_soc = 0;

    LedBar_InitOutputPins();

    LedBar_Command = LED_BAR_NORMAL;
    s_led_ui_mode = LED_UI_NORMAL;
    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_temp_soc = 0;
    s_temp_duration_ticks = LEDBAR_SOC_TEMP_TICKS_100MS;
    s_discharge_display_enable = 0;
    s_charge_display_enable = 0;
    s_water_alarm_enable = 0;
    s_water_alarm_on = 0;
    s_water_alarm_ticks = 0;
    s_charge_blink_on = 1;
    s_charge_blink_ticks = 0;
    s_shutdown_animation_active = 0;

    if (!WakeDisplayState_Read(&wake_mode, &wake_soc))
    {
        LedBar_SetAllOff();
        WakeDisplayMode_ClearKeepSoc();
        return;
    }

    switch (wake_mode)
    {
    case WAKE_DISPLAY_MODE_SOC_PREVIEW:
        if (LedBar_HandleWakeSocPreviewAfterReset(wake_soc))
        {
            WakeDisplayMode_ClearKeepSoc();
            return;
        }
        SleepDeal_ReenterDeepSleepFromWakePreview();
        return;

    case WAKE_DISPLAY_MODE_WATER_ALARM:
        // LedBar_HandleWakeWaterAlarmAfterReset();
        // WakeDisplayMode_ClearKeepSoc();
        LedBar_SetWaterAlarm(1);
        return;

    case WAKE_DISPLAY_MODE_BOOT_SEQUENCE:
        LedBar_RequestBootAnimation(wake_soc);
        WakeDisplayMode_ClearKeepSoc();
        return;

    case WAKE_DISPLAY_MODE_CHARGER_WAKE:
    case WAKE_DISPLAY_MODE_NONE:
    default:
        LedBar_SetAllOff();
        WakeDisplayMode_ClearKeepSoc();
        return;
    }
}

void APP_LedBar(void)
{
    if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
    {
        return;
    }

    if (SystemStatus.bits.b1StartUpBMS)
    {
        return;
    }

    if (s_water_alarm_enable)
    {
        if (s_led_ui_mode != LED_UI_SHUTDOWN_ANIM)
        {
            LedBar_RunWaterAlarm();
            return;
        }
        else
        {
            LedBar_SetAllOff();
        }
    }

    switch (s_led_ui_mode)
    {
    case LED_UI_SOC_TEMP:
        LedBar_RunSocTemp();
        return;

    case LED_UI_BOOT_ANIM_ON:
        LedBar_RunBootAnimOn();
        return;

    case LED_UI_BOOT_ANIM_OFF:
        LedBar_RunBootAnimOff();
        return;

    case LED_UI_SHUTDOWN_ANIM:
        LedBar_RunShutdownAnim();
        return;

    default:
        break;
    }

    if (sleep_reason == 1)
    {
        LedBar_SetAllOff();
        return;
    }

    if (s_charge_display_enable || LedBar_Command == LED_BAR_CHG)
    {
        LedBar_Show_CHG();
        return;
    }

    if (s_discharge_display_enable || LedBar_Command == LED_BAR_NORMAL)
    {
        if (s_discharge_display_enable)
        {
            LedBar_Show_Normal();
        }
        else
        {
            LedBar_SetAllOff();
        }
        return;
    }

    LedBar_SetAllOff();
}

void test_water_in(void)
{
    static bool toggle = false;
    uint16_t i = 0;

    LedBar_InitOutputPins();

    for (i = 0; i < (5 * 60 * 2); i++)
    {
        LedBar_SetByMask(toggle ? LEDBAR_MASK_ALL : 0);
        __delay_ms(200);
        toggle = !toggle;
    }
}