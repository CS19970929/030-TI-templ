#include "param.h"
#include "stm32f0xx_flash.h"

typedef struct
{
	UINT32 magic;
	UINT16 storage_version;
	UINT16 payload_size;
	UINT32 sequence;
	UINT16 crc;
	UINT16 reserved;
} PARAM_FLASH_HEADER;

typedef char ParamStorageSizeCheck1[(sizeof(PARAM_T) < 256U) ? 1 : -1];
typedef char ParamStorageSizeCheck2[
	((sizeof(PARAM_T) + sizeof(PARAM_FLASH_HEADER) + 1U) <= PARAM_FLASH_PAGE_SIZE) ? 1 : -1];

#define PARAM_FLASH_MAGIC               ((UINT32)0x5041524DU) /* "PARM" */

PARAM_T g_tParam;

static UINT16 ParamFlash_CalcCrc(const void *data, UINT16 length)
{
	if ((data == 0) || (length == 0))
	{
		return 0xFFFFU;
	}

	return Sci_CRC16RTU((UINT8 *)data, (UINT8)length);
}

static FLASH_Status ParamFlash_ErasePageVerified(uint32_t page_addr)
{
	UINT16 offset;
	FLASH_Status result;

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0; offset < PARAM_FLASH_PAGE_SIZE; offset += 2U)
	{
		if (*((volatile UINT16 *)(page_addr + offset)) != 0xFFFFU)
		{
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status ParamFlash_ProgramHalfWordVerified(uint32_t addr, UINT16 data)
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

static FLASH_Status ParamFlash_ProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset = 0;
	UINT16 half_word;
	FLASH_Status result = FLASH_COMPLETE;

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

		result = ParamFlash_ProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE)
		{
			return result;
		}

		offset += 2U;
	}

	return result;
}

static FLASH_Status ParamFlash_WriteSlot(uint32_t slot_addr, const PARAM_T *payload, UINT32 sequence)
{
	PARAM_FLASH_HEADER header;
	FLASH_Status result;

	header.magic = PARAM_FLASH_MAGIC;
	header.storage_version = PARAM_FLASH_STORAGE_VERSION;
	header.payload_size = (UINT16)sizeof(PARAM_T);
	header.sequence = sequence;
	header.crc = ParamFlash_CalcCrc(payload, (UINT16)sizeof(PARAM_T));
	header.reserved = 0xFFFFU;

	result = ParamFlash_ErasePageVerified(slot_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	result = ParamFlash_ProgramBytesVerified(slot_addr, (const UINT8 *)&header, (UINT16)sizeof(header));
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	result = ParamFlash_ProgramBytesVerified(slot_addr + sizeof(header), (const UINT8 *)payload,
											 (UINT16)sizeof(PARAM_T));
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	return FLASH_COMPLETE;
}

static UINT8 ParamFlash_ReadSlot(uint32_t slot_addr, PARAM_T *payload, UINT32 *sequence)
{
	const PARAM_FLASH_HEADER *header = (const PARAM_FLASH_HEADER *)slot_addr;

	if (header->magic != PARAM_FLASH_MAGIC)
	{
		return 0;
	}

	if (header->storage_version != PARAM_FLASH_STORAGE_VERSION)
	{
		return 0;
	}

	if (header->payload_size != sizeof(PARAM_T))
	{
		return 0;
	}

	if (header->crc != ParamFlash_CalcCrc((const void *)(slot_addr + sizeof(PARAM_FLASH_HEADER)),
										  header->payload_size))
	{
		return 0;
	}

	if (payload != 0)
	{
		memcpy(payload, (const void *)(slot_addr + sizeof(PARAM_FLASH_HEADER)), sizeof(PARAM_T));
	}

	if (sequence != 0)
	{
		*sequence = header->sequence;
	}

	return 1;
}

static UINT8 ParamFlash_Load(PARAM_T *payload, UINT32 *sequence)
{
	PARAM_T payload_a;
	PARAM_T payload_b;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT8 valid_a;
	UINT8 valid_b;

	valid_a = ParamFlash_ReadSlot(PARAM_FLASH_SLOT_A_ADDR, &payload_a, &seq_a);
	valid_b = ParamFlash_ReadSlot(PARAM_FLASH_SLOT_B_ADDR, &payload_b, &seq_b);

	if (!valid_a && !valid_b)
	{
		return 0;
	}

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			*payload = payload_a;
			if (sequence != 0)
			{
				*sequence = seq_a;
			}
		}
		else
		{
			*payload = payload_b;
			if (sequence != 0)
			{
				*sequence = seq_b;
			}
		}
		return 1;
	}

	if (valid_a)
	{
		*payload = payload_a;
		if (sequence != 0)
		{
			*sequence = seq_a;
		}
		return 1;
	}

	*payload = payload_b;
	if (sequence != 0)
	{
		*sequence = seq_b;
	}
	return 1;
}

static UINT8 ParamFlash_Save(const PARAM_T *payload)
{
	PARAM_T current_a;
	PARAM_T current_b;
	PARAM_T verify;
	const PARAM_T *current = 0;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT32 next_seq = 1U;
	uint32_t target_slot = PARAM_FLASH_SLOT_A_ADDR;
	UINT8 valid_a;
	UINT8 valid_b;
	FLASH_Status result;

	valid_a = ParamFlash_ReadSlot(PARAM_FLASH_SLOT_A_ADDR, &current_a, &seq_a);
	valid_b = ParamFlash_ReadSlot(PARAM_FLASH_SLOT_B_ADDR, &current_b, &seq_b);

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			current = &current_a;
			next_seq = seq_a + 1U;
			target_slot = PARAM_FLASH_SLOT_B_ADDR;
		}
		else
		{
			current = &current_b;
			next_seq = seq_b + 1U;
			target_slot = PARAM_FLASH_SLOT_A_ADDR;
		}
	}
	else if (valid_a)
	{
		current = &current_a;
		next_seq = seq_a + 1U;
		target_slot = PARAM_FLASH_SLOT_B_ADDR;
	}
	else if (valid_b)
	{
		current = &current_b;
		next_seq = seq_b + 1U;
		target_slot = PARAM_FLASH_SLOT_A_ADDR;
	}

	if ((current != 0) && (memcmp(current, payload, sizeof(PARAM_T)) == 0))
	{
		return 1;
	}

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	result = ParamFlash_WriteSlot(target_slot, payload, next_seq);
	FLASH_Lock();
	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if (!ParamFlash_ReadSlot(target_slot, &verify, 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if (memcmp(&verify, payload, sizeof(PARAM_T)) != 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return 1;
}

static void ParamFlash_SetDefault(PARAM_T *param)
{
	memset(param, 0, sizeof(*param));
	param->ParamVer = PARAM_VER;
	param->protect = E2P_PROTECT_DEFAULT_PRT;
	param->other = OtherElement_default;
	param->heat = HeatCoolElement_Default;
}

void LoadParam(void)
{
	UINT32 sequence = 0;

	sys_time.test_sizeof_g_tParam = sizeof(g_tParam);

	if (ParamFlash_Load(&g_tParam, &sequence) && (g_tParam.ParamVer == PARAM_VER))
	{
		return;
	}

	ParamFlash_SetDefault(&g_tParam);
	(void)ParamFlash_Save(&g_tParam);
}

void SaveParam(void)
{
	PARAM_T verify;
	UINT32 sequence = 0;

	g_tParam.ParamVer = PARAM_VER;
	if (!ParamFlash_Save(&g_tParam))
	{
		return;
	}

	if (ParamFlash_Load(&verify, &sequence) && (verify.ParamVer == PARAM_VER))
	{
		g_tParam = verify;
	}
}
