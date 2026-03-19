#ifndef SLEEPDEAL_OPTIMIZED_H
#define SLEEPDEAL_OPTIMIZED_H

#include "main.h"

// 优化后的休眠模式定义
typedef enum {
    SLEEP_MODE_NONE = 0,        // 不休眠
    SLEEP_MODE_HICCUP,          // 打嗝休眠（RTC定时唤醒）
    SLEEP_MODE_NORMAL,          // 普通休眠（STOP模式）
    SLEEP_MODE_DEEP             // 深度休眠
} SleepMode_e;

// 休眠触发原因
typedef enum {
    SLEEP_TRIGGER_NONE = 0,
    SLEEP_TRIGGER_TEST,         // 测试休眠
    SLEEP_TRIGGER_OVERCURRENT,  // 过流保护
    SLEEP_TRIGGER_OVERVDELTA,   // 过压差保护
    SLEEP_TRIGGER_CBC,          // CBC保护
    SLEEP_TRIGGER_FORCED,       // 强制休眠
    SLEEP_TRIGGER_OVP,          // 单体过压
    SLEEP_TRIGGER_UVP,          // 单体欠压
    SLEEP_TRIGGER_VOLTAGE_LOW,  // 电压过低
    SLEEP_TRIGGER_VOLTAGE_NORMAL, // 正常休眠电压
    SLEEP_TRIGGER_TIMEOUT,      // 超时休眠
    SLEEP_TRIGGER_COMMUNICATION // 通信休眠
} SleepTrigger_e;

// 休眠状态
typedef enum {
    SLEEP_STATE_IDLE = 0,       // 空闲状态
    SLEEP_STATE_CHECK,          // 检查休眠条件
    SLEEP_STATE_PREPARE,        // 准备进入休眠
    SLEEP_STATE_ENTER,          // 进入休眠
    SLEEP_STATE_WAKEUP,         // 唤醒处理
    SLEEP_STATE_RECOVER         // 恢复处理
} SleepState_e;

// 休眠条件配置
typedef struct {
    uint16_t voltage_normal;    // 正常休眠电压阈值（mV）
    uint16_t time_normal;       // 正常休眠时间（min）
    uint16_t voltage_low;       // 低电压休眠阈值（mV）
    uint16_t time_low;          // 低电压休眠时间（min）
    uint16_t voltage_very_low;  // 极低电压阈值（mV）
    uint16_t time_very_low;     // 极低电压休眠时间（min）
    uint16_t current_threshold; // 电流阈值（A*10）
    uint16_t rtc_sleep_time;    // RTC休眠时间（s）
} SleepConfig_t;

// 休眠管理结构体
typedef struct {
    SleepMode_e mode;           // 当前休眠模式
    SleepTrigger_e trigger;     // 休眠触发原因
    SleepState_e state;         // 当前状态
    SleepConfig_t config;       // 休眠配置
    
    uint32_t sleep_cnt;         // 休眠计数器
    uint32_t wakeup_cnt;        // 唤醒计数器
    uint32_t last_sleep_time;   // 上次休眠时间
    uint32_t last_wakeup_time;  // 上次唤醒时间
    
    bool is_sleeping;           // 是否正在休眠
    bool is_wakeup_source[10];  // 唤醒源状态
    
    // 状态机相关
    uint8_t state_machine_state;    // 状态机状态
    uint32_t state_counter;         // 状态计数器
} SleepManager_t;

// 函数声明
void Sleep_Init(void);
void Sleep_Process(void);
void Sleep_EnterSleep(SleepMode_e mode, SleepTrigger_e trigger);
void Sleep_WakeupHandler(void);
bool Sleep_CheckConditions(void);
void Sleep_UpdateConfig(SleepConfig_t *config);
SleepMode_e Sleep_SelectMode(void);
void Sleep_PrepareForSleep(void);
void Sleep_CleanupAfterWakeup(void);

#endif // SLEEPDEAL_OPTIMIZED_H