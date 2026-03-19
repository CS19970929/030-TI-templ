#include "main.h"
#include "SleepDeal_Optimized.h"

// 优化后的休眠管理器
static SleepManager_t sleep_manager;

// 休眠条件检查函数指针表
typedef bool (*SleepConditionCheckFunc)(void);

// 休眠条件检查函数
static bool Check_OverCurrent(void) {
    return (Sleep_Mode.bits.b1OverCurSleep != 0);
}

static bool Check_OverVdelta(void) {
    return (Sleep_Mode.bits.b1OverVdeltaSleep != 0);
}

static bool Check_CBC(void) {
    return (Sleep_Mode.bits.b1CBCSleep != 0);
}

static bool Check_Forced(void) {
    return (Sleep_Mode.bits.b1ForceToSleep_L1 || 
            Sleep_Mode.bits.b1ForceToSleep_L2 || 
            Sleep_Mode.bits.b1ForceToSleep_L3);
}

static bool Check_VcellOVP(void) {
    return (Sleep_Mode.bits.b1VcellOVP != 0);
}

static bool Check_VcellUVP(void) {
    return (Sleep_Mode.bits.b1VcellUVP != 0);
}

static bool Check_Test(void) {
    return (Sleep_Mode.bits.b1TestSleep != 0);
}

// 休眠条件检查表
static const struct {
    SleepConditionCheckFunc check_func;
    SleepTrigger_e trigger;
    SleepMode_e mode;
} sleep_conditions[] = {
    {Check_Test,        SLEEP_TRIGGER_TEST,         SLEEP_MODE_NORMAL},
    {Check_OverCurrent, SLEEP_TRIGGER_OVERCURRENT,  SLEEP_MODE_DEEP},
    {Check_OverVdelta,  SLEEP_TRIGGER_OVERVDELTA,   SLEEP_MODE_DEEP},
    {Check_CBC,         SLEEP_TRIGGER_CBC,          SLEEP_MODE_DEEP},
    {Check_Forced,      SLEEP_TRIGGER_FORCED,       SLEEP_MODE_NORMAL},
    {Check_VcellOVP,    SLEEP_TRIGGER_OVP,          SLEEP_MODE_DEEP},
    {Check_VcellUVP,    SLEEP_TRIGGER_UVP,          SLEEP_MODE_DEEP},
};

#define SLEEP_CONDITION_COUNT (sizeof(sleep_conditions)/sizeof(sleep_conditions[0]))

// 初始化休眠管理器
void Sleep_Init(void) {
    // 初始化休眠管理器
    sleep_manager.mode = SLEEP_MODE_NONE;
    sleep_manager.trigger = SLEEP_TRIGGER_NONE;
    sleep_manager.state = SLEEP_STATE_IDLE;
    
    sleep_manager.sleep_cnt = 0;
    sleep_manager.wakeup_cnt = 0;
    sleep_manager.last_sleep_time = 0;
    sleep_manager.last_wakeup_time = 0;
    
    sleep_manager.is_sleeping = false;
    for (int i = 0; i < 10; i++) {
        sleep_manager.is_wakeup_source[i] = false;
    }
    
    // 加载配置
    sleep_manager.config.voltage_normal = OtherElement.u16Sleep_VNormal;
    sleep_manager.config.time_normal = OtherElement.u16Sleep_TimeNormal;
    sleep_manager.config.voltage_low = OtherElement.u16Sleep_Vlow;
    sleep_manager.config.time_low = OtherElement.u16Sleep_TimeVlow;
    sleep_manager.config.voltage_very_low = 2500;
    sleep_manager.config.time_very_low = 60; // 1小时
    sleep_manager.config.current_threshold = 10; // 1A
    sleep_manager.config.rtc_sleep_time = 30; // 30秒
    
    sleep_manager.state_machine_state = 0;
    sleep_manager.state_counter = 0;
}

// 检查是否满足休眠条件
bool Sleep_CheckConditions(void) {
    // 检查强制休眠条件
    if (Sleep_Mode.bits.b1ForceToSleep_L1 || 
        Sleep_Mode.bits.b1ForceToSleep_L2 || 
        Sleep_Mode.bits.b1ForceToSleep_L3) {
        return true;
    }
    
    // 检查异常休眠条件
    if (Sleep_Mode.bits.b1OverCurSleep || 
        Sleep_Mode.bits.b1OverVdeltaSleep || 
        Sleep_Mode.bits.b1CBCSleep || 
        Sleep_Mode.bits.b1VcellOVP || 
        Sleep_Mode.bits.b1VcellUVP || 
        Sleep_Mode.bits.b1TestSleep) {
        return true;
    }
    
    // 检查电压条件
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_very_low) {
        return true;
    }
    
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_low && 
        !g_stCellInfoReport.u16Ichg) {
        return true;
    }
    
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_normal && 
        !g_stCellInfoReport.u16Ichg && !g_stCellInfoReport.u16IDischg) {
        return true;
    }
    
    return false;
}

// 选择休眠模式
SleepMode_e Sleep_SelectMode(void) {
    // 优先检查强制休眠
    if (Sleep_Mode.bits.b1ForceToSleep_L1) {
        return SLEEP_MODE_HICCUP;
    }
    if (Sleep_Mode.bits.b1ForceToSleep_L2) {
        return SLEEP_MODE_NORMAL;
    }
    if (Sleep_Mode.bits.b1ForceToSleep_L3) {
        return SLEEP_MODE_DEEP;
    }
    
    // 检查异常休眠条件
    if (Sleep_Mode.bits.b1OverCurSleep || 
        Sleep_Mode.bits.b1OverVdeltaSleep || 
        Sleep_Mode.bits.b1CBCSleep || 
        Sleep_Mode.bits.b1VcellOVP || 
        Sleep_Mode.bits.b1VcellUVP) {
        return SLEEP_MODE_DEEP;
    }
    
    // 检查测试休眠
    if (Sleep_Mode.bits.b1TestSleep) {
        return SLEEP_MODE_NORMAL;
    }
    
    // 根据电压选择休眠模式
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_very_low) {
        return SLEEP_MODE_DEEP;
    }
    
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_low && 
        !g_stCellInfoReport.u16Ichg) {
        return SLEEP_MODE_DEEP;
    }
    
    if (g_stCellInfoReport.u16VCellMin <= sleep_manager.config.voltage_normal) {
        if (g_stCellInfoReport.u16Ichg || g_stCellInfoReport.u16IDischg) {
            return SLEEP_MODE_NONE; // 有电流，不休眠
        }
        
        // 根据RTC配置选择HICCUP或NORMAL模式
        if (OtherElement.u16Sleep_TimeRTC > 0) {
            return SLEEP_MODE_HICCUP;
        } else {
            return SLEEP_MODE_NORMAL;
        }
    }
    
    return SLEEP_MODE_NONE;
}

// 准备进入休眠
void Sleep_PrepareForSleep(void) {
    // 记录休眠前状态
    sleep_manager.last_sleep_time = sys_time.sys_tick_10ms;
    
    // 清除唤醒源状态
    for (int i = 0; i < 10; i++) {
        sleep_manager.is_wakeup_source[i] = false;
    }
    
    // 根据休眠模式进行不同的准备
    switch (sleep_manager.mode) {
        case SLEEP_MODE_HICCUP:
            // RTC休眠准备
            Init_RTC();
            IOstatus_RTCMode();
            InitWakeUp_RTCMode();
            break;
            
        case SLEEP_MODE_NORMAL:
            // 普通休眠准备
            IOstatus_NormalMode();
            InitWakeUp_NormalMode();
            break;
            
        case SLEEP_MODE_DEEP:
            // 深度休眠准备
            IOstatus_DeepMode();
            InitWakeUp_DeepMode();
            break;
            
        default:
            break;
    }
}

// 进入休眠
void Sleep_EnterSleep(SleepMode_e mode, SleepTrigger_e trigger) {
    // 设置休眠模式和触发原因
    sleep_manager.mode = mode;
    sleep_manager.trigger = trigger;
    sleep_manager.is_sleeping = true;
    sleep_manager.sleep_cnt++;
    
    // 记录日志
    LogRecord_Flag.bits.Log_Sleep = 1;
    LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
    
    // 准备进入休眠
    Sleep_PrepareForSleep();
    
    // 根据模式进入休眠
    switch (mode) {
        case SLEEP_MODE_HICCUP:
            // 写入flash标志
            BootFlag_Write(FLASH_HICCUP_SLEEP_VALUE);
            
            // 执行休眠操作
            App_AFEshutdown();
            lk8625_SendAT("AT+DISCON");
            lk8625_SendAT("AT+DSLEEP");
            
            // 进入STOP模式
            Sys_StopMode();
            break;
            
        case SLEEP_MODE_NORMAL:
            // 写入flash标志
            BootFlag_Write(FLASH_NORMAL_SLEEP_VALUE);
            
            // 执行休眠操作
            App_AFEshutdown();
            lk8625_SendAT("AT+DISCON");
            lk8625_SendAT("AT+DSLEEP");
            
            // 进入STOP模式
            Sys_StopMode();
            break;
            
        case SLEEP_MODE_DEEP:
            // 写入flash标志
            BootFlag_Write(FLASH_DEEP_SLEEP_VALUE);
            
            // 执行休眠操作
            App_AFEshutdown();
            lk8625_SendAT("AT+DISCON");
            lk8625_SendAT("AT+DSLEEP");
            
            // 复位MCU进入深度休眠
            MCU_RESET();
            break;
            
        default:
            break;
    }
}

// 唤醒处理
void Sleep_WakeupHandler(void) {
    // 记录唤醒时间
    sleep_manager.last_wakeup_time = sys_time.sys_tick_10ms;
    sleep_manager.wakeup_cnt++;
    sleep_manager.is_sleeping = false;
    
    // 恢复IO状态
    switch (sleep_manager.mode) {
        case SLEEP_MODE_HICCUP:
            IORecover_RTCMode();
            break;
            
        case SLEEP_MODE_NORMAL:
            IORecover_NormalMode();
            break;
            
        case SLEEP_MODE_DEEP:
            IORecover_DeepMode();
            break;
            
        default:
            break;
    }
    
    // 清除休眠标志
    Sleep_Mode.all = 0;
    Sleep_Status = SLEEP_HICCUP_SHIFT;
    
    // 清除flash标志
    BootFlag_Clear();
    
    // 重置休眠管理器状态
    sleep_manager.state = SLEEP_STATE_RECOVER;
    sleep_manager.mode = SLEEP_MODE_NONE;
    sleep_manager.trigger = SLEEP_TRIGGER_NONE;
}

// 清理休眠后处理
void Sleep_CleanupAfterWakeup(void) {
    // 清除休眠相关标志
    Sleep_Mode.all = 0;
    Sleep_Status = SLEEP_HICCUP_SHIFT;
    
    // 清除计数器
    sys_time.sleep_veryvlow_cnt = 0;
    sys_time.sleep_vlow_cnt = 0;
    sys_time.sleep_vnormal_cnt = 0;
    sys_time.afe_comm_err_sleepcnt = 0;
}

// 休眠处理主函数
void Sleep_Process(void) {
    static uint32_t condition_check_timer = 0;
    static uint32_t state_timer = 0;
    
    // 状态机处理
    switch (sleep_manager.state) {
        case SLEEP_STATE_IDLE:
            // 检查是否需要进入休眠状态
            if (Sleep_CheckConditions()) {
                sleep_manager.state = SLEEP_STATE_CHECK;
                condition_check_timer = 0;
            }
            break;
            
        case SLEEP_STATE_CHECK:
            // 持续检查条件
            condition_check_timer++;
            
            // 条件检查通过，进入准备状态
            if (condition_check_timer >= 2) { // 2秒确认
                sleep_manager.state = SLEEP_STATE_PREPARE;
                state_timer = 0;
            }
            
            // 条件不满足，回到空闲状态
            if (!Sleep_CheckConditions()) {
                sleep_manager.state = SLEEP_STATE_IDLE;
            }
            break;
            
        case SLEEP_STATE_PREPARE:
            // 准备进入休眠
            state_timer++;
            
            // 选择休眠模式
            sleep_manager.mode = Sleep_SelectMode();
            
            // 如果选择了有效的休眠模式，进入休眠状态
            if (sleep_manager.mode != SLEEP_MODE_NONE) {
                sleep_manager.state = SLEEP_STATE_ENTER;
            }
            
            // 超时检查
            if (state_timer >= 5) { // 5秒超时
                sleep_manager.state = SLEEP_STATE_IDLE;
            }
            break;
            
        case SLEEP_STATE_ENTER:
            // 执行休眠操作
            Sleep_EnterSleep(sleep_manager.mode, sleep_manager.trigger);
            sleep_manager.state = SLEEP_STATE_WAKEUP;
            break;
            
        case SLEEP_STATE_WAKEUP:
            // 等待唤醒，这里实际上已经复位或休眠
            // 在实际代码中，这里不会执行，因为休眠后会复位
            sleep_manager.state = SLEEP_STATE_RECOVER;
            break;
            
        case SLEEP_STATE_RECOVER:
            // 恢复处理
            Sleep_CleanupAfterWakeup();
            sleep_manager.state = SLEEP_STATE_IDLE;
            break;
            
        default:
            sleep_manager.state = SLEEP_STATE_IDLE;
            break;
    }
}

// 更新休眠配置
void Sleep_UpdateConfig(SleepConfig_t *config) {
    if (config) {
        sleep_manager.config = *config;
    }
}

// 获取休眠管理器状态
SleepManager_t* Sleep_GetManager(void) {
    return &sleep_manager;
}