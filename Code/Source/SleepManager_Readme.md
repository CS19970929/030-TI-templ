# SleepManager 模块使用说明

## 一、模块概述

`SleepManager` 是一个精简的休眠管理模块，**仅基于电压、电流、通信三个条件**进行休眠控制，移除了原有的测试休眠、强制休眠、CBC休眠、过压休眠等无关部分。

### 核心特点
- **精简逻辑**：只有电压、电流、通信三个判断条件
- **两种休眠模式**：普通休眠（STOP模式）和深度休眠（STANDBY模式）
- **状态机清晰**：空闲→检查→准备→休眠→唤醒，状态清晰
- **易于配置**：所有阈值和时间参数可配置

## 二、休眠触发条件

### 1. 电压条件
| 条件 | 阈值配置 | 触发休眠 | 等待时间 |
|------|----------|----------|----------|
| 电压极低 | `voltage_very_low` (如2500mV) | 深度休眠 | `time_very_low` (如1小时) |
| 电压低且无电流 | `voltage_low` (如3000mV) | 深度休眠 | `time_low` (如24小时) |
| 电压正常且无电流 | `voltage_normal` (如4200mV) | 普通休眠 | `time_normal` (如24小时) |

### 2. 电流条件
| 条件 | 触发动作 |
|------|----------|
| 过流保护触发 | 立即进入深度休眠 |
| 检测到充电/放电电流 | 重置休眠计数器（有活动不休眠） |

### 3. 通信条件
| 条件 | 触发动作 |
|------|----------|
| AFE通信错误 | 立即进入深度休眠 |
| 外部通信活跃 | 重置休眠计数器 |

## 三、文件说明

```
SleepManager.h      - 模块头文件
SleepManager.c      - 模块实现文件
SleepManager_Readme.md - 本说明文档
```

## 四、集成步骤

### 1. 添加文件到工程
将 `SleepManager.h` 和 `SleepManager.c` 添加到你的工程中。

### 2. 在main.c中初始化
```c
#include "SleepManager.h"

int main(void)
{
    SystemInit();
    
    // 初始化休眠管理器（使用默认配置）
    Sleep_Init(NULL);
    
    // 或使用自定义配置
    // SleepConfig_t my_config = SLEEP_CONFIG_DEFAULT;
    // my_config.voltage_normal = 4000;  // 修改正常休眠阈值
    // Sleep_Init(&my_config);
    
    while (1) {
        App_SysTime();
        // ... 其他应用函数
        
        // 在主循环中调用休眠处理（建议1s调用一次）
        if (g_st_SysTimeFlag.bits.b1Sys1000msFlag1) {
            g_st_SysTimeFlag.bits.b1Sys1000msFlag1 = 0;
            Sleep_Process();
        }
    }
}
```

### 3. 在中断中处理唤醒
```c
// 外部中断唤醒处理
void EXTI0_1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        // 唤醒处理
        Sleep_WakeupHandler(0);  // 0=外部中断唤醒
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

// RTC闹钟唤醒处理
void RTC_Alarm_IRQHandler(void)
{
    if (RTC_GetFlagStatus(RTC_FLAG_ALRAF) != RESET) {
        // 唤醒处理
        Sleep_WakeupHandler(1);  // 1=RTC唤醒
        RTC_ClearFlag(RTC_FLAG_ALRAF);
    }
}
```

## 五、配置参数说明

### 默认配置
```c
SleepConfig_t default_config = {
    .voltage_normal = 4200,     // 正常休眠电压阈值（mV）
    .voltage_low = 3000,        // 低电压阈值（mV）
    .voltage_very_low = 2500,   // 极低电压阈值（mV）
    .time_normal = 1440,        // 正常休眠等待时间（分钟），24小时
    .time_low = 1440,           // 低电压休眠等待时间（分钟），24小时
    .time_very_low = 60,        // 极低电压休眠等待时间（分钟），1小时
    .current_threshold = 5      // 电流阈值（A*10），0.5A
};
```

### 配置示例
```c
// 磷酸铁锂电池配置
SleepConfig_t lifepo_config = {
    .voltage_normal = 3300,     // 3.3V
    .voltage_low = 2800,        // 2.8V
    .voltage_very_low = 2500,   // 2.5V
    .time_normal = 1440,        // 24小时
    .time_low = 720,            // 12小时
    .time_very_low = 30,        // 30分钟
    .current_threshold = 5      // 0.5A
};

// 三元锂电池配置
SleepConfig_t ternary_config = {
    .voltage_normal = 3600,     // 3.6V
    .voltage_low = 3200,        // 3.2V
    .voltage_very_low = 2800,   // 2.8V
    .time_normal = 1440,        // 24小时
    .time_low = 720,            // 12小时
    .time_very_low = 30,        // 30分钟
    .current_threshold = 5      // 0.5A
};
```

## 六、API函数说明

### 初始化函数
```c
void Sleep_Init(const SleepConfig_t *config);
```
- `config`: 配置参数，传NULL使用默认配置

### 主处理函数
```c
void Sleep_Process(void);
```
- 在主循环中调用，建议每秒调用一次
- 内部会检查电压、电流、通信条件

### 状态查询函数
```c
SleepState_e Sleep_GetState(void);  // 获取当前状态
SleepMode_e Sleep_GetMode(void);    // 获取当前模式
```

### 配置函数
```c
void Sleep_SetConfig(const SleepConfig_t *config);  // 设置配置
const SleepConfig_t* Sleep_GetConfig(void);         // 获取配置
```

### 唤醒处理函数
```c
void Sleep_WakeupHandler(uint8_t wake_source);
```
- `wake_source`: 唤醒源类型（0=外部中断，1=RTC）

### 辅助函数
```c
bool Sleep_HasCurrent(void);    // 检查是否有电流
bool Sleep_IsAfeOk(void);       // 检查AFE通信是否正常
```

### 统计函数
```c
void Sleep_GetStats(uint32_t *sleep_count, uint32_t *wakeup_count);
void Sleep_ClearStats(void);
```

## 七、状态机说明

```
┌─────────────┐
│    IDLE     │  空闲状态，正常工作
└──────┬──────┘
       │ 条件满足
       ▼
┌─────────────┐
│  CHECKING   │  检查休眠条件
└──────┬──────┘
       │ 确认满足
       ▼
┌─────────────┐
│  PREPARING  │  准备进入休眠
└──────┬──────┘
       │ 执行休眠
       ▼
┌─────────────┐
│  SLEEPING   │  休眠中
└──────┬──────┘
       │ 唤醒
       ▼
┌─────────────┐
│   WAKEUP    │  唤醒处理
└──────┬──────┘
       │ 处理完成
       ▼
┌─────────────┐
│    IDLE     │  回到空闲
└─────────────┘
```

## 八、与原有模块的兼容

新模块复用了原有`SleepDeal.c`中的以下函数：
- `InitWakeUp_Base()` - 唤醒引脚配置
- `InitWakeUp_NormalMode()` - 普通休眠唤醒配置
- `InitWakeUp_DeepMode()` - 深度休眠唤醒配置
- `IOstatus_NormalMode()` - 普通休眠IO配置
- `IOstatus_DeepMode()` - 深度休眠IO配置
- `IORecover_NormalMode()` - 普通休眠唤醒恢复
- `Sys_StopMode()` - 进入STOP模式
- `BootFlag_Write()` - 写入Flash标志
- `App_AFEshutdown()` - 关闭AFE
- `lk8625_SendAT()` - 蓝牙控制

## 九、移除的功能

以下功能已被移除：
- ❌ 测试休眠（TestSleep）
- ❌ 强制休眠（ForceToSleep_L1/L2/L3）
- ❌ CBC休眠（CBCSleep）
- ❌ 单体过压休眠（VcellOVP）
- ❌ 打嗝休眠模式（HICCUP_MODE）
- ❌ 正常休眠L1/L2/L3分级
- ❌ 过压差休眠（OverVdeltaSleep）

## 十、调试建议

### 1. 查看休眠状态
```c
// 在调试串口中打印状态
printf("Sleep State: %d, Mode: %d, Trigger: %d\n", 
       Sleep_GetState(), Sleep_GetMode(), g_sleepMgr.trigger);
```

### 2. 查看统计信息
```c
uint32_t sleep_cnt, wakeup_cnt;
Sleep_GetStats(&sleep_cnt, &wakeup_cnt);
printf("Sleep Count: %lu, Wakeup Count: %lu\n", sleep_cnt, wakeup_cnt);
```

### 3. 监控电压和电流
```c
printf("VCellMin: %dmV, IChg: %d, IDischg: %d\n",
       g_stCellInfoReport.u16VCellMin,
       g_stCellInfoReport.u16Ichg,
       g_stCellInfoReport.u16IDischg);
```

## 十一、注意事项

1. **计数器单位**：时间计数器以秒为单位累加，配置的`time_*`参数单位是分钟
2. **电流阈值**：`current_threshold`单位是A*10，即5表示0.5A
3. **Flash写入**：休眠前会写入Flash标志，注意Flash写入次数限制
4. **唤醒源配置**：确保唤醒引脚配置正确，否则无法唤醒
5. **IO状态**：休眠前会配置IO状态，唤醒后需要正确恢复

## 十二、常见问题

### Q: 为什么进入休眠后无法唤醒？
A: 检查唤醒引脚配置是否正确，确保唤醒源能够产生中断。

### Q: 电压达到阈值但没有进入休眠？
A: 检查是否有电流（Sleep_HasCurrent()），有电流时不会进入休眠。

### Q: 如何修改休眠阈值？
A: 使用`Sleep_SetConfig()`函数或在初始化时传入自定义配置。

### Q: 如何禁用休眠功能？
A: 不调用`Sleep_Process()`即可，或在回调函数中返回false。