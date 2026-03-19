/**
 * @file SleepManager_LowPower.c
 * @brief 增强的低功耗管理模块实现
 * @note 基于分层休眠+周期性唤醒+自适应策略
 */

#include "SleepManager_LowPower.h"

// ==================== 全局变量 ====================
SleepManager_t g_sleepMgr;

// 默认配置
static const SleepConfig_t default_config = SLEEP_CONFIG_DEFAULT;

// ==================== 内部函数声明 ====================
static void Sleep_CheckActivity(void);
static void Sleep_UpdateState(void);
static void Sleep_EnterLightSleep(void);
static void Sleep_EnterMediumSleep(void);
static void Sleep_EnterDeepSleep(void);
static void Sleep_ExitSleep(void);

// ==================== 函数实现 ====================

/**
 * @brief 初始化休眠管理器
 */
void Sleep_Init(const SleepConfig_t *config)
{
    // 清零结构体
    memset(&g_sleepMgr, 0, sizeof(SleepManager_t));
    
    // 设置配置
    if (config != NULL) {
        g_sleepMgr.config = *config;
    } else {
        g_sleepMgr.config = default_config;
    }
    
    // 初始化状态
    g_sleepMgr.state = SLEEP_STATE_ACTIVE;
    g_sleepMgr.mode = SLEEP_MODE_NONE;
    g_sleepMgr.trigger = SLEEP_TRIGGER_NONE;
    g_sleepMgr.wake_source = WAKE_SOURCE_NONE;
    g_sleepMgr.is_sleeping = FALSE;
    g_sleepMgr.is_first_sleep = TRUE;
    
    // 初始化周期性唤醒
    g_sleepMgr.enable_periodic_wakeup = TRUE;
    g_sleepMgr.periodic_interval = g_sleepMgr.config.light_sleep_wakeup_time;
    g_sleepMgr.periodic_counter = 0;
    
    // 初始化自适应休眠
    g_sleepMgr.enable_adaptive_sleep = TRUE;
    g_sleepMgr.current_soc = 50;
    
    // 配置唤醒引脚
    InitWakeUp_Base();
}

/**
 * @brief 休眠处理主函数（1秒调用一次）
 */
void Sleep_Process(void)
{
    // 如果正在休眠中，不处理
    if (g_sleepMgr.is_sleeping) {
        return;
    }
    
    // 更新空闲计数器
    g_sleepMgr.idle_counter++;
    
    // 检查活动状态
    Sleep_CheckActivity();
    
    // 更新状态机
    Sleep_UpdateState();
    
    // 更新周期性唤醒计数器
    if (g_sleepMgr.enable_periodic_wakeup) {
        g_sleepMgr.periodic_counter++;
    }
    
    // 更新休眠计时器
    if (g_sleepMgr.sleep_timer > 0) {
        g_sleepMgr.sleep_timer--;
    }
}

/**
 * @brief 检查活动状态
 */
static void Sleep_CheckActivity(void)
{
    static UINT8 last_ext_com_cnt = 0;
    
    // 检查是否有电流
    if (Sleep_HasCurrent()) {
        g_sleepMgr.idle_counter = 0;
        g_sleepMgr.state = SLEEP_STATE_ACTIVE;
        return;
    }
    
    // 检查外部通信
    if (RTC_ExtComCnt != last_ext_com_cnt) {
        last_ext_com_cnt = RTC_ExtComCnt;
        g_sleepMgr.idle_counter = 0;
        g_sleepMgr.state = SLEEP_STATE_ACTIVE;
        return;
    }
    
    // 检查过流保护
    if (Sleep_Mode.bits.b1OverCurSleep) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_OVERCURRENT;
        Sleep_EnterSleep(SLEEP_MODE_DEEP);
        Sleep_Mode.bits.b1OverCurSleep = 0;
        return;
    }
    
    // 检查AFE通信
    if (!Sleep_IsAfeOk()) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_AFE_ERROR;
        Sleep_EnterSleep(SLEEP_MODE_DEEP);
        return;
    }
    
    // 检查电压
    if (g_stCellInfoReport.u16VCellMin <= g_sleepMgr.config.voltage_very_low) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_VOLTAGE_VERYLOW;
        Sleep_EnterSleep(SLEEP_MODE_DEEP);
        return;
    }
}

/**
 * @brief 更新状态机
 */
static void Sleep_UpdateState(void)
{
    // 计算自适应休眠时间
    UINT32 light_idle_time = Sleep_GetAdaptiveTime(g_sleepMgr.config.light_sleep_idle_time);
    UINT32 medium_idle_time = Sleep_GetAdaptiveTime(g_sleepMgr.config.medium_sleep_idle_time);
    UINT32 deep_idle_time = Sleep_GetAdaptiveTime(g_sleepMgr.config.deep_sleep_idle_time);
    
    // 根据空闲时间选择休眠模式
    if (g_sleepMgr.idle_counter >= deep_idle_time) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_NO_ACTIVITY;
        Sleep_EnterSleep(SLEEP_MODE_DEEP);
    } else if (g_sleepMgr.idle_counter >= medium_idle_time) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_NO_ACTIVITY;
        Sleep_EnterSleep(SLEEP_MODE_MEDIUM);
    } else if (g_sleepMgr.idle_counter >= light_idle_time) {
        g_sleepMgr.trigger = SLEEP_TRIGGER_NO_ACTIVITY;
        Sleep_EnterSleep(SLEEP_MODE_LIGHT);
    }
}

/**
 * @brief 进入休眠
 */
void Sleep_EnterSleep(SleepMode_e mode)
{
    // 检查是否首次休眠
    if (g_sleepMgr.is_first_sleep) {
        g_sleepMgr.is_first_sleep = FALSE;
    }
    
    // 设置状态和标志
    g_sleepMgr.is_sleeping = TRUE;
    g_sleepMgr.mode = mode;
    g_sleepMgr.stats.total_sleep_count++;
    
    // 记录休眠日志
    LogRecord_Flag.bits.Log_Sleep = 1;
    
    // 根据模式执行休眠
    switch (mode) {
        case SLEEP_MODE_LIGHT:
            g_sleepMgr.state = SLEEP_STATE_LIGHT_SLEEP;
            Sleep_EnterLightSleep();
            break;
            
        case SLEEP_MODE_MEDIUM:
            g_sleepMgr.state = SLEEP_STATE_MEDIUM_SLEEP;
            Sleep_EnterMediumSleep();
            break;
            
        case SLEEP_MODE_DEEP:
            g_sleepMgr.state = SLEEP_STATE_DEEP_SLEEP;
            Sleep_EnterDeepSleep();
            break;
            
        default:
            g_sleepMgr.is_sleeping = FALSE;
            break;
    }
}

/**
 * @brief 进入浅休眠（STOP模式，快速响应）
 */
static void Sleep_EnterLightSleep(void)
{
    // 关闭不必要的外设，保留快速唤醒能力
    IOstatus_NormalMode();
    InitWakeUp_NormalMode();
    
    // 进入STOP模式
    Sys_StopMode();
    
    // 唤醒后恢复
    Sleep_ExitSleep();
    g_sleepMgr.stats.light_sleep_count++;
}

/**
 * @brief 进入中休眠（STOP模式，中等响应）
 */
static void Sleep_EnterMediumSleep(void)
{
    // 写入Flash标志
    BootFlag_Write(FLASH_NORMAL_SLEEP_VALUE);
    
    // 关闭更多外设
    App_AFEshutdown();
    lk8625_SendAT("AT+DISCON");
    lk8625_SendAT("AT+DSLEEP");
    
    // 配置IO和唤醒源
    IOstatus_NormalMode();
    InitWakeUp_NormalMode();
    
    // 进入STOP模式
    Sys_StopMode();
    
    // 唤醒后恢复
    Sleep_ExitSleep();
    g_sleepMgr.stats.medium_sleep_count++;
}

/**
 * @brief 进入深休眠（STANDBY模式，最省电）
 */
static void Sleep_EnterDeepSleep(void)
{
    // 写入Flash标志
    BootFlag_Write(FLASH_DEEP_SLEEP_VALUE);
    
    // 关闭所有外设
    App_AFEshutdown();
    lk8625_SendAT("AT+DISCON");
    lk8625_SendAT("AT+DSLEEP");
    
    // 配置IO和唤醒源
    IOstatus_DeepMode();
    InitWakeUp_DeepMode();
    
    // 复位进入深休眠
    MCU_RESET();
    
    // 更新统计（实际上不会执行到这里）
    g_sleepMgr.stats.deep_sleep_count++;
}

/**
 * @brief 退出休眠处理
 */
static void Sleep_ExitSleep(void)
{
    // 清除休眠相关标志
    g_sleepMgr.is_sleeping = FALSE;
    g_sleepMgr.state = SLEEP_STATE_WAKEUP;
    
    // 恢复IO状态
    IORecover_NormalMode();
    
    // 清除Flash标志
    BootFlag_Clear();
    
    // 更新统计
    g_sleepMgr.stats.wakeup_count++;
    
    // 重置空闲计数器
    g_sleepMgr.idle_counter = 0;
}

/**
 * @brief 唤醒处理
 */
void Sleep_WakeupHandler(WakeSource_e source)
{
    // 记录唤醒源
    g_sleepMgr.wake_source = source;
    
    // 更新统计
    switch (source) {
        case WAKE_SOURCE_RTC:
            g_sleepMgr.stats.rtc_wakeup_count++;
            break;
        case WAKE_SOURCE_CURRENT:
            g_sleepMgr.stats.current_wakeup_count++;
            break;
        case WAKE_SOURCE_FAULT:
            g_sleepMgr.stats.fault_wakeup_count++;
            break;
        default:
            break;
    }
    
    // 退出休眠
    Sleep_ExitSleep();
}

/**
 * @brief 周期性唤醒检查（在RTC中断中调用）
 */
void Sleep_PeriodicWakeupCheck(void)
{
    // 更新周期性唤醒计数器
    g_sleepMgr.periodic_counter++;
    
    // 检查是否需要继续休眠
    BOOL should_continue_sleep = TRUE;
    
    // 检查是否有电流
    if (Sleep_HasCurrent()) {
        should_continue_sleep = FALSE;
    }
    
    // 检查电压
    if (g_stCellInfoReport.u16VCellMin <= g_sleepMgr.config.voltage_very_low) {
        should_continue_sleep = FALSE;
    }
    
    // 检查故障
    if (g_sleepMgr.has_pending_fault) {
        should_continue_sleep = FALSE;
    }
    
    // 如果需要退出休眠
    if (!should_continue_sleep) {
        Sleep_WakeupHandler(WAKE_SOURCE_RTC);
    }
    
    // 否则继续休眠，更新休眠计时
    g_sleepMgr.sleep_timer = g_sleepMgr.periodic_interval;
}

/**
 * @brief 获取自适应休眠时间
 */
UINT32 Sleep_GetAdaptiveTime(UINT32 base_time)
{
    if (!g_sleepMgr.enable_adaptive_sleep) {
        return base_time;
    }
    
    float factor = g_sleepMgr.config.sleep_time_factor_normal;
    
    if (g_sleepMgr.current_soc >= g_sleepMgr.config.soc_high_threshold) {
        factor = g_sleepMgr.config.sleep_time_factor_high;
    } else if (g_sleepMgr.current_soc <= g_sleepMgr.config.soc_low_threshold) {
        factor = g_sleepMgr.config.sleep_time_factor_low;
    }
    
    return (UINT32)(base_time * factor);
}

/**
 * @brief 检查是否有电流
 */
BOOL Sleep_HasCurrent(void)
{
    // 检查充电电流
    if (g_stCellInfoReport.u16Ichg > g_sleepMgr.config.current_threshold) {
        return TRUE;
    }
    
    // 检查放电电流
    if (g_stCellInfoReport.u16IDischg > g_sleepMgr.config.current_threshold) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief 检查AFE通信是否正常
 */
BOOL Sleep_IsAfeOk(void)
{
    // 检查AFE通信错误标志
    if (System_Error_UserCallback(ERROR_STATUS_AFE1)) {
        return FALSE;
    }
    
    if (System_Error_UserCallback(ERROR_STATUS_AFE2)) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief 获取当前状态
 */
SleepState_e Sleep_GetState(void)
{
    return g_sleepMgr.state;
}

/**
 * @brief 获取当前休眠模式
 */
SleepMode_e Sleep_GetMode(void)
{
    return g_sleepMgr.mode;
}

/**
 * @brief 获取唤醒源
 */
WakeSource_e Sleep_GetWakeSource(void)
{
    return g_sleepMgr.wake_source;
}

/**
 * @brief 设置休眠配置
 */
void Sleep_SetConfig(const SleepConfig_t *config)
{
    if (config != NULL) {
        g_sleepMgr.config = *config;
    }
}

/**
 * @brief 获取休眠配置
 */
const SleepConfig_t* Sleep_GetConfig(void)
{
    return &g_sleepMgr.config;
}

/**
 * @brief 获取统计信息
 */
const SleepStats_t* Sleep_GetStats(void)
{
    return &g_sleepMgr.stats;
}

/**
 * @brief 清除统计
 */
void Sleep_ClearStats(void)
{
    memset(&g_sleepMgr.stats, 0, sizeof(SleepStats_t));
}

/**
 * @brief 设置SOC值
 */
void Sleep_SetSOC(UINT16 soc)
{
    g_sleepMgr.current_soc = soc;
}

/**
 * @brief 启用/禁用周期性唤醒
 */
void Sleep_EnablePeriodicWakeup(BOOL enable)
{
    g_sleepMgr.enable_periodic_wakeup = enable;
}

/**
 * @brief 设置周期性唤醒间隔
 */
void Sleep_SetPeriodicInterval(UINT32 interval)
{
    g_sleepMgr.periodic_interval = interval;
}

/**
 * @brief 启用/禁用自适应休眠
 */
void Sleep_EnableAdaptiveSleep(BOOL enable)
{
    g_sleepMgr.enable_adaptive_sleep = enable;
}