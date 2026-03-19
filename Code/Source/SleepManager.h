/**
 * @file SleepManager.h
 * @brief 精简的休眠管理模块（仅基于电压、电流、通信）
 * @note 移除了测试休眠、强制休眠、CBC休眠、过压休眠等无关部分
 */

#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include "main.h"

// ==================== 休眠模式定义 ====================
typedef enum {
    SLEEP_MODE_NONE = 0,    // 不休眠
    SLEEP_MODE_NORMAL,      // 普通休眠（STOP模式，外部中断唤醒）
    SLEEP_MODE_DEEP         // 深度休眠（STANDBY模式，仅特定唤醒源）
} SleepMode_e;

// ==================== 休眠状态定义 ====================
typedef enum {
    SLEEP_STATE_IDLE = 0,       // 空闲状态，正常工作
    SLEEP_STATE_CHECKING,       // 正在检查休眠条件
    SLEEP_STATE_PREPARING,      // 准备进入休眠
    SLEEP_STATE_SLEEPING,       // 正在休眠中
    SLEEP_STATE_WAKEUP          // 唤醒处理中
} SleepState_e;

// ==================== 休眠触发原因 ====================
typedef enum {
    SLEEP_TRIGGER_NONE = 0,
    SLEEP_TRIGGER_VOLTAGE_LOW,      // 电压过低（正常阈值）
    SLEEP_TRIGGER_VOLTAGE_VERYLOW,  // 电压极低（深度休眠阈值）
    SLEEP_TRIGGER_OVERCURRENT,      // 过流保护
    SLEEP_TRIGGER_AFE_ERROR,        // AFE通信错误
    SLEEP_TRIGGER_NO_CURRENT        // 无电流且电压低
} SleepTrigger_e;

// ==================== 休眠条件配置 ====================
typedef struct {
    // 电压阈值（单位：mV）
    uint16_t voltage_normal;        // 正常休眠电压阈值（如4200mV）
    uint16_t voltage_low;           // 低电压阈值（如3000mV）
    uint16_t voltage_very_low;      // 极低电压阈值（如2500mV）
    
    // 时间阈值（单位：分钟）
    uint16_t time_normal;           // 正常休眠等待时间
    uint16_t time_low;              // 低电压休眠等待时间
    uint16_t time_very_low;         // 极低电压休眠等待时间
    
    // 电流阈值（单位：A*10，即0.1A为单位）
    uint16_t current_threshold;     // 电流判断阈值（低于此值视为无电流）
} SleepConfig_t;

// ==================== 休眠管理器结构 ====================
typedef struct {
    // 当前状态
    SleepState_e state;             // 当前休眠状态
    SleepMode_e mode;               // 休眠模式
    SleepTrigger_e trigger;         // 触发原因
    
    // 配置
    SleepConfig_t config;           // 休眠配置参数
    
    // 计数器（用于延时判断）
    uint32_t cnt_voltage_normal;    // 正常电压计数器
    uint32_t cnt_voltage_low;       // 低电压计数器
    uint32_t cnt_voltage_very_low;  // 极低电压计数器
    
    // 状态标志
    bool is_sleeping;               // 是否正在休眠
    bool wake_source_ext;           // 外部中断唤醒源
    bool wake_source_rtc;           // RTC唤醒源
    
    // 统计信息
    uint32_t sleep_count;           // 休眠次数统计
    uint32_t wakeup_count;          // 唤醒次数统计
} SleepManager_t;

// ==================== 全局变量声明 ====================
extern SleepManager_t g_sleepMgr;

// ==================== 函数声明 ====================

/**
 * @brief 初始化休眠管理器
 * @param config 休眠配置参数，传NULL使用默认配置
 */
void Sleep_Init(const SleepConfig_t *config);

/**
 * @brief 休眠处理主函数（在主循环中调用，建议1s调用一次）
 * @note 内部会检查电压、电流、通信条件
 */
void Sleep_Process(void);

/**
 * @brief 获取当前休眠状态
 * @return 当前休眠状态
 */
SleepState_e Sleep_GetState(void);

/**
 * @brief 获取当前休眠模式
 * @return 当前休眠模式
 */
SleepMode_e Sleep_GetMode(void);

/**
 * @brief 设置休眠配置参数
 * @param config 新的配置参数
 */
void Sleep_SetConfig(const SleepConfig_t *config);

/**
 * @brief 获取休眠配置参数
 * @return 当前配置参数指针
 */
const SleepConfig_t* Sleep_GetConfig(void);

/**
 * @brief 执行休眠操作（根据当前模式进入休眠）
 */
void Sleep_EnterSleep(void);

/**
 * @brief 唤醒处理函数
 * @param wake_source 唤醒源类型（0=外部中断，1=RTC）
 */
void Sleep_WakeupHandler(uint8_t wake_source);

/**
 * @brief 检查是否有电流（充电或放电）
 * @return true=有电流，false=无电流
 */
bool Sleep_HasCurrent(void);

/**
 * @brief 检查AFE通信是否正常
 * @return true=正常，false=错误
 */
bool Sleep_IsAfeOk(void);

/**
 * @brief 获取休眠统计信息
 * @param sleep_count 输出休眠次数
 * @param wakeup_count 输出唤醒次数
 */
void Sleep_GetStats(uint32_t *sleep_count, uint32_t *wakeup_count);

/**
 * @brief 清除休眠统计
 */
void Sleep_ClearStats(void);

// ==================== 默认配置宏 ====================
// 默认配置：磷酸铁锂电池
#define SLEEP_CONFIG_DEFAULT { \
    .voltage_normal = 4200,     /* 正常休眠电压阈值 */ \
    .voltage_low = 3000,        /* 低电压阈值 */ \
    .voltage_very_low = 2500,   /* 极低电压阈值 */ \
    .time_normal = 1440,        /* 正常休眠：24小时 */ \
    .time_low = 1440,           /* 低电压休眠：24小时 */ \
    .time_very_low = 60,        /* 极低电压休眠：1小时 */ \
    .current_threshold = 5      /* 电流阈值：0.5A */ \
}

#endif /* SLEEP_MANAGER_H */