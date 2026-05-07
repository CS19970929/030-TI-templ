#include "main.h"
#include "stm32f0xx_flash.h"

#define EEPROM_FLASH_SLOT_A_ADDR            ((UINT32)0x0800D000)
#define EEPROM_FLASH_SLOT_B_ADDR            ((UINT32)0x0800D800)
#define EEPROM_FLASH_SLOT_SIZE              ((UINT32)0x00000800)
#define EEPROM_FLASH_PAGE_SIZE              ((UINT32)0x00000400)
#define EEPROM_FLASH_STORAGE_VERSION        ((UINT16)0x0001)
#define EEPROM_FLASH_MAGIC                  ((UINT32)0x45505231u) /* "EPR1" */

#define EEPROM_STORAGE_RAW_BYTES            ((UINT16)(E2P_ADDR_E2POS_EVENT_POINT + 4U))
#define EEPROM_STORAGE_COMMIT_DELAY_TICKS   ((UINT8)5)
#define EEPROM_EVENT_RECORD_LENGTH           ((UINT8)100)

typedef struct
{
	UINT32 magic;
	UINT16 storage_version;
	UINT16 payload_size;
	UINT32 sequence;
	UINT16 crc;
	UINT16 reserved;
} EEPROM_FLASH_HEADER;

typedef struct
{
	UINT8 raw[EEPROM_STORAGE_RAW_BYTES];
	UINT16 switch_onoff;
	UINT16 sys_func_select[2];
	UINT16 pass_flag;
	UINT16 sleep_flag;
	UINT16 flash_update_flag;
} EEPROM_FLASH_PAYLOAD;

typedef char EEPROM_STORAGE_SIZE_CHECK[
	((sizeof(EEPROM_FLASH_HEADER) + sizeof(EEPROM_FLASH_PAYLOAD)) <= EEPROM_FLASH_SLOT_SIZE) ? 1 : -1];

static void EEPROM_ResetData_AllToDefault(void);
static void EEPROM_ResetData_OtherToDefault(void);
static void ReadEEPROM_ByteData_StartUp(void);
static void WriteEEPROM_ByteData_Circle(void);
static UINT8 EEPROM_SaveSnapshotNow(void);
static UINT8 EEPROM_LoadSnapshotFromFlash(void);

UINT32 u32E2P_Pro_VolCur_WriteFlag = 0;
UINT32 u32E2P_Pro_Temp_WriteFlag = 0;
UINT32 u32E2P_Pro_Other_WriteFlag = 0;
UINT32 u32E2P_OtherElement1_WriteFlag = 0;
UINT32 u32E2P_HeatCool_WriteFlag = 0;

UINT8 u8E2P_KB_WriteFlag = 0;

UINT8 u8E2P_KB_WritePos = 0;

static EEPROM_FLASH_PAYLOAD s_tStorage;
static UINT8 s_u8StorageDirty = 0;
static UINT8 s_u8StorageCommitDelay = 0;

static UINT16 EEPROM_CalcCrc16(const UINT8 *data, UINT16 length)
{
	UINT16 crc;
	UINT16 i;

	if (data == 0 || length == 0)
	{
		return 0xFFFFU;
	}

	crc = 0xFFFFU;
	while (length--)
	{
		crc ^= *data++;
		for (i = 0; i < 8U; ++i)
		{
			if (crc & 0x0001U)
			{
				crc = (UINT16)((crc >> 1) ^ 0xA001U);
			}
			else
			{
				crc >>= 1;
			}
		}
	}

	return crc;
}

static void EEPROM_MarkDirty(void)
{
	s_u8StorageDirty = 1;
	s_u8StorageCommitDelay = 0;
}

static void EEPROM_ResetShadowBlank(void)
{
	memset(&s_tStorage, 0xFF, sizeof(s_tStorage));
	EEPROM_MarkDirty();
}

static UINT8 EEPROM_IsRawAddr(UINT16 addr)
{
	return (UINT8)(addr < EEPROM_STORAGE_RAW_BYTES);
}

static UINT8 EEPROM_IsSpecialWordAddr(UINT16 addr)
{
	return (UINT8)((addr == EEPROM_ADDR_SWITCH_ONOFF) ||
				   (addr == EEPROM_ADDR_SYS_FUNC_SELECT) ||
				   (addr == (EEPROM_ADDR_SYS_FUNC_SELECT + 2U)) ||
				   (addr == EEPROM_ADDR_PASS) ||
				   (addr == EEPROM_ADDR_SLEEP) ||
				   (addr == EEPROM_ADDR_FLASHUPDATE));
}

static UINT16 EEPROM_ReadSpecialWord(UINT16 addr)
{
	if (addr == EEPROM_ADDR_SWITCH_ONOFF)
	{
		return s_tStorage.switch_onoff;
	}
	if (addr == EEPROM_ADDR_SYS_FUNC_SELECT)
	{
		return s_tStorage.sys_func_select[0];
	}
	if (addr == (EEPROM_ADDR_SYS_FUNC_SELECT + 2U))
	{
		return s_tStorage.sys_func_select[1];
	}
	if (addr == EEPROM_ADDR_PASS)
	{
		return s_tStorage.pass_flag;
	}
	if (addr == EEPROM_ADDR_SLEEP)
	{
		return s_tStorage.sleep_flag;
	}
	if (addr == EEPROM_ADDR_FLASHUPDATE)
	{
		return s_tStorage.flash_update_flag;
	}
	return 0xFFFFU;
}

static UINT8 EEPROM_WriteSpecialWord(UINT16 addr, UINT16 data)
{
	UINT16 *target;

	target = 0;
	if (addr == EEPROM_ADDR_SWITCH_ONOFF)
	{
		target = &s_tStorage.switch_onoff;
	}
	else if (addr == EEPROM_ADDR_SYS_FUNC_SELECT)
	{
		target = &s_tStorage.sys_func_select[0];
	}
	else if (addr == (EEPROM_ADDR_SYS_FUNC_SELECT + 2U))
	{
		target = &s_tStorage.sys_func_select[1];
	}
	else if (addr == EEPROM_ADDR_PASS)
	{
		target = &s_tStorage.pass_flag;
	}
	else if (addr == EEPROM_ADDR_SLEEP)
	{
		target = &s_tStorage.sleep_flag;
	}
	else if (addr == EEPROM_ADDR_FLASHUPDATE)
	{
		target = &s_tStorage.flash_update_flag;
	}

	if (target == 0)
	{
		return 1;
	}

	if (*target != data)
	{
		*target = data;
		EEPROM_MarkDirty();
	}

	return 0;
}

static UINT8 EEPROM_ReadByteInternal(UINT16 addr, UINT8 *value)
{
	if (value == 0)
	{
		return 0;
	}

	if (EEPROM_IsRawAddr(addr))
	{
		*value = s_tStorage.raw[addr];
		return 1;
	}

	if (EEPROM_IsSpecialWordAddr((UINT16)(addr & 0xFFFEU)))
	{
		UINT16 word = EEPROM_ReadSpecialWord((UINT16)(addr & 0xFFFEU));
		*value = (UINT8)(((addr & 1U) == 0U) ? (word & 0x00FFU) : (word >> 8));
		return 1;
	}

	*value = 0xFFU;
	return 0;
}

static UINT8 EEPROM_WriteByteInternal(UINT16 addr, UINT8 value)
{
	UINT8 old_value;
	UINT16 word_addr;
	UINT16 word_value;
	UINT8 changed;

	word_addr = (UINT16)(addr & 0xFFFEU);
	changed = 0;

	if (EEPROM_IsRawAddr(addr))
	{
		old_value = s_tStorage.raw[addr];
		if (old_value != value)
		{
			s_tStorage.raw[addr] = value;
			EEPROM_MarkDirty();
		}
		return 0;
	}

	if (!EEPROM_IsSpecialWordAddr(word_addr))
	{
		return 1;
	}

	word_value = EEPROM_ReadSpecialWord(word_addr);
	if ((addr & 1U) == 0U)
	{
		old_value = (UINT8)(word_value & 0x00FFU);
		if (old_value != value)
		{
			word_value = (UINT16)((word_value & 0xFF00U) | value);
			changed = 1;
		}
	}
	else
	{
		old_value = (UINT8)(word_value >> 8);
		if (old_value != value)
		{
			word_value = (UINT16)((word_value & 0x00FFU) | ((UINT16)value << 8));
			changed = 1;
		}
	}

	if (!changed)
	{
		return 0;
	}

	return EEPROM_WriteSpecialWord(word_addr, word_value);
}

static FLASH_Status EEPROM_FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0; offset < EEPROM_FLASH_PAGE_SIZE; offset += 2U)
	{
		if (*((volatile UINT16 *)(page_addr + offset)) != 0xFFFFU)
		{
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status EEPROM_FlashProgramHalfWordVerified(uint32_t addr, UINT16 data)
{
	FLASH_Status result;

	result = FLASH_ProgramHalfWord(addr, data);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	if (*((volatile UINT16 *)addr) != data)
	{
		return FLASH_ERROR_PG;
	}

	return FLASH_COMPLETE;
}

static FLASH_Status EEPROM_FlashProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset;
	UINT16 half_word;
	FLASH_Status result;

	offset = 0;
	result = FLASH_COMPLETE;

	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1U) < length)
		{
			half_word |= ((UINT16)data[offset + 1U] << 8);
		}
		else
		{
			half_word |= 0xFF00U;
		}

		result = EEPROM_FlashProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE)
		{
			return result;
		}
		offset += 2U;
	}

	return result;
}

static UINT8 EEPROM_LoadSlot(uint32_t slot_addr,
							 EEPROM_FLASH_PAYLOAD *payload,
							 UINT32 *sequence)
{
	const EEPROM_FLASH_HEADER *header;
	const UINT8 *payload_ptr;
	UINT16 crc;

	header = (const EEPROM_FLASH_HEADER *)slot_addr;
	if ((header->magic != EEPROM_FLASH_MAGIC) ||
		(header->storage_version != EEPROM_FLASH_STORAGE_VERSION) ||
		(header->payload_size != sizeof(EEPROM_FLASH_PAYLOAD)))
	{
		return 0;
	}

	payload_ptr = (const UINT8 *)(slot_addr + sizeof(EEPROM_FLASH_HEADER));
	crc = EEPROM_CalcCrc16(payload_ptr, header->payload_size);
	if (crc != header->crc)
	{
		return 0;
	}

	if (payload != 0)
	{
		memcpy(payload, payload_ptr, sizeof(EEPROM_FLASH_PAYLOAD));
	}
	if (sequence != 0)
	{
		*sequence = header->sequence;
	}
	return 1;
}

static FLASH_Status EEPROM_WriteSlot(uint32_t slot_addr,
									 const EEPROM_FLASH_PAYLOAD *payload,
									 UINT32 sequence)
{
	EEPROM_FLASH_HEADER header;
	FLASH_Status result;

	header.magic = EEPROM_FLASH_MAGIC;
	header.storage_version = EEPROM_FLASH_STORAGE_VERSION;
	header.payload_size = (UINT16)sizeof(EEPROM_FLASH_PAYLOAD);
	header.sequence = sequence;
	header.crc = EEPROM_CalcCrc16((const UINT8 *)payload, (UINT16)sizeof(EEPROM_FLASH_PAYLOAD));
	header.reserved = 0xFFFFU;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

	result = EEPROM_FlashErasePageVerified(slot_addr);
	if (result == FLASH_COMPLETE)
	{
		result = EEPROM_FlashErasePageVerified(slot_addr + EEPROM_FLASH_PAGE_SIZE);
	}
	if (result == FLASH_COMPLETE)
	{
		result = EEPROM_FlashProgramBytesVerified(slot_addr, (const UINT8 *)&header, (UINT16)sizeof(header));
	}
	if (result == FLASH_COMPLETE)
	{
		result = EEPROM_FlashProgramBytesVerified(slot_addr + sizeof(header),
												  (const UINT8 *)payload,
												  (UINT16)sizeof(EEPROM_FLASH_PAYLOAD));
	}

	FLASH_Lock();
	return result;
}

static UINT8 EEPROM_SaveSnapshotNow(void)
{
	EEPROM_FLASH_PAYLOAD current_a;
	EEPROM_FLASH_PAYLOAD current_b;
	EEPROM_FLASH_PAYLOAD verify;
	UINT32 seq_a;
	UINT32 seq_b;
	UINT32 next_sequence;
	UINT32 target_slot;
	UINT8 valid_a;
	UINT8 valid_b;
	const EEPROM_FLASH_PAYLOAD *current;

	valid_a = EEPROM_LoadSlot(EEPROM_FLASH_SLOT_A_ADDR, &current_a, &seq_a);
	valid_b = EEPROM_LoadSlot(EEPROM_FLASH_SLOT_B_ADDR, &current_b, &seq_b);
	current = 0;
	target_slot = EEPROM_FLASH_SLOT_A_ADDR;
	next_sequence = 1U;

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			current = &current_a;
			target_slot = EEPROM_FLASH_SLOT_B_ADDR;
			next_sequence = seq_a + 1U;
		}
		else
		{
			current = &current_b;
			target_slot = EEPROM_FLASH_SLOT_A_ADDR;
			next_sequence = seq_b + 1U;
		}
	}
	else if (valid_a)
	{
		current = &current_a;
		target_slot = EEPROM_FLASH_SLOT_B_ADDR;
		next_sequence = seq_a + 1U;
	}
	else if (valid_b)
	{
		current = &current_b;
		target_slot = EEPROM_FLASH_SLOT_A_ADDR;
		next_sequence = seq_b + 1U;
	}

	if ((current != 0) && (memcmp(current, &s_tStorage, sizeof(EEPROM_FLASH_PAYLOAD)) == 0))
	{
		s_u8StorageDirty = 0;
		s_u8StorageCommitDelay = 0;
		return 1;
	}

	if (EEPROM_WriteSlot(target_slot, &s_tStorage, next_sequence) != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if (!EEPROM_LoadSlot(target_slot, &verify, 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}
	if (memcmp(&verify, &s_tStorage, sizeof(EEPROM_FLASH_PAYLOAD)) != 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	s_u8StorageDirty = 0;
	s_u8StorageCommitDelay = 0;
	return 1;
}

static UINT8 EEPROM_LoadSnapshotFromFlash(void)
{
	EEPROM_FLASH_PAYLOAD data_a;
	EEPROM_FLASH_PAYLOAD data_b;
	UINT32 seq_a;
	UINT32 seq_b;
	UINT8 valid_a;
	UINT8 valid_b;

	valid_a = EEPROM_LoadSlot(EEPROM_FLASH_SLOT_A_ADDR, &data_a, &seq_a);
	valid_b = EEPROM_LoadSlot(EEPROM_FLASH_SLOT_B_ADDR, &data_b, &seq_b);

	if (!valid_a && !valid_b)
	{
		return 0;
	}

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			s_tStorage = data_a;
		}
		else
		{
			s_tStorage = data_b;
		}
	}
	else if (valid_a)
	{
		s_tStorage = data_a;
	}
	else
	{
		s_tStorage = data_b;
	}

	s_u8StorageDirty = 0;
	s_u8StorageCommitDelay = 0;
	return 1;
}

static void InitData_E2prom(void)
{
	if (!EEPROM_LoadSnapshotFromFlash())
	{
		EEPROM_ResetData_AllToDefault();
		while (u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag ||
			   u32E2P_Pro_Other_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag)
		{
			WriteEEPROM_ByteData_Circle();
		}
		EEPROM_ResetData_OtherToDefault();
		WriteProID_Default();
		WriteEEPROM_Word_NoZone(812, 0xFFFFU);
		WriteEEPROM_Word_NoZone(EEPROM_ADDR_PASS, EEPROM_VALUE_BEGIN_FLAG);
		WriteEEPROM_Word_NoZone(EEPROM_ADDR_SLEEP, EEPROM_VALUE_SLEEP_RESET);
		WriteEEPROM_Word_NoZone(EEPROM_ADDR_FLASHUPDATE, EEPROM_VALUE_FLASHUPDATE_RESET);
		(void)EEPROM_SaveSnapshotNow();
		return;
	}

	ReadEEPROM_ByteData_StartUp();
	ReadEEPROM_EventRecord_Parameters();
	(void)EEPROM_SaveSnapshotNow();
}

UINT8 ReadEEPROM_Byte(UINT16 addr)
{
	UINT8 value;

	Feed_IWatchDog;
	if (!EEPROM_ReadByteInternal(addr, &value))
	{
		return 0xFFU;
	}
	return value;
}

UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val)
{
	Feed_IWatchDog;
	if (EEPROM_WriteByteInternal(addr, val) != 0)
	{
		return 1;
	}
	return 0;
}

UINT16 ReadEEPROM_Word_NoZone(UINT16 addr)
{
	UINT8 low;
	UINT8 high;

	low = ReadEEPROM_Byte(addr);
	high = ReadEEPROM_Byte((UINT16)(addr + 1U));
	return (UINT16)((UINT16)high << 8 | low);
}

UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)
{
	UINT8 result;
	UINT8 low;
	UINT8 high;
	UINT16 verify;
	UINT8 retry;

	result = 0;
	retry = 0;
	do
	{
		result |= WriteEEPROM_Byte(addr, (UINT8)(data & 0x00FFU));
		result |= WriteEEPROM_Byte((UINT16)(addr + 1U), (UINT8)(data >> 8));
		low = ReadEEPROM_Byte(addr);
		high = ReadEEPROM_Byte((UINT16)(addr + 1U));
		verify = (UINT16)(((UINT16)high << 8) | low);
		retry++;
		if (retry > 2U || result != 0)
		{
			result++;
			break;
		}
	} while (verify != data);

	return result;
}

void ReadEEPROM_ByteData_StartUp(void)
{
	UINT16 i;
	UINT16 t_u16RdTemp;
	INT16 t_i16RdTemp;
	UINT16 t_u16TempMax;
	UINT16 t_u16TempMin;

	const struct PRT_E2ROM_PARAS PrtE2paras_Min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS PrtE2paras_Max = E2P_PROTECT_MAX_PRT;
	const struct PRT_E2ROM_PARAS PrtE2paras_Pos = E2P_ADDR_E2POS_PROTECT;

	const struct OTHER_ELEMENT OtherElement_to_Max = OtherElement_max;
	const struct OTHER_ELEMENT OtherElement_to_Min = OtherElement_min;
	const struct OTHER_ELEMENT OtherElement_to_Pos = E2P_ADDR_E2POS_OTHER_ELEMENT1;

	const struct HEAT_COOL_ELEMENT HeatCoolEle_Max = HeatCoolElement_Max;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Min = HeatCoolElement_Min;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Pos = E2P_ADDR_E2POS_HEAT_COOL;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i));
		t_u16TempMax = *(&PrtE2paras_Max.u16VcellOvp_First + i);
		t_u16TempMin = *(&PrtE2paras_Min.u16VcellOvp_First + i);
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = t_u16RdTemp;
		if ((t_u16RdTemp < t_u16TempMin) || (t_u16RdTemp > t_u16TempMax))
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_CALIB_K; ++i)
	{
		t_u16RdTemp = ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_K + (i << 1));
		g_u16CalibCoefK[i] = t_u16RdTemp;
		if ((t_u16RdTemp < SYSKMIN) || (t_u16RdTemp > SYSKMAX))
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}

		t_i16RdTemp = (INT16)ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_B + (i << 1));
		g_i16CalibCoefB[i] = t_i16RdTemp;
		if ((t_i16RdTemp < SYSBMIN) || (t_i16RdTemp > SYSBMAX))
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&OtherElement_to_Pos.u16Balance_OpenVoltage + i));
		t_u16TempMax = *(&OtherElement_to_Max.u16Balance_OpenVoltage + i);
		t_u16TempMin = *(&OtherElement_to_Min.u16Balance_OpenVoltage + i);
		*(&OtherElement.u16Balance_OpenVoltage + i) = t_u16RdTemp;
		if ((t_u16RdTemp < t_u16TempMin) || (t_u16RdTemp > t_u16TempMax))
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i));
		t_u16TempMax = *(&HeatCoolEle_Max.u16Heat_OpenTemp + i);
		t_u16TempMin = *(&HeatCoolEle_Min.u16Heat_OpenTemp + i);
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = t_u16RdTemp;
		if ((t_u16RdTemp < t_u16TempMin) || (t_u16RdTemp > t_u16TempMax))
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}
}

void EEPROM_ResetData_AllToDefault(void)
{
	const struct PRT_E2ROM_PARAS PrtE2PARAS_Default = E2P_PROTECT_DEFAULT_PRT;
	const struct OTHER_ELEMENT OtherElement_Default = OtherElement_default;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Default = HeatCoolElement_Default;
	UINT8 i;

	EEPROM_ResetShadowBlank();

	for (i = 0; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
	u8E2P_KB_WriteFlag = KB_NUM;
	u8E2P_KB_WritePos = 0;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&PrtE2PARAS_Default.u16VcellOvp_First + i);
	}
	u32E2P_Pro_VolCur_WriteFlag = E2P_PARA_ALL_VOLCUR_PROTECT;
	u32E2P_Pro_Temp_WriteFlag = E2P_PARA_ALL_TEM_PROTECT;
	u32E2P_Pro_Other_WriteFlag = E2P_PARA_ALL_OTHER_PROTECT;

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&OtherElement_Default.u16Balance_OpenVoltage + i);
	}
	u32E2P_OtherElement1_WriteFlag = E2P_PARA_ALL_OTHER_ELEMENT1;

	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = *(&HeatCoolEle_Default.u16Heat_OpenTemp + i);
	}
	u32E2P_HeatCool_WriteFlag = E2P_PARA_ALL_HEAT_COOL_ELE;
}

void EEPROM_ResetData_OtherToDefault(void)
{
	EEPROM_ResetData_EventRecord_ToDefault();
	SystemMonitorResetData_EEPROM();
}

void WriteEEPROM_ByteData_Circle(void)
{
	UINT8 i;
	UINT8 u8temp;
	const struct PRT_E2ROM_PARAS PrtE2paras_Pos = E2P_ADDR_E2POS_PROTECT;
	const struct OTHER_ELEMENT OtherCanAdd_Pos = E2P_ADDR_E2POS_OTHER_ELEMENT1;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Pos = E2P_ADDR_E2POS_HEAT_COOL;

	i = 0;
	if (u8E2P_KB_WriteFlag)
	{
		WriteEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_CALIB_K + (u8E2P_KB_WritePos << 1)), g_u16CalibCoefK[u8E2P_KB_WritePos]);
		WriteEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_CALIB_B + (u8E2P_KB_WritePos << 1)), g_i16CalibCoefB[u8E2P_KB_WritePos]);
		++u8E2P_KB_WritePos;
		--u8E2P_KB_WriteFlag;
	}
	else if (u32E2P_Pro_VolCur_WriteFlag & E2P_PARA_ALL_VOLCUR_PROTECT)
	{
		while (i < E2P_PARA_ALL_VOLCUR_PROTECT)
		{
			if ((u32E2P_Pro_VolCur_WriteFlag >> i) & 1U)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i), *(&PRT_E2ROMParas.u16VcellOvp_First + i));
				u32E2P_Pro_VolCur_WriteFlag -= ((UINT32)1 << i);
				break;
			}
			++i;
		}
	}
	else if (u32E2P_Pro_Temp_WriteFlag & E2P_PARA_ALL_TEM_PROTECT)
	{
		while (i < E2P_PARA_ALL_TEM_PROTECT)
		{
			if ((u32E2P_Pro_Temp_WriteFlag >> i) & 1U)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16TChgOTp_First + i), *(&PRT_E2ROMParas.u16TChgOTp_First + i));
				u32E2P_Pro_Temp_WriteFlag -= ((UINT32)1 << i);
				break;
			}
			++i;
		}
	}
	else if (u32E2P_Pro_Other_WriteFlag & E2P_PARA_ALL_OTHER_PROTECT)
	{
		while (i < E2P_PARA_ALL_OTHER_PROTECT)
		{
			if ((u32E2P_Pro_Other_WriteFlag >> i) & 1U)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VdeltaOvp_First + i), *(&PRT_E2ROMParas.u16VdeltaOvp_First + i));
				u32E2P_Pro_Other_WriteFlag -= ((UINT32)1 << i);
				break;
			}
			++i;
		}
	}
	else if (u32E2P_OtherElement1_WriteFlag & E2P_PARA_ALL_OTHER_ELEMENT1)
	{
		while (i < E2P_PARA_ALL_OTHER_ELEMENT1)
		{
			if ((u32E2P_OtherElement1_WriteFlag >> i) & 1U)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&OtherCanAdd_Pos.u16Balance_OpenVoltage + i), *(&OtherElement.u16Balance_OpenVoltage + i));
				u32E2P_OtherElement1_WriteFlag -= ((UINT32)1 << i);
				break;
			}
			++i;
		}
	}
	else if (u32E2P_HeatCool_WriteFlag)
	{
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			if ((u32E2P_HeatCool_WriteFlag >> i) & 1U)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i), *(&Heat_Cool_Element.u16Heat_OpenTemp + i));
				u32E2P_HeatCool_WriteFlag -= ((UINT32)1 << i);
				break;
			}
		}
	}
	else if (gu8_Reset_EventRecord)
	{
		u8temp = (UINT8)(100U - gu8_Reset_EventRecord);
		WriteEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_EVENT_RECORD + (u8temp << 1)), 0x0000U);
		--gu8_Reset_EventRecord;
		if (gu8_Reset_EventRecord == 1U)
		{
			WriteEEPROM_Word_NoZone(E2P_ADDR_E2POS_EVENT_POINT, 0x0000U);
		}
	}
}

void InitE2PROM(void)
{
	InitData_E2prom();
}

void App_E2promDeal(void)
{
	if (u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag ||
		u32E2P_Pro_Other_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag)
	{
		WriteEEPROM_ByteData_Circle();
	}

	if (gu8_Reset_EventRecord)
	{
		WriteEEPROM_ByteData_Circle();
	}

	if (s_u8StorageDirty)
	{
		if (++s_u8StorageCommitDelay >= EEPROM_STORAGE_COMMIT_DELAY_TICKS)
		{
			(void)EEPROM_SaveSnapshotNow();
		}
	}
}

void EEPROM_ResetData_EventRecord_ToDefault(void)
{
	UINT8 i;

	for (i = 0; i < EEPROM_EVENT_RECORD_LENGTH; ++i)
	{
		BMS_LOG_RECORD[i][0] = 0;
		BMS_LOG_RECORD[i][1] = 0;
	}
	BMS_LOG_POINT = 0;

	for (i = 0; i < EEPROM_EVENT_RECORD_LENGTH; ++i)
	{
		WriteEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_EVENT_RECORD + (i << 1)), 0x0000U);
	}
	WriteEEPROM_Word_NoZone(E2P_ADDR_E2POS_EVENT_POINT, BMS_LOG_POINT);
}

void ReadEEPROM_EventRecord_Parameters(void)
{
	UINT8 i;
	UINT16 t_u16RdTemp;

	BMS_LOG_POINT = ReadEEPROM_Word_NoZone(E2P_ADDR_E2POS_EVENT_POINT);
	if (BMS_LOG_POINT >= (EEPROM_EVENT_RECORD_LENGTH + 1U))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		EEPROM_ResetData_EventRecord_ToDefault();
	}

	for (i = 0; i < EEPROM_EVENT_RECORD_LENGTH; ++i)
	{
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_EVENT_RECORD + (i << 1)));
		if (((t_u16RdTemp & 0x00FFU) <= EVENT_NUM) && ((t_u16RdTemp >> 8) <= 171U))
		{
			BMS_LOG_RECORD[i][0] = (UINT8)(t_u16RdTemp & 0x00FFU);
			BMS_LOG_RECORD[i][1] = (UINT8)(t_u16RdTemp >> 8);
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
			BMS_LOG_RECORD[i][0] = 0;
			BMS_LOG_RECORD[i][1] = 0;
			WriteEEPROM_Word_NoZone((UINT16)(E2P_ADDR_START_EVENT_RECORD + (i << 1)), 0x0000U);
		}
	}
}
