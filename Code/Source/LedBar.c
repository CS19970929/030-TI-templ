#include "main.h"
#include "Flash.h"
#include "gan_huang_guan_logi.h"

#ifndef WAKE_DISPLAY_MODE_NONE
#define WAKE_DISPLAY_MODE_NONE ((UINT16)0x0000)
#endif
#ifndef WAKE_DISPLAY_MODE_SOC_PREVIEW
#define WAKE_DISPLAY_MODE_SOC_PREVIEW ((UINT16)0x0001)
#endif

extern UINT8 WakeDisplayState_Read(UINT16 *mode, UINT16 *soc);
extern void WakeDisplayState_Clear(void);

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

#define LEDBAR_MASK_ALL ((UINT8)0x1F)
#define LEDBAR_LONG_PRESS_TICKS_100MS ((UINT8)30)
#define LEDBAR_SHORT_SHOW_TICKS_100MS ((UINT8)50)
#define LEDBAR_BOOT_PREVIEW_SHOW_TICKS_100MS ((UINT8)30)
#define LEDBAR_BOOT_POST_SHOW_TICKS_100MS ((UINT8)30)
#define LEDBAR_BOOT_DELAY_TICKS_100MS ((UINT8)10)
#define LEDBAR_KEY_DEBOUNCE_TICKS_100MS ((UINT8)1)
#define LEDBAR_ANIM_STEP_TICKS_100MS ((UINT8)1)
#define LEDBAR_SHORT_BLOCK_AFTER_SEQUENCE_TICKS_100MS ((UINT8)10)
#define LEDBAR_PREBOOT_POWERON_TICKS_10MS ((UINT16)300)
#define LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS ((UINT16)300)
#define LEDBAR_PREBOOT_RELEASE_TICKS_10MS ((UINT16)10)
#define LEDBAR_PREBOOT_WAKE_TICKS_10MS ((UINT16)5)

typedef enum _LEDBAR_UI_MODE
{
    LED_UI_NORMAL = 0,
    LED_UI_SHORT_SHOW,
    LED_UI_WAKE_PREVIEW_SHOW,
    LED_UI_BOOT_DELAY,
    LED_UI_BOOT_ANIM_ON,
    LED_UI_BOOT_ANIM_OFF,
    LED_UI_BOOT_POST_SHOW,
    LED_UI_SHUTDOWN_ANIM
} LEDBAR_UI_MODE;

static LEDBAR_UI_MODE s_led_ui_mode = LED_UI_NORMAL;
static UINT8 s_anim_step = 0;
static UINT8 s_anim_step_ticks = 0;
static UINT8 s_ui_ticks = 0;
static UINT8 s_pending_shutdown_sleep = 0;
static UINT16 s_cached_soc = 0;

static UINT8 s_key_press_ticks = 0;
static UINT8 s_key_prev_pressed = 0;
static UINT8 s_key_long_handled = 0;
static UINT8 s_key_wait_release = 0;
static UINT8 s_key_stable_pressed = 0;
static UINT8 s_key_debounce_ticks = 0;
static UINT8 s_short_press_block_ticks = 0;
static UINT8 s_ignore_next_release_short = 0;
static UINT8 s_wake_preview_hold_ticks = 0;

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
    if (soc_clamped > 20)
    {
        mask |= 0x02;
    }
    if (soc_clamped > 40)
    {
        mask |= 0x04;
    }
    if (soc_clamped > 60)
    {
        mask |= 0x08;
    }
    if (soc_clamped > 80)
    {
        mask |= 0x10;
    }

    return mask;
}

static UINT8 LedBar_GetLiveSocMask(void)
{
    return LedBar_GetSocMaskFromValue(g_stCellInfoReport.SocElement.u16Soc);
}

static UINT8 LedBar_GetCachedSocMask(void)
{
    return LedBar_GetSocMaskFromValue(s_cached_soc);
}

static void LedBar_SetByMask(UINT8 mask)
{
    MCUO_SOC_20 = (mask & 0x01) ? 1 : 0;
    MCUO_SOC_40 = (mask & 0x02) ? 1 : 0;
    MCUO_SOC_60 = (mask & 0x04) ? 1 : 0;
    MCUO_SOC_80 = (mask & 0x08) ? 1 : 0;
    MCUO_SOC_100 = (mask & 0x10) ? 1 : 0;
    MCUO_SOC_RUN = (mask != 0) ? 1 : 0;
}

static void LedBar_SetAllOff(void)
{
    LedBar_SetByMask(0);
}

static UINT8 LedBar_IsPowerOn(void)
{
    return (UINT8)System_OnOFF_Func.bits.b1OnOFF_MOS_Relay;
}

static UINT8 LedBar_IsKeyPressed(void)
{
    return (UINT8)(MCUI_ENI_DI1 == 0);
}

UINT8 LedBar_IsWakePreviewPending(void)
{
    /* 保留旧接口，避免再通过该标志改动系统开机功能位。 */
    return 0;
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

static void LedBar_StartShortShow(void)
{
    s_led_ui_mode = LED_UI_SHORT_SHOW;
    s_ui_ticks = 0;
}

static void LedBar_StartWakePreview(void)
{
    s_led_ui_mode = LED_UI_WAKE_PREVIEW_SHOW;
    s_ui_ticks = 0;
    s_wake_preview_hold_ticks = 0;
    s_key_wait_release = 1;
    s_ignore_next_release_short = 1;
    LedBar_SetByMask(LedBar_GetCachedSocMask());
}

static void LedBar_StartBootDelay(void)
{
    s_led_ui_mode = LED_UI_BOOT_DELAY;
    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_pending_shutdown_sleep = 0;
    s_key_wait_release = 1;
    s_key_long_handled = 1;
    s_ignore_next_release_short = 1;
    LedBar_SetAllOff();
}

static void LedBar_StartPowerOnAnim(void)
{
    s_led_ui_mode = LED_UI_BOOT_ANIM_ON;
    s_anim_step = 1;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_pending_shutdown_sleep = 0;
    s_key_wait_release = 1;
    s_key_long_handled = 1;
    s_ignore_next_release_short = 1;
    LedBar_SetByMask(0x01);
}

static void LedBar_StartShutdownAnim(void)
{
    s_led_ui_mode = LED_UI_SHUTDOWN_ANIM;
    s_anim_step = 0;
    s_anim_step_ticks = 0;
    s_ui_ticks = 0;
    s_pending_shutdown_sleep = 1;
    s_ignore_next_release_short = 1;
    LedBar_SetByMask(LEDBAR_MASK_ALL);
}

static void LedBar_ProcessKeyEvent(void)
{
    UINT8 key_pressed_raw;
    UINT8 key_pressed;

    if (s_short_press_block_ticks > 0)
    {
        --s_short_press_block_ticks;
    }

    key_pressed_raw = LedBar_IsKeyPressed();
    if (key_pressed_raw != s_key_stable_pressed)
    {
        if (++s_key_debounce_ticks >= LEDBAR_KEY_DEBOUNCE_TICKS_100MS)
        {
            s_key_stable_pressed = key_pressed_raw;
            s_key_debounce_ticks = 0;
        }
    }
    else
    {
        s_key_debounce_ticks = 0;
    }

    key_pressed = s_key_stable_pressed;

    if (key_pressed)
    {
        if (!s_key_prev_pressed)
        {
            s_key_press_ticks = 0;
            s_key_long_handled = 0;
        }

        if (s_key_press_ticks < 0xFF)
        {
            ++s_key_press_ticks;
        }

        if (LedBar_IsPowerOn() &&
            s_led_ui_mode == LED_UI_NORMAL &&
            !s_key_wait_release &&
            !s_key_long_handled &&
            s_key_press_ticks >= LEDBAR_LONG_PRESS_TICKS_100MS)
        {
            s_key_long_handled = 1;
            s_key_wait_release = 1;
            s_ignore_next_release_short = 1;
            LedBar_StartShutdownAnim();
        }
    }
    else
    {
        if (s_key_prev_pressed)
        {
            if (LedBar_IsPowerOn() &&
                s_led_ui_mode == LED_UI_NORMAL &&
                !s_key_long_handled &&
                s_key_press_ticks > 0 &&
                s_key_press_ticks < LEDBAR_LONG_PRESS_TICKS_100MS &&
                !s_ignore_next_release_short &&
                s_short_press_block_ticks == 0)
            {
                LedBar_StartShortShow();
            }
        }

        s_key_press_ticks = 0;
        s_key_long_handled = 0;
        s_key_wait_release = 0;
        s_ignore_next_release_short = 0;
    }

    s_key_prev_pressed = key_pressed;
}

static void LedBar_RunShortShow(void)
{
    LedBar_SetByMask(LedBar_GetLiveSocMask());

    if (++s_ui_ticks < LEDBAR_SHORT_SHOW_TICKS_100MS)
    {
        return;
    }

    s_ui_ticks = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    LedBar_SetAllOff();
}
UINT8 LedBar_HandleWakePreviewBeforeBoot(void)
{
    UINT16 wake_mode = WAKE_DISPLAY_MODE_NONE;
    UINT16 wake_soc = 0;
    UINT16 hold_ticks = LEDBAR_PREBOOT_WAKE_TICKS_10MS;
    UINT16 preview_ticks = LEDBAR_PREBOOT_WAKE_TICKS_10MS;
    UINT16 release_ticks = 0;
    UINT8 soc_mask;

    LedBar_InitOutputPins();

    if (!WakeDisplayState_Read(&wake_mode, &wake_soc))
    {
        wake_soc = 0;
    }

    soc_mask = LedBar_GetSocMaskFromValue(wake_soc);
    LedBar_SetByMask(soc_mask);

    while (1)
    {
        __delay_ms(10);

        if (LedBar_IsKeyPressed() && is_open_gan1())
        {
            release_ticks = 0;
            if (hold_ticks < LEDBAR_PREBOOT_POWERON_TICKS_10MS)
            {
                ++hold_ticks;
            }
            if (preview_ticks < LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                ++preview_ticks;
            }

            if (hold_ticks >= LEDBAR_PREBOOT_POWERON_TICKS_10MS)
            {
                WakeDisplay_RequestBootSequence();
                LedBar_SetAllOff();
                return 1;
            }
        }
        else
        {
            if (preview_ticks < LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                ++preview_ticks;
            }

            if (release_ticks < LEDBAR_PREBOOT_RELEASE_TICKS_10MS)
            {
                ++release_ticks;
                continue;
            }

            if (preview_ticks >= LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                WakeDisplayState_Clear();
                WakeDisplaySocCache_Write(wake_soc);
                LedBar_SetAllOff();
                return 0;
            }
        }
    }
}

#if 0
UINT8 LedBar_HandleWakePreviewBeforeBoot(void)
{
    UINT16 wake_mode = WAKE_DISPLAY_MODE_NONE;
    UINT16 wake_soc = 0;
    UINT16 hold_ticks = LEDBAR_PREBOOT_WAKE_TICKS_10MS;
    UINT16 preview_ticks = LEDBAR_PREBOOT_WAKE_TICKS_10MS;
    UINT16 release_ticks = 0;
    UINT8 soc_mask;

    LedBar_InitOutputPins();

    if (!WakeDisplayState_Read(&wake_mode, &wake_soc))
    {
        wake_soc = 0;
    }

    soc_mask = LedBar_GetSocMaskFromValue(wake_soc);
    LedBar_SetByMask(soc_mask);

    while (1)
    {
        __delay_ms(10);

        if (LedBar_IsKeyPressed())
        {
            release_ticks = 0;
            if (hold_ticks < LEDBAR_PREBOOT_POWERON_TICKS_10MS)
            {
                ++hold_ticks;
            }
            if (preview_ticks < LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                ++preview_ticks;
            }

            if (hold_ticks >= LEDBAR_PREBOOT_POWERON_TICKS_10MS)
            {
                WakeDisplay_RequestBootSequence();
                LedBar_SetAllOff();
                return 1;
            }
        }
        else
        {
            if (preview_ticks < LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                ++preview_ticks;
            }

            if (release_ticks < LEDBAR_PREBOOT_RELEASE_TICKS_10MS)
            {
                ++release_ticks;
                continue;
            }

            if (preview_ticks >= LEDBAR_PREBOOT_SOC_SHOW_TICKS_10MS)
            {
                WakeDisplayState_Clear();
                WakeDisplaySocCache_Write(wake_soc);
                LedBar_SetAllOff();
                return 0;
            }
        }
    }
}
#endif

static void LedBar_RunWakePreviewShow(void)
{
    s_led_ui_mode = LED_UI_NORMAL;
    s_wake_preview_hold_ticks = 0;
    LedBar_SetAllOff();
}

static void LedBar_RunBootDelay(void)
{
    LedBar_SetAllOff();

    if (++s_ui_ticks < LEDBAR_BOOT_DELAY_TICKS_100MS)
    {
        return;
    }

    LedBar_StartPowerOnAnim();
}

void LedBar_RunBootAnimOn_test(void)
{
    uint8_t i = 0;
    s_anim_step_ticks = 0;
    s_anim_step = 0;

    for (i = 0; i < 5; i++)
    {
        if (s_anim_step < 5)
        {
            ++s_anim_step;
            LedBar_SetByMask((UINT8)((1U << s_anim_step) - 1U));
        }
        __delay_ms(150);
    }

    s_anim_step_ticks = 0;
    s_anim_step = 0;
    for (i = 0; i < 5; i++)
    {
        if (s_anim_step < 5)
        {
            ++s_anim_step;
            LedBar_SetByMask((UINT8)(LEDBAR_MASK_ALL >> s_anim_step));
            __delay_ms(150);
        }
    }

    s_anim_step = 0;
    s_led_ui_mode = LED_UI_BOOT_ANIM_OFF;

    // LedBar_SetByMask(LedBar_GetLiveSocMask());
    LedBar_SetByMask(7);
    __delay_ms(200);

    s_ui_ticks = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    LedBar_SetAllOff();
}

static void LedBar_RunBootAnimOn(void)
{
#if 0
    if (++s_anim_step_ticks < LEDBAR_ANIM_STEP_TICKS_100MS)
    {
        return;
    }
    s_anim_step_ticks = 0;

    if (s_anim_step < 5)
    {
        ++s_anim_step;
        LedBar_SetByMask((UINT8)((1U << s_anim_step) - 1U));
    }

    if (s_anim_step >= 5)
    {
        s_anim_step = 0;
        s_led_ui_mode = LED_UI_BOOT_ANIM_OFF;
    }
#endif
}

static void LedBar_RunBootAnimOff(void)
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
    }

    if (s_anim_step >= 5)
    {
        s_anim_step = 0;
        s_ui_ticks = 0;
        s_led_ui_mode = LED_UI_BOOT_POST_SHOW;
        LedBar_SetByMask(LedBar_GetCachedSocMask());
    }
}

static void LedBar_RunBootPostShow(void)
{
    LedBar_SetByMask(LedBar_GetCachedSocMask());

    if (++s_ui_ticks < LEDBAR_BOOT_POST_SHOW_TICKS_100MS)
    {
        return;
    }

    s_ui_ticks = 0;
    s_led_ui_mode = LED_UI_NORMAL;
    s_short_press_block_ticks = LEDBAR_SHORT_BLOCK_AFTER_SEQUENCE_TICKS_100MS;
    LedBar_SetAllOff();
}

void LedBar_RunShutdownAnim_test(void)
{
    uint8_t i = 0;
    s_anim_step_ticks = 0;
    s_anim_step = 0;

    for (i = 0; i < 5; i++)
    {
        if (s_anim_step < 5)
        {
            ++s_anim_step;
            LedBar_SetByMask((UINT8)(LEDBAR_MASK_ALL >> s_anim_step));
            __delay_ms(150);
        }
    }

    LedBar_SetAllOff();
}

static void LedBar_RunShutdownAnim(void)
{
#if 0
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
    LedBar_SetAllOff();
    if (s_pending_shutdown_sleep)
    {
        s_pending_shutdown_sleep = 0;
        Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
    }
#endif
    Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
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
    s_pending_shutdown_sleep = 0;
    s_cached_soc = 0;
    s_key_press_ticks = 0;
    s_key_prev_pressed = 0;
    s_key_long_handled = 0;
    s_key_wait_release = 0;
    s_key_stable_pressed = 0;
    s_key_debounce_ticks = 0;
    s_short_press_block_ticks = 0;
    s_ignore_next_release_short = 0;
    s_wake_preview_hold_ticks = 0;

    if (WakeDisplayState_Read(&wake_mode, &wake_soc))
    {
        s_cached_soc = wake_soc;
    }
    else
    {
        s_cached_soc = g_stCellInfoReport.SocElement.u16Soc;
    }

    if (wake_mode == WAKE_DISPLAY_MODE_BOOT_SEQUENCE ||
        (wake_mode == WAKE_DISPLAY_MODE_NONE && LedBar_IsKeyPressed()))
    {
        s_key_stable_pressed = LedBar_IsKeyPressed();
        s_key_prev_pressed = s_key_stable_pressed;
        LedBar_StartBootDelay();
    }
    else
    {
        LedBar_SetAllOff();
    }

    WakeDisplayState_Clear();
}

void LedBar_Show_Normal(void)
{
    if (g_stCellInfoReport.u16Ichg)
    {
        LedBar_Command = LED_BAR_CHG;
        return;
    }

    if (g_stCellInfoReport.u16IDischg)
    {
        LedBar_Command = LED_BAR_DSG;
        return;
    }

    LedBar_SetAllOff();
}

void LedBar_Show_CHG(void)
{
    static UINT8 su8_temp = 0;
    static UINT16 su16_ShowDelay = 0;

    if (++su16_ShowDelay <= 5)
    {
        su8_temp = ~(1 << (g_stCellInfoReport.SocElement.u16Soc / 20));
    }
    else if (++su16_ShowDelay <= 10)
    {
        su8_temp = 0xFF;
    }
    else
    {
        su16_ShowDelay = 0;
    }

    MCUO_SOC_RUN = 1;
    MCUO_SOC_20 = (su8_temp & 0x01) ? 1 : 0;
    MCUO_SOC_40 = (g_stCellInfoReport.SocElement.u16Soc >= 20 ? 1 : 0) && (su8_temp & 0x02);
    MCUO_SOC_60 = (g_stCellInfoReport.SocElement.u16Soc >= 40 ? 1 : 0) && (su8_temp & 0x04);
    MCUO_SOC_80 = (g_stCellInfoReport.SocElement.u16Soc >= 60 ? 1 : 0) && (su8_temp & 0x08);
    MCUO_SOC_100 = (g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0) && (su8_temp & 0x10);

    if (g_stCellInfoReport.u16Ichg == 0)
    {
        LedBar_Command = LED_BAR_NORMAL;
    }
}

void LedBar_Show_DSG(void)
{
    MCUO_SOC_RUN = 1;
    MCUO_SOC_20 = g_stCellInfoReport.SocElement.u16Soc > 0 ? 1 : 0;
    MCUO_SOC_40 = g_stCellInfoReport.SocElement.u16Soc >= 20 ? 1 : 0;
    MCUO_SOC_60 = g_stCellInfoReport.SocElement.u16Soc >= 40 ? 1 : 0;
    MCUO_SOC_80 = g_stCellInfoReport.SocElement.u16Soc >= 60 ? 1 : 0;
    MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0;

    if (g_stCellInfoReport.u16IDischg == 0)
    {
        MCUO_SOC_RUN = 0;
        MCUO_SOC_20 = 0;
        MCUO_SOC_40 = 0;
        MCUO_SOC_60 = 0;
        MCUO_SOC_80 = 0;
        MCUO_SOC_100 = 0;

        LedBar_Command = LED_BAR_NORMAL;
    }
}

void LedBar_Show_Fault(void)
{
    if (g_stCellInfoReport.unMdlFault_Third.all & 0x2FFA || System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) || System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
    {
        MCUO_SOC_ALARM = !MCUO_SOC_ALARM;
        MCUO_SOC_40 = 0;
        MCUO_SOC_60 = 0;
        MCUO_SOC_80 = 0;
        MCUO_SOC_100 = 0;
    }
}

void LedBar_Show_Sleep(void)
{
    static UINT16 su16_SleepDelay_Tcnt = 0;

    if (!MCUI_SOC_KEY)
    {
        if (++su16_SleepDelay_Tcnt >= 30)
        {
            Sleep_Mode.bits.b1ForceToSleep_L2 = 1;
        }
    }
    else
    {
        su16_SleepDelay_Tcnt = 0;
    }
}

void APP_LedBar(void)
{
    static bool first_reset_status = true;

    if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
    {
        return;
    }

    if (SystemStatus.bits.b1StartUpBMS)
    {
        return;
    }

    LedBar_ProcessKeyEvent();

    if (is_water_in())
    {
        if (s_led_ui_mode == LED_UI_SHUTDOWN_ANIM)
        {
            LedBar_RunShutdownAnim();
            return;
        }

        if (first_reset_status)
        {
            first_reset_status = false;
            MCUO_SOC_20 = 0;
            MCUO_SOC_40 = 0;
            MCUO_SOC_60 = 0;
            MCUO_SOC_80 = 0;
            MCUO_SOC_100 = 0;
        }

        MCUO_SOC_20 = !MCUO_SOC_20;
        MCUO_SOC_40 = !MCUO_SOC_40;
        MCUO_SOC_60 = !MCUO_SOC_60;
        MCUO_SOC_80 = !MCUO_SOC_80;
        MCUO_SOC_100 = !MCUO_SOC_100;
    }
    else
    {
        first_reset_status = true;

        switch (s_led_ui_mode)
        {
        case LED_UI_SHORT_SHOW:
            LedBar_RunShortShow();
            break;

        case LED_UI_WAKE_PREVIEW_SHOW:
            LedBar_RunWakePreviewShow();
            break;

        case LED_UI_BOOT_DELAY:
            LedBar_RunBootDelay();
            break;

        case LED_UI_BOOT_ANIM_ON:
            LedBar_RunBootAnimOn();
            break;

        case LED_UI_BOOT_ANIM_OFF:
            LedBar_RunBootAnimOff();
            break;

        case LED_UI_BOOT_POST_SHOW:
            LedBar_RunBootPostShow();
            break;

        case LED_UI_SHUTDOWN_ANIM:
            LedBar_RunShutdownAnim();
            break;

        case LED_UI_NORMAL:
            switch (LedBar_Command)
            {
            case LED_BAR_NORMAL:
                LedBar_Show_Normal();
                break;
            case LED_BAR_CHG:
                LedBar_Show_CHG();
                break;
            case LED_BAR_DSG:
                LedBar_Show_DSG();
                break;
            case LED_BAR_FAULT:
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }

    if (s_led_ui_mode == LED_UI_NORMAL)
    {
        LedBar_Show_Fault();
    }
}
