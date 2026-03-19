/**
 * @file SleepManager_LowPower.h
 * @brief 增强的低功耗管理模块（分层休眠+周期性唤醒+自适应策略）
 * @note 基于BMS行业最佳实践设计
 */

#ifndef SLEEP_MANAGER_LOWPOWER_H
#define SLEEP_MANAGER_LOWPOWER_H

#include "main.h"

// 布尔类型定义（兼容Keil C51）
#ifndef BOOL
typedef UINT8 BOOL;
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// ==================== 休眠模式定义（分层）====================
typedef enum {
    SLEEP_MODE_NONE = 0,        // 不休眠，正常工作模式
    SLEEP_MODE_LIGHT,           // 浅休眠：STOP模式，快速响应，短周期唤醒
    SLEEP_MODE_MEDIUM,          // 中休眠：STOP模式，中等响应，中周期唤醒
    SLEEP_MODE_DEEP             // 深休眠：STANDBY模式，最省电，长周期唤醒
} SleepMode_e;

// ==================== 休眠状态定义 ====================
typedef enum {
    SLEEP_STATE_ACTIVE = 0,     // 正常工作状态
    SLEEP_STATE_IDLE,           // 空闲状态，准备进入休眠
    SLEEP_STATE_LIGHT_SLEEP,    // 浅休眠中
    SLEEP_STATE_MEDIUM_SLEEP,   // 中休眠中
    SLEEP_STATE_DEEP_SLEEP,     // 深休眠中
    SLEEP_STATE_WAKEUP          // 唤醒处理中
} SleepState_e;

// ==================== 唤醒源定义 ====================
typedef enum {
    WAKE_SOURCE_NONE = 0,       // 无唤醒源
    WAKE_SOURCE_CURRENT,        // 电流变化唤醒
    WAKE_SOURCE_COMMUNICATION,  // 通信唤醒
    WAKE_SOURCE_RTC,            // RTC定时唤醒
    WAKE_SOURCE_FAULT,          // 故障唤醒
    WAKE_SOURCE_EXTERNAL        // 外部中断唤醒
} WakeSource_e;

// ==================== 休眠触发原因 ====================
typedef enum {
    SLEEP_TRIGGER_NONE = 0,
    SLEEP_TRIGGER_NO_ACTIVITY,      // 无活动（无电流/无通信）
    SLEEP_TRIGGER_VOLTAGE_LOW,      // 电压过低
    SLEEP_TRIGGER_VOLTAGE_VERYLOW,  // 电压极低
    SLEEP_TRIGGER_OVERCURRENT,      // 过流保护
    SLEEP_TRIGGER_AFE_ERROR,        // AFE通信错误
    SLEEP_TRIGGER_TIMEOUT           // 超时休眠
} SleepTrigger_e;

// ==================== 分层休眠配置 ====================
typedef struct {
    // 浅休眠配置（STOP模式，快速响应）
    UINT16 light_sleep_idle_time;     // 进入浅休眠的空闲时间（秒）
    UINT16 light_sleep_wakeup_time;   // 浅休眠唤醒周期（秒）
    
    // 中休眠配置（STOP模式，中等响应）
    UINT16 medium_sleep_idle_time;    // 进入中休眠的空闲时间（秒）
    UINT16 medium_sleep_wakeup_time;  // 中休眠唤醒周期（秒）
    
    // 深休眠配置（STANDBY模式，最省电）
    UINT16 deep_sleep_idle_time;      // 进入深休眠的空闲时间（秒）
    UINT16 deep_sleep_wakeup_time;    // 深休眠唤醒周期（秒）
    
    // 电压阈值（单位：mV）
    UINT16 voltage_normal;            // 正常电压阈值
    UINT16 voltage_low;               // 低电压阈值
    UINT16 voltage_very_low;          // 极低电压阈值
    
    // 电流阈值（单位：A*10）
    UINT16 current_threshold;         // 电流判断阈值
    
    // 自适应休眠系数（SOC百分比）
    UINT8 soc_high_threshold;         // SOC高阈值（>80%）
    UINT8 soc_low_threshold;          // SOC低阈值（<20%）
    float sleep_time_factor_high;     // SOC高时的休眠时间系数
    float sleep_time_factor_normal;   // SOC正常时的休眠时间系数
    float sleep_time_factor_low;      // SOC低时的休眠时间系数
} SleepConfig_t;

// ==================== 休眠统计信息 ====================
typedef struct {
    UINT32 total_sleep_count;         // 总休眠次数
    UINT32 light_sleep_count;         // 浅休眠次数
    UINT32 medium_sleep_count;        // 中休眠次数
    UINT32 deep_sleep_count;          // 深休眠次数
    UINT32 wakeup_count;              // 唤醒次数
    UINT32 fault_wakeup_count;        // 故障唤醒次数
    UINT32 rtc_wakeup_count;          // RTC唤醒次数
    UINT32 current_wakeup_count;      // 电流唤醒次数
    UINT32 total_sleep_time;          // 总休眠时间（秒）
} SleepStats_t;

// ==================== 休眠管理器结构 ====================
typedef struct {
    // 当前状态
    SleepState_e state;                 // 当前状态
    SleepMode_e mode;                   // 当前休眠模式
    SleepTrigger_e trigger;             // 触发原因
    WakeSource_e wake_source;           // 最近唤醒源
    
    // 配置
    SleepConfig_t config;               // 休眠配置
    
    // 计数器
    UINT32 idle_counter;                // 空闲计数器（秒）
    UINT32 sleep_timer;                 // 休眠定时器
    UINT32 wakeup_timer;                // 唤醒定时器
    
    // 状态标志
    BOOL is_sleeping;                   // 是否正在休眠
    BOOL is_first_sleep;                // 是否首次休眠
    BOOL has_pending_fault;             // 是否有待处理故障
    
    // 周期性唤醒
    BOOL enable_periodic_wakeup;        // 启用周期性唤醒
    UINT32 periodic_interval;           // 周期性唤醒间隔（秒）
    UINT32 periodic_counter;            // 周期性唤醒计数器
    
    // 自适应休眠
    BOOL enable_adaptive_sleep;         // 启用自适应休眠
    UINT16 current_soc;                 // 当前SOC（0-100）
    
    // 统计信息
    SleepStats_t stats;                 // 统计信息
} SleepManager_t;

// ==================== 全局变量声明 ====================
extern SleepManager_t g_sleepMgr;

// ==================== 函数声明 ====================

/**
 * @brief 初始化休眠管理器
 * @param config 配置参数，NULL使用默认配置
 */
void Sleep_Init(const SleepConfig_t *config);

/**
 * @brief 休眠处理主函数（1秒调用一次）
 */
void Sleep_Process(void);

/**
 * @brief 周期性唤醒检查函数（在RTC中断中调用）
 */
void Sleep_PeriodicWakeupCheck(void);

/**
 * @brief 进入休眠
 * @param mode 休眠模式
 */
void Sleep_EnterSleep(SleepMode_e mode);

/**
 * @brief 唤醒处理
 * @param source 唤醒源
 */
void Sleep_WakeupHandler(WakeSource_e source);

/**
 * @brief 设置休眠配置
 * @param config 新的配置
 */
void Sleep_SetConfig(const SleepConfig_t *config);

/**
 * @brief 获取休眠配置
 * @return 当前配置指针
 */
const SleepConfig_t* Sleep_GetConfig(void);

/**
 * @brief 获取当前状态
 * @return 当前状态
 */
SleepState_e Sleep_GetState(void);

/**
 * @brief 获取当前休眠模式
 * @return 当前休眠模式
 */
SleepMode_e Sleep_GetMode(void);

/**
 * @brief 获取当前唤醒源
 * @return 唤醒源
 */
WakeSource_e Sleep_GetWakeSource(void);

/**
 * @brief 获取休眠统计信息
 * @return 统计信息指针
 */
const SleepStats_t* Sleep_GetStats(void);

/**
 * @brief 清除休眠统计
 */
void Sleep_ClearStats(void);

/**
 * @brief 设置SOC值（用于自适应休眠）
 * @param soc SOC值（0-100）
 */
void Sleep_SetSOC(UINT16 soc);

/**
 * @brief 启用/禁用周期性唤醒
 * @param enable TRUE=启用，FALSE=禁用
 */
void Sleep_EnablePeriodicWakeup(BOOL enable);

/**
 * @brief 设置周期性唤醒间隔
 * @param interval 唤醒间隔（秒）
 */
void Sleep_SetPeriodicInterval(UINT32 interval);

/**
 * @brief 启用/禁用自适应休眠
 * @param enable TRUE=启用，FALSE=禁用
 */
void Sleep_EnableAdaptiveSleep(BOOL enable);

/**
 * @brief 检查是否有电流
 * @return TRUE=有电流，FALSE=无电流
 */
BOOL Sleep_HasCurrent(void);

/**
 * @brief 检查AFE通信是否正常
 * @return TRUE=正常，FALSE=错误
 */
BOOL Sleep_IsAfeOk(void);

/**
 * @brief 获取自适应休眠时间
 * @param base_time 基础休眠时间
 * @return 调整后的休眠时间
 */
UINT32 Sleep_GetAdaptiveTime(UINT32 base_time);

// ==================== 默认配置宏 ====================
#define SLEEP_CONFIG_DEFAULT {5,10,30,120,300,1800,4200,3000,2500,5,80,20,1.5f,1.0f,0.5f}

#endif /* SLEEP_MANAGER_LOWPOWER_H */