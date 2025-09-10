#ifndef FLASH_H
#define FLASH_H

//系统存储器：出厂由ST在这个区域内部预置了一段BootLoader，也就是ISP程序，这是一块ROM
//没有什么中低容量产品，只会用C8和R8，均为64K，	 每页1KB。系统存储器3KB。
#define FLASH_ADDR_IAP_START 			0x08000000		//IAP=7K，这个地方出了一次问题，修改后大于6K，侧面反映编译出来的Zi-data也是flash的东西
#define FLASH_ADDR_APP_START 			0x08001C00		//APP=64-7-1-1=55K


// #define FLASH_ADDR_SOC_RTC_CNT          0x0800F000		//RTC次数统计，用于是否更新SOC

#define FLASH_ADDR_WAKE_TYPE            0x0800F400		//唤醒方式的记录
#define FLASH_ADDR_UPDATE_FLAG 			0x0800F800		//升级标志位，1K
#define FLASH_ADDR_SLEEP_FLAG           0x0800FC00		//休眠关键指令，1K

#define FLASH_TO_IAP_VALUE				((UINT16)0x00AB)
#define FLASH_TO_APP_VALUE				((UINT16)0xFFFF)

#define FLASH_VALUE_WAKE_RTC            ((UINT16)0x1234)
#define FLASH_VALUE_WAKE_OTHER          ((UINT16)0xFFFF)

#define FLASH_NORMAL_SLEEP_VALUE    	((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE    		((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE    	((UINT16)0x1236)
#define FLASH_SLEEP_RESET_VALUE    		((UINT16)0xFFFF)


#define MCU_RESET()	NVIC_SystemReset()


FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr,uint16_t Buffer);
UINT16 FlashReadOneHalfWord(UINT32 faddr);
void App_FlashUpdateDet(void);
void Init_IAPAPP(void);

#endif	/* FLASH_H */

