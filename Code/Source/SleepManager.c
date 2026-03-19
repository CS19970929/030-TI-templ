/**
 * @file SleepManager.c
 * @brief 精简的休眠管理模块实现（仅基于电压、电流、通信）
 */

#include "SleepManager.h"
#include "SleepDeal.h"  // 复用原有的IO配置和低功耗函数

// ==================== 全局变量 ====================
SleepManager_t g_sleepMgr = {0};

// 默认休眠配置
static const SleepConfig_t default_config = SLEEP_CONFIG_DEFAULT;

// ==================== 内部函数声明 ====================
static void Sleep_CheckVoltage(void);
static void Sleep_CheckCurrent(void);
static void Sleep_CheckCommunication(void);
static void Sleep_UpdateState(void);
static void Sleep_EnterNormal(void);
static void Sleep_EnterDeep(void);

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
    g_sleepMgr.state = SLEEP_STATE_IDLE;
    g_sleepMgr.mode = SLEEP_MODE_NONE;
    g_sleepMgr.trigger = SLEEP_TRIGGER_NONE;
    g_sleepMgr.is_sleeping = false;
    
    // 初始化唤醒源
    g_sleepMgr.wake_source_ext = false;
    g_sleepMgr.wake_source_rtc = false;
    
    // 配置唤醒引脚（复用原有函数）
    InitWakeUp_Base();
}

/**
 * @brief 休眠处理主函数（1s调用一次）
 */
void Sleep_Process(void)
{
    // 如果正在休眠中，不处理
    if (g_sleepMgr.is_sleeping) {
        return;
    }
    
    // 根据当前状态处理
    switch (g_sleepMgr.state) {
        case SLEEP_STATE_IDLE:
            // 检查是否需要进入休眠
            Sleep_CheckVoltage();
            Sleep_CheckCurrent();
            Sleep_CheckCommunication();
            Sleep_UpdateState();
            break;
            
        case SLEEP_STATE_CHECKING:
            // 持续检查条件，如果条件满足则准备休眠
            Sleep_CheckVoltage();
            Sleep_CheckCurrent();
            Sleep_CheckCommunication();
            Sleep_UpdateState();
            break;
            
        case SLEEP_STATE_PREPARING:
            // 执行休眠
            Sleep_EnterSleep();
            break;
            
        case SLEEP_STATE_WAKEUP:
            // 唤醒处理完成，回到空闲状态
            g_sleepMgr.state = SLEEP_STATE_IDLE;
            g_sleepMgr.mode = SLEEP_MODE_NONE;
            g_sleepMgr.trigger = SLEEP_TRIGGER_NONE;
            break;
            
        default:
            g_sleepMgr.state = SLEEP_STATE_IDLE;
            break;
    }
}

/**
 * @brief 检查电压条件
 */
static void Sleep_CheckVoltage(void)
{
    uint16_t vcell_min = g_stCellInfoReport.u16VCellMin;
    
    // 检查极低电压（快速进入深度休眠）
    if (vcell_min <= g_sleepMgr.config.voltage_very_low) {
        g_sleepMgr.cnt_voltage_very_low++;
        g_sleepMgr.cnt_voltage_low = 0;
        g_sleepMgr.cnt_voltage_normal = 0;
        
        // 极低电压达到阈值，直接深度休眠
        if (g_sleepMgr.cnt_voltage_very_low >= (g_sleepMgr.config.time_very_low * 60)) {
            g_sleepMgr.mode = SLEEP_MODE_DEEP;
            g_sleepMgr.trigger = SLEEP_TRIGGER_VOLTAGE_VERYLOW;
            g_sleepMgr.cnt_voltage_very_low = 0;
        }
    }
    // 检查低电压
    else if (vcell_min <= g_sleepMgr.config.voltage_low) {
        g_sleepMgr.cnt_voltage_low++;
        g_sleepMgr.cnt_voltage_very_low = 0;
        g_sleepMgr.cnt_voltage_normal = 0;
        
        // 低电压且无电流，达到阈值进入深度休眠
        if (!Sleep_HasCurrent() && 
            g_sleepMgr.cnt_voltage_low >= (g_sleepMgr.config.time_low * 60)) {
            g_sleepMgr.mode = SLEEP_MODE_DEEP;
            g_sleepMgr.trigger = SLEEP_TRIGGER_NO_CURRENT;
            g_sleepMgr.cnt_voltage_low = 0;
        }
    }
    // 检查正常电压（可进入普通休眠）
    else if (vcell_min <= g_sleepMgr.config.voltage_normal) {
        g_sleepMgr.cnt_voltage_normal++;
        g_sleepMgr.cnt_voltage_very_low = 0;
        g_sleepMgr.cnt_voltage_low = 0;
        
        // 正常电压阈值，无电流时可进入普通休眠
        if (!Sleep_HasCurrent() && 
            g_sleepMgr.cnt_voltage_normal >= (g_sleepMgr.config.time_normal * 60)) {
            g_sleepMgr.mode = SLEEP_MODE_NORMAL;
            g_sleepMgr.trigger = SLEEP_TRIGGER_VOLTAGE_LOW;
            g_sleepMgr.cnt_voltage_normal = 0;
        }
    }
    // 电压正常，清除计数器
    else {
        g_sleepMgr.cnt_voltage_very_low = 0;
        g_sleepMgr.cnt_voltage_low = 0;
        g_sleepMgr.cnt_voltage_normal = 0;
    }
}

/**
 * @brief 检查电流条件
 */
static void Sleep_CheckCurrent(void)
{
    // 过流保护触发，直接深度休眠
    if (Sleep_Mode.bits.b1OverCurSleep) {
        g_sleepMgr.mode = SLEEP_MODE_DEEP;
        g_sleepMgr.trigger = SLEEP_TRIGGER_OVERCURRENT;
        
        // 清除过流标志
        Sleep_Mode.bits.b1OverCurSleep = 0;
    }
    
    // 如果有电流，重置电压计数器（有活动不休眠）
    if (Sleep_HasCurrent()) {
        g_sleepMgr.cnt_voltage_very_low = 0;
        g_sleepMgr.cnt_voltage_low = 0;
        g_sleepMgr.cnt_voltage_normal = 0;
    }
}

/**
 * @brief 检查通信条件
 */
static void Sleep_CheckCommunication(void)
{
    static uint8_t last_ext_com_cnt = 0;
    
    // 检查外部通信活跃度
    if (RTC_ExtComCnt != last_ext_com_cnt) {
        last_ext_com_cnt = RTC_ExtComCnt;
        
        // 有外部通信，重置计数器
        g_sleepMgr.cnt_voltage_very_low = 0;
        g_sleepMgr.cnt_voltage_low = 0;
        g_sleepMgr.cnt_voltage_normal = 0;
    }
    
    // 检查AFE通信是否正常
    if (!Sleep_IsAfeOk()) {
        // AFE通信错误，进入深度休眠
        g_sleepMgr.mode = SLEEP_MODE_DEEP;
        g_sleepMgr.trigger = SLEEP_TRIGGER_AFE_ERROR;
    }
}

/**
 * @brief 更新休眠状态
 */
static void Sleep_UpdateState(void)
{
    // 如果已经设置了休眠模式，进入准备状态
    if (g_sleepMgr.mode != SLEEP_MODE_NONE) {
        g_sleepMgr.state = SLEEP_STATE_PREPARING;
    }
}

/**
 * @brief 执行休眠操作
 */
void Sleep_EnterSleep(void)
{
    if (g_sleepMgr.mode == SLEEP_MODE_NONE) {
        return;
    }
    
    // 设置状态和标志
    g_sleepMgr.is_sleeping = true;
    g_sleepMgr.state = SLEEP_STATE_SLEEPING;
    g_sleepMgr.sleep_count++;
    
    // 记录休眠日志
    LogRecord_Flag.bits.Log_Sleep = 1;
    LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
    
    // 根据模式执行休眠
    switch (g_sleepMgr.mode) {
        case SLEEP_MODE_NORMAL:
            Sleep_EnterNormal();
            break;
            
        case SLEEP_MODE_DEEP:
            Sleep_EnterDeep();
            break;
            
        default:
            break;
    }
}

/**
 * @brief 进入普通休眠（STOP模式）
 */
static void Sleep_EnterNormal(void)
{
    // 写入Flash标志
    BootFlag_Write(FLASH_NORMAL_SLEEP_VALUE);
    
    // 关闭AFE和蓝牙
    App_AFEshutdown();
    lk8625_SendAT("AT+DISCON");
    lk8625_SendAT("AT+DSLEEP");
    
    // 配置IO和唤醒源
    IOstatus_NormalMode();
    InitWakeUp_NormalMode();
    
    // 进入STOP模式
    Sys_StopMode();
    
    // 唤醒后恢复
    IORecover_NormalMode();
    BootFlag_Clear();
}

/**
 * @brief 进入深度休眠（STANDBY模式）
 */
static void Sleep_EnterDeep(void)
{
    // 写入Flash标志
    BootFlag_Write(FLASH_DEEP_SLEEP_VALUE);
    
    // 关闭AFE和蓝牙
    App_AFEshutdown();
    lk8625_SendAT("AT+DISCON");
    lk8625_SendAT("AT+DSLEEP");
    
    // 配置IO和唤醒源
    IOstatus_DeepMode();
    InitWakeUp_DeepMode();
    
    // 复位进入深度休眠（STANDBY模式通过复位实现）
    MCU_RESET();
}

/**
 * @brief 唤醒处理
 */
void Sleep_WakeupHandler(uint8_t wake_source)
{
    // 记录唤醒源
    if (wake_source == 0) {
        g_sleepMgr.wake_source_ext = true;
    } else {
        g_sleepMgr.wake_source_rtc = true;
    }
    
    // 更新统计
    g_sleepMgr.wakeup_count++;
    g_sleepMgr.is_sleeping = false;
    
    // 清除休眠相关标志
    Sleep_Mode.all = 0;
    Sleep_Status = SLEEP_HICCUP_SHIFT;
    
    // 清除Flash标志
    BootFlag_Clear();
    
    // 进入唤醒处理状态
    g_sleepMgr.state = SLEEP_STATE_WAKEUP;
}

/**
 * @brief 检查是否有电流
 */
bool Sleep_HasCurrent(void)
{
    // 检查充电电流
    if (g_stCellInfoReport.u16Ichg > g_sleepMgr.config.current_threshold) {
        return true;
    }
    
    // 检查放电电流
    if (g_stCellInfoReport.u16IDischg > g_sleepMgr.config.current_threshold) {
        return true;
    }
    
    return false;
}

/**
 * @brief 检查AFE通信是否正常
 */
bool Sleep_IsAfeOk(void)
{
    // 检查AFE通信错误标志
    if (System_Error_UserCallback(ERROR_STATUS_AFE1)) {
        return false;
    }
    
    if (System_Error_UserCallback(ERROR_STATUS_AFE2)) {
        return false;
    }
    
    return true;
}

/**
 * @brief 获取休眠状态
 */
SleepState_e Sleep_GetState(void)
{
    return g_sleepMgr.state;
}

/**
 * @brief 获取休眠模式
 */
SleepMode_e Sleep_GetMode(void)
{
    return g_sleepMgr.mode;
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
 * @brief 获取休眠统计
 */
void Sleep_GetStats(uint32_t *sleep_count, uint32_t *wakeup_count)
{
    if (sleep_count != NULL) {
        *sleep_count = g_sleepMgr.sleep_count;
    }
    if (wakeup_count != NULL) {
        *wakeup_count = g_sleepMgr.wakeup_count;
    }
}

/**
 * @brief 清除休眠统计
 */
void Sleep_ClearStats(void)
{
    g_sleepMgr.sleep_count = 0;
    g_sleepMgr.wakeup_count = 0;
}