#include "main.h"

#ifdef _IAP

// 个数为48，看.s文件相关vector个数，这个我没看过，后面学习一下怎么执行的
#if (defined(__CC_ARM))
__IO uint32_t VectorTable[48] __attribute__((at(0x20000000)));
#elif (defined(__ICCARM__))
#pragma location = 0x20000000
__no_init __IO uint32_t VectorTable[48];
#elif defined(__GNUC__)
__IO uint32_t VectorTable[48] __attribute__((section(".RAMVectorTable")));
#elif defined(__TASKING__)
__IO uint32_t VectorTable[48] __at(0x20000000);
#endif

#endif

void Init_IAPAPP(void)
{
#ifdef _IAP
	UINT8 i;
	// 就是这句话，导致无法debug，不按reset无法烧代码，具体原因后面再想，错误教程，函数都用错了
	// RCC_APB2PeriphResetCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); // 中断向量表重映射使能

	for (i = 0; i < 48; i++)
	{
		VectorTable[i] = *(__IO uint32_t *)(APPLICATION_ADDRESS + (i << 2));
	}
	SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_SRAM); // Remap SRAM at 0x00000000
#endif
}

static void BootFlag_EnableAccess(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	PWR->CR |= PWR_CR_DBP;
}

static UINT16 WakeDisplay_ClampSoc(UINT16 soc)
{
	if (soc > 100)
	{
		return 100;
	}

	return soc;
}

static UINT8 s_wake_display_shadow_valid = 0;
static UINT16 s_wake_display_shadow_mode = WAKE_DISPLAY_MODE_NONE;
static UINT16 s_wake_display_shadow_soc = 0;

static void WakeDisplayState_UpdateShadow(UINT16 mode, UINT16 soc, UINT8 valid)
{
	s_wake_display_shadow_valid = valid;
	s_wake_display_shadow_mode = mode;
	s_wake_display_shadow_soc = WakeDisplay_ClampSoc(soc);
}

static void WakeDisplayState_Write(UINT16 mode, UINT16 soc)
{
	UINT32 value;

	soc = WakeDisplay_ClampSoc(soc);
	value = ((UINT32)mode << 16) | soc;

	BootFlag_EnableAccess();
	RTC->BKP3R = value;
	RTC->BKP4R = ~value;
	WakeDisplayState_UpdateShadow(mode, soc, 1);
}

static UINT8 WakeDisplayState_ReadRaw(UINT32 *value)
{
	UINT32 inverse_value;

	BootFlag_EnableAccess();
	*value = RTC->BKP3R;
	inverse_value = RTC->BKP4R;
	if ((*value ^ inverse_value) != 0xFFFFFFFFu)
	{
		*value = 0;
		return 0;
	}

	return 1;
}

void BootFlag_Write(UINT16 flag)
{
	BootFlag_EnableAccess();
	RTC->BKP1R = flag;
	RTC->BKP2R = (UINT16)(~flag);
}

UINT16 BootFlag_Read(void)
{
	UINT16 flag;
	UINT16 inverse_flag;

	BootFlag_EnableAccess();
	flag = (UINT16)RTC->BKP1R;
	inverse_flag = (UINT16)RTC->BKP2R;
	if ((UINT16)(flag ^ inverse_flag) != 0xFFFF)
	{
		return BOOT_FLAG_RESET_VALUE;
	}

	return flag;
}

void BootFlag_Clear(void)
{
	BootFlag_Write(BOOT_FLAG_RESET_VALUE);
}

void WakeDisplaySocCache_Write(UINT16 soc)
{
	UINT32 value = 0;
	UINT16 mode = WAKE_DISPLAY_MODE_NONE;

	if (WakeDisplayState_ReadRaw(&value))
	{
		mode = (UINT16)(value >> 16);
	}

	WakeDisplayState_Write(mode, soc);
}

void WakeDisplay_RequestSocPreview(void)
{
	UINT32 value = 0;
	UINT16 soc = 0;

	if (WakeDisplayState_ReadRaw(&value))
	{
		soc = (UINT16)value;
	}

	WakeDisplayState_Write(WAKE_DISPLAY_MODE_SOC_PREVIEW, soc);
}

void WakeDisplay_RequestBootSequence(void)
{
	UINT32 value = 0;
	UINT16 soc = 0;

	if (WakeDisplayState_ReadRaw(&value))
	{
		soc = (UINT16)value;
	}

	WakeDisplayState_Write(WAKE_DISPLAY_MODE_BOOT_SEQUENCE, soc);
}

void WakeDisplayState_CaptureForBoot(void)
{
	UINT32 value = 0;

	if (!WakeDisplayState_ReadRaw(&value))
	{
		WakeDisplayState_UpdateShadow(WAKE_DISPLAY_MODE_NONE, 0, 0);
		return;
	}

	WakeDisplayState_UpdateShadow((UINT16)(value >> 16), (UINT16)value, 1);
}

UINT8 WakeDisplayState_Read(UINT16 *mode, UINT16 *soc)
{
	UINT32 value = 0;

	if (s_wake_display_shadow_valid)
	{
		if (mode)
		{
			*mode = s_wake_display_shadow_mode;
		}
		if (soc)
		{
			*soc = s_wake_display_shadow_soc;
		}
		return 1;
	}

	if (!WakeDisplayState_ReadRaw(&value))
	{
		if (mode)
		{
			*mode = WAKE_DISPLAY_MODE_NONE;
		}
		if (soc)
		{
			*soc = 0;
		}
		return 0;
	}

	WakeDisplayState_UpdateShadow((UINT16)(value >> 16), (UINT16)value, 1);

	if (mode)
	{
		*mode = s_wake_display_shadow_mode;
	}
	if (soc)
	{
		*soc = s_wake_display_shadow_soc;
	}

	return 1;
}

void WakeDisplayState_Clear(void)
{
	WakeDisplayState_Write(WAKE_DISPLAY_MODE_NONE, 0);
	WakeDisplayState_UpdateShadow(WAKE_DISPLAY_MODE_NONE, 0, 0);
}

void App_FlashUpdateDet(void)
{
	if (1 == u8FlashUpdateFlag)
	{
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		MCU_RESET();
	}
}

UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
	FLASH_Status result;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	while (FLASH_ErasePage(StartAddr) != FLASH_COMPLETE)
		;
	result = FLASH_ProgramHalfWord(StartAddr, Buffer);
	FLASH_Lock();
	return result;
}
