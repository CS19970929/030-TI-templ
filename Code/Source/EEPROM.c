#include "main.h"

volatile uint32_t sEETimeout = sEE_LONG_TIMEOUT;

UINT32 u32E2P_Pro_VolCur_WriteFlag = 0;
UINT32 u32E2P_Pro_Temp_WriteFlag = 0;
UINT32 u32E2P_Pro_Other_WriteFlag = 0;
UINT32 u32E2P_RTC_Element_WriteFlag = 0;
UINT32 u32E2P_OtherElement1_WriteFlag = 0;
UINT32 u32E2P_HeatCool_WriteFlag = 0;

UINT8 u8E2P_SocTable_WriteFlag = 0;
UINT8 u8E2P_CopperLoss_WriteFlag = 0;
UINT8 u8E2P_KB_WriteFlag = 0;

UINT8 u8E2P_KB_WritePos = 0;

void InitData_E2prom(void);

#define EEPROM_STORAGE_LOW_BYTES              ((UINT16)2048)
#define EEPROM_STORAGE_LOW_WORDS              ((UINT16)(EEPROM_STORAGE_LOW_BYTES / 2u))
#define EEPROM_STORAGE_SPECIAL_WORDS          ((UINT8)3)
#define EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS  ((UINT16)6)
#define EEPROM_STORAGE_SNAPSHOT_PAYLOAD_WORDS ((UINT16)(EEPROM_STORAGE_LOW_WORDS + EEPROM_STORAGE_SPECIAL_WORDS))
#define EEPROM_STORAGE_SNAPSHOT_TOTAL_WORDS   ((UINT16)(EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS + EEPROM_STORAGE_SNAPSHOT_PAYLOAD_WORDS))
#define EEPROM_STORAGE_JOURNAL_HEADER_WORDS   ((UINT16)8)
#define EEPROM_STORAGE_JOURNAL_ENTRY_WORDS    ((UINT16)5)
#define EEPROM_STORAGE_MAGIC_SNAPSHOT         ((UINT16)0xE2F1)
#define EEPROM_STORAGE_MAGIC_JOURNAL          ((UINT16)0xE2F2)
#define EEPROM_STORAGE_VERSION                ((UINT16)0x0001)
#define EEPROM_STORAGE_ENTRY_COMMIT           ((UINT16)0xA55A)
#define EEPROM_STORAGE_WRITE_FLAG_BYTE        ((UINT16)0x8000)
#define EEPROM_STORAGE_ADDR_MASK              ((UINT16)0x7FFF)
#define EEPROM_STORAGE_SNAPSHOT_COMMIT        ((UINT16)(~EEPROM_STORAGE_MAGIC_SNAPSHOT))
#define EEPROM_STORAGE_JOURNAL_COMMIT         ((UINT16)(~EEPROM_STORAGE_MAGIC_JOURNAL))

static UINT8 s_u8EepromStorageReady = 0;
static UINT16 s_u16EepromStorageSeq = 0;
static UINT8 s_u8EepromActiveSnapshotSlot = 0xFF;
static UINT16 s_u16EepromJournalNextWord = EEPROM_STORAGE_JOURNAL_HEADER_WORDS;

static UINT16 EEPROM_Storage_Crc16Step(UINT16 crc, UINT16 data)
{
	UINT8 i;

	crc ^= data;
	for (i = 0; i < 16; ++i)
	{
		if (crc & 1u)
		{
			crc = (UINT16)((crc >> 1) ^ 0xA001u);
		}
		else
		{
			crc >>= 1;
		}
	}

	return crc;
}

static UINT32 EEPROM_Storage_SnapshotBase(UINT8 slot)
{
	return (0u == slot) ? FLASH_ADDR_STORAGE_SLOT0 : FLASH_ADDR_STORAGE_SLOT1;
}

static UINT8 EEPROM_Storage_IsSpecialByteAddress(UINT16 addr)
{
	return (UINT8)((addr >= EEPROM_ADDR_SLEEP) && (addr <= (UINT16)(EEPROM_ADDR_FLASHUPDATE + 1u)));
}

static UINT8 EEPROM_Storage_GetSpecialIndex(UINT16 addr)
{
	return (UINT8)(((UINT16)(addr - EEPROM_ADDR_SLEEP)) >> 1);
}

static UINT16 EEPROM_Storage_CalcJournalCrc(UINT16 seq, UINT16 addr, UINT16 value);

static UINT8 EEPROM_Storage_ReadSnapshotByteByBase(UINT32 base, UINT16 addr)
{
	UINT16 word;

	if (addr < EEPROM_STORAGE_LOW_BYTES)
	{
		word = FlashReadOneHalfWord(base + ((UINT32)EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS << 1) + (addr & (UINT16)~1u));
		if (addr & 1u)
		{
			return (UINT8)(word >> 8);
		}
		return (UINT8)(word & 0x00FF);
	}

	if (EEPROM_Storage_IsSpecialByteAddress(addr))
	{
		word = FlashReadOneHalfWord(base + ((UINT32)(EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS + EEPROM_STORAGE_LOW_WORDS + EEPROM_Storage_GetSpecialIndex(addr)) << 1));
		if (addr & 1u)
		{
			return (UINT8)(word >> 8);
		}
		return (UINT8)(word & 0x00FF);
	}

	return 0xFF;
}

static UINT8 EEPROM_Storage_ReadJournalByte(UINT16 addr, UINT8 *value_out)
{
	UINT16 offset;
	UINT16 seq;
	UINT16 read_addr;
	UINT16 read_value;
	UINT16 crc;
	UINT16 commit;
	UINT16 entry_addr;

	offset = s_u16EepromJournalNextWord;
	while (offset > EEPROM_STORAGE_JOURNAL_HEADER_WORDS)
	{
		offset = (UINT16)(offset - EEPROM_STORAGE_JOURNAL_ENTRY_WORDS);
		seq = FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)offset << 1));
		read_addr = FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)(offset + 1u) << 1));
		read_value = FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)(offset + 2u) << 1));
		crc = FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)(offset + 3u) << 1));
		commit = FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)(offset + 4u) << 1));

		if (seq == 0xFFFFu || commit != EEPROM_STORAGE_ENTRY_COMMIT)
		{
			break;
		}

		if (crc != EEPROM_Storage_CalcJournalCrc(seq, read_addr, read_value))
		{
			break;
		}

		entry_addr = (UINT16)(read_addr & EEPROM_STORAGE_ADDR_MASK);
		if (read_addr & EEPROM_STORAGE_WRITE_FLAG_BYTE)
		{
			if (entry_addr == addr)
			{
				*value_out = (UINT8)(read_value & 0x00FF);
				return 1;
			}
		}
		else if ((UINT16)(entry_addr & (UINT16)~1u) == (UINT16)(addr & (UINT16)~1u))
		{
			if (addr & 1u)
			{
				*value_out = (UINT8)(read_value >> 8);
			}
			else
			{
				*value_out = (UINT8)(read_value & 0x00FF);
			}
			return 1;
		}
	}

	return 0;
}

static UINT8 EEPROM_Storage_ReadByteRaw(UINT16 addr)
{
	UINT8 value;

	if (EEPROM_Storage_ReadJournalByte(addr, &value))
	{
		return value;
	}

	if (s_u8EepromActiveSnapshotSlot <= 1u)
	{
		return EEPROM_Storage_ReadSnapshotByteByBase(EEPROM_Storage_SnapshotBase(s_u8EepromActiveSnapshotSlot), addr);
	}

	return 0xFF;
}

static UINT16 EEPROM_Storage_ReadWordRaw(UINT16 addr)
{
	UINT8 low;
	UINT8 high;

	low = EEPROM_Storage_ReadByteRaw(addr);
	high = EEPROM_Storage_ReadByteRaw((UINT16)(addr + 1u));
	return (UINT16)(low | ((UINT16)high << 8));
}

static UINT16 EEPROM_Storage_CalcSnapshotCrc(void)
{
	UINT16 crc = 0xFFFF;
	UINT16 i;
	UINT16 word;

	for (i = 0; i < EEPROM_STORAGE_LOW_WORDS; ++i)
	{
		word = EEPROM_Storage_ReadWordRaw((UINT16)(i << 1));
		crc = EEPROM_Storage_Crc16Step(crc, word);
	}

	for (i = 0; i < EEPROM_STORAGE_SPECIAL_WORDS; ++i)
	{
		word = EEPROM_Storage_ReadWordRaw((UINT16)(EEPROM_ADDR_SLEEP + (i << 1)));
		crc = EEPROM_Storage_Crc16Step(crc, word);
	}

	return crc;
}

static UINT16 EEPROM_Storage_CalcJournalCrc(UINT16 seq, UINT16 addr, UINT16 value)
{
	UINT16 crc = 0xFFFF;

	crc = EEPROM_Storage_Crc16Step(crc, seq);
	crc = EEPROM_Storage_Crc16Step(crc, addr);
	crc = EEPROM_Storage_Crc16Step(crc, value);
	return crc;
}

static FLASH_Status EEPROM_Storage_ProgramWord(UINT32 addr, UINT16 value)
{
	FLASH_Status status;

	status = FLASH_ProgramHalfWord(addr, value);
	if ((status == FLASH_COMPLETE) && (*(vu16 *)addr != value))
	{
		status = FLASH_ERROR_PROGRAM;
	}

	return status;
}

static FLASH_Status EEPROM_Storage_ErasePages(UINT32 base_addr, UINT32 page_count)
{
	UINT32 page;
	FLASH_Status status = FLASH_COMPLETE;
	UINT8 retry;

	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	for (page = 0; page < page_count; ++page)
	{
		retry = 0;
		do
		{
			status = FLASH_ErasePage(base_addr + (page * FLASH_STORAGE_PAGE_BYTES));
			++retry;
		} while (status != FLASH_COMPLETE && retry < 3);

		if (status != FLASH_COMPLETE)
		{
			break;
		}
	}

	return status;
}

static UINT8 EEPROM_Storage_LoadSnapshotSlot(UINT8 slot, UINT16 *seq_out)
{
	UINT32 base;
	UINT16 header_magic;
	UINT16 header_version;
	UINT16 header_payload_words;
	UINT16 header_seq;
	UINT16 header_crc;
	UINT16 header_commit;
	UINT16 crc;
	UINT16 i;
	UINT16 word;

	base = (0 == slot) ? FLASH_ADDR_STORAGE_SLOT0 : FLASH_ADDR_STORAGE_SLOT1;
	header_magic = FlashReadOneHalfWord(base);
	header_version = FlashReadOneHalfWord(base + 2u);
	header_payload_words = FlashReadOneHalfWord(base + 4u);
	header_seq = FlashReadOneHalfWord(base + 6u);
	header_crc = FlashReadOneHalfWord(base + 8u);
	header_commit = FlashReadOneHalfWord(base + 10u);

	if (header_magic != EEPROM_STORAGE_MAGIC_SNAPSHOT ||
		header_version != EEPROM_STORAGE_VERSION ||
		header_payload_words != EEPROM_STORAGE_SNAPSHOT_PAYLOAD_WORDS ||
		header_commit != EEPROM_STORAGE_SNAPSHOT_COMMIT)
	{
		return 0;
	}

	crc = 0xFFFF;
	for (i = 0; i < EEPROM_STORAGE_SNAPSHOT_PAYLOAD_WORDS; ++i)
	{
		word = FlashReadOneHalfWord(base + ((UINT32)(EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS + i) << 1));
		crc = EEPROM_Storage_Crc16Step(crc, word);
	}

	if (crc != header_crc)
	{
		return 0;
	}

	*seq_out = header_seq;
	return 1;
}

static UINT8 EEPROM_Storage_LoadLatestSnapshot(UINT16 *seq_out)
{
	UINT16 seq0;
	UINT16 seq1;
	UINT8 valid0;
	UINT8 valid1;

	valid0 = EEPROM_Storage_LoadSnapshotSlot(0, &seq0);
	valid1 = EEPROM_Storage_LoadSnapshotSlot(1, &seq1);
	if (valid0 && valid1)
	{
		if ((UINT16)(seq1 - seq0) < 0x8000u)
		{
			*seq_out = seq1;
			s_u8EepromActiveSnapshotSlot = 1;
		}
		else
		{
			*seq_out = seq0;
			s_u8EepromActiveSnapshotSlot = 0;
		}
		return 1;
	}

	if (valid0)
	{
		*seq_out = seq0;
		s_u8EepromActiveSnapshotSlot = 0;
		return 1;
	}

	if (valid1)
	{
		*seq_out = seq1;
		s_u8EepromActiveSnapshotSlot = 1;
		return 1;
	}

	return 0;
}

static UINT8 EEPROM_Storage_JournalHeaderValid(UINT16 *base_seq_out)
{
	UINT32 base;
	UINT16 magic;
	UINT16 version;
	UINT16 payload_words;
	UINT16 base_seq;
	UINT16 crc;
	UINT16 stored_crc;
	UINT16 commit;

	base = FLASH_ADDR_STORAGE_JOURNAL;
	magic = FlashReadOneHalfWord(base);
	version = FlashReadOneHalfWord(base + 2u);
	payload_words = FlashReadOneHalfWord(base + 4u);
	base_seq = FlashReadOneHalfWord(base + 6u);
	stored_crc = FlashReadOneHalfWord(base + 8u);
	commit = FlashReadOneHalfWord(base + 14u);

	if (magic != EEPROM_STORAGE_MAGIC_JOURNAL ||
		version != EEPROM_STORAGE_VERSION ||
		payload_words != (UINT16)((FLASH_STORAGE_JOURNAL_BYTES / 2u) - EEPROM_STORAGE_JOURNAL_HEADER_WORDS) ||
		commit != EEPROM_STORAGE_JOURNAL_COMMIT)
	{
		return 0;
	}

	crc = 0xFFFF;
	crc = EEPROM_Storage_Crc16Step(crc, magic);
	crc = EEPROM_Storage_Crc16Step(crc, version);
	crc = EEPROM_Storage_Crc16Step(crc, payload_words);
	crc = EEPROM_Storage_Crc16Step(crc, base_seq);
	if (crc != stored_crc)
	{
		return 0;
	}

	*base_seq_out = base_seq;
	return 1;
}

static UINT8 EEPROM_Storage_JournalIsEmpty(void)
{
	UINT16 i;

	for (i = 0; i < EEPROM_STORAGE_JOURNAL_HEADER_WORDS; ++i)
	{
		if (FlashReadOneHalfWord(FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)i << 1)) != 0xFFFFu)
		{
			return 0;
		}
	}

	return 1;
}

static UINT8 EEPROM_Storage_FormatJournalHeader(UINT16 base_seq)
{
	UINT16 header[EEPROM_STORAGE_JOURNAL_HEADER_WORDS];
	UINT16 crc;
	FLASH_Status status;

	FLASH_Unlock();
	header[0] = EEPROM_STORAGE_MAGIC_JOURNAL;
	header[1] = EEPROM_STORAGE_VERSION;
	header[2] = (UINT16)((FLASH_STORAGE_JOURNAL_BYTES / 2u) - EEPROM_STORAGE_JOURNAL_HEADER_WORDS);
	header[3] = base_seq;
	crc = 0xFFFF;
	crc = EEPROM_Storage_Crc16Step(crc, header[0]);
	crc = EEPROM_Storage_Crc16Step(crc, header[1]);
	crc = EEPROM_Storage_Crc16Step(crc, header[2]);
	crc = EEPROM_Storage_Crc16Step(crc, header[3]);
	header[4] = crc;
	header[5] = 0xFFFF;
	header[6] = 0xFFFF;
	header[7] = EEPROM_STORAGE_JOURNAL_COMMIT;

	status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 0u, header[0]);
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 2u, header[1]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 4u, header[2]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 6u, header[3]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 8u, header[4]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(FLASH_ADDR_STORAGE_JOURNAL + 14u, header[7]);
	}

	FLASH_Lock();
	if (status != FLASH_COMPLETE)
	{
		return 1;
	}

	s_u16EepromJournalNextWord = EEPROM_STORAGE_JOURNAL_HEADER_WORDS;
	return 0;
}

static UINT8 EEPROM_Storage_ReplayJournal(void)
{
	UINT32 base;
	UINT16 offset;
	UINT16 seq;
	UINT16 crc;
	UINT16 commit;
	UINT16 read_seq;
	UINT16 read_addr;
	UINT16 read_value;
	UINT16 latest_seq;

	base = FLASH_ADDR_STORAGE_JOURNAL;
	offset = EEPROM_STORAGE_JOURNAL_HEADER_WORDS;
	latest_seq = s_u16EepromStorageSeq;
	while ((UINT16)(offset + EEPROM_STORAGE_JOURNAL_ENTRY_WORDS) <= (UINT16)(FLASH_STORAGE_JOURNAL_BYTES / 2u))
	{
		read_seq = FlashReadOneHalfWord(base + ((UINT32)offset << 1));
		read_addr = FlashReadOneHalfWord(base + ((UINT32)(offset + 1u) << 1));
		read_value = FlashReadOneHalfWord(base + ((UINT32)(offset + 2u) << 1));
		crc = FlashReadOneHalfWord(base + ((UINT32)(offset + 3u) << 1));
		commit = FlashReadOneHalfWord(base + ((UINT32)(offset + 4u) << 1));

		if (read_seq == 0xFFFFu || commit != EEPROM_STORAGE_ENTRY_COMMIT)
		{
			break;
		}

		seq = read_seq;
		if (crc != EEPROM_Storage_CalcJournalCrc(seq, read_addr, read_value))
		{
			break;
		}

		if ((UINT16)(seq - s_u16EepromStorageSeq) < 0x8000u)
		{
			latest_seq = seq;
		}

		offset = (UINT16)(offset + EEPROM_STORAGE_JOURNAL_ENTRY_WORDS);
	}

	s_u16EepromStorageSeq = latest_seq;
	s_u16EepromJournalNextWord = offset;
	return 1;
}

static UINT8 EEPROM_Storage_WriteSnapshotToSlot(UINT8 slot, UINT16 sequence)
{
	UINT32 base;
	UINT32 page_count;
	UINT16 crc;
	UINT16 i;
	UINT16 word;
	FLASH_Status status;

	base = (0 == slot) ? FLASH_ADDR_STORAGE_SLOT0 : FLASH_ADDR_STORAGE_SLOT1;
	page_count = FLASH_STORAGE_SLOT_BYTES / FLASH_STORAGE_PAGE_BYTES;
	crc = EEPROM_Storage_CalcSnapshotCrc();

	FLASH_Unlock();
	status = EEPROM_Storage_ErasePages(base, page_count);
	if (status != FLASH_COMPLETE)
	{
		FLASH_Lock();
		return 1;
	}

	status = EEPROM_Storage_ProgramWord(base + 0u, EEPROM_STORAGE_MAGIC_SNAPSHOT);
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(base + 2u, EEPROM_STORAGE_VERSION);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(base + 4u, EEPROM_STORAGE_SNAPSHOT_PAYLOAD_WORDS);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(base + 6u, sequence);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(base + 8u, crc);
	}

	if (status == FLASH_COMPLETE)
	{
		for (i = 0; i < EEPROM_STORAGE_LOW_WORDS; ++i)
		{
			word = EEPROM_Storage_ReadWordRaw((UINT16)(i << 1));
			status = EEPROM_Storage_ProgramWord(base + ((UINT32)(EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS + i) << 1), word);
			if (status != FLASH_COMPLETE)
			{
				break;
			}
		}
	}

	if (status == FLASH_COMPLETE)
	{
		for (i = 0; i < EEPROM_STORAGE_SPECIAL_WORDS; ++i)
		{
			word = EEPROM_Storage_ReadWordRaw((UINT16)(EEPROM_ADDR_SLEEP + (i << 1)));
			status = EEPROM_Storage_ProgramWord(base + ((UINT32)(EEPROM_STORAGE_SNAPSHOT_HEADER_WORDS + EEPROM_STORAGE_LOW_WORDS + i) << 1), word);
			if (status != FLASH_COMPLETE)
			{
				break;
			}
		}
	}

	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(base + 10u, EEPROM_STORAGE_SNAPSHOT_COMMIT);
	}

	FLASH_Lock();
	return (status == FLASH_COMPLETE) ? 0 : 1;
}

static UINT8 EEPROM_Storage_CompactJournal(void)
{
	UINT8 next_slot;
	UINT16 new_seq;

	next_slot = (s_u8EepromActiveSnapshotSlot == 0u) ? 1u : 0u;
	new_seq = (UINT16)(s_u16EepromStorageSeq + 1u);
	if (EEPROM_Storage_WriteSnapshotToSlot(next_slot, new_seq))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 1;
	}

	s_u16EepromStorageSeq = new_seq;
	s_u8EepromActiveSnapshotSlot = next_slot;

	FLASH_Unlock();
	if (EEPROM_Storage_ErasePages(FLASH_ADDR_STORAGE_JOURNAL, FLASH_STORAGE_JOURNAL_BYTES / FLASH_STORAGE_PAGE_BYTES) != FLASH_COMPLETE)
	{
		s_u16EepromJournalNextWord = (UINT16)(FLASH_STORAGE_JOURNAL_BYTES / 2u);
		FLASH_Lock();
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 1;
	}

	if (EEPROM_Storage_FormatJournalHeader(s_u16EepromStorageSeq))
	{
		FLASH_Lock();
		s_u16EepromJournalNextWord = (UINT16)(FLASH_STORAGE_JOURNAL_BYTES / 2u);
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 1;
	}

	FLASH_Lock();
	return 0;
}

static UINT8 EEPROM_Storage_EnsureReady(void)
{
	UINT16 snapshot_seq;
	UINT16 journal_base_seq;

	if (s_u8EepromStorageReady)
	{
		return 0;
	}

	if (EEPROM_Storage_LoadLatestSnapshot(&snapshot_seq))
	{
		s_u16EepromStorageSeq = snapshot_seq;
	}

	if (EEPROM_Storage_JournalHeaderValid(&journal_base_seq))
	{
		if ((UINT16)(journal_base_seq - s_u16EepromStorageSeq) < 0x8000u)
		{
			s_u16EepromStorageSeq = journal_base_seq;
		}
	}

	EEPROM_Storage_ReplayJournal();

	if (EEPROM_Storage_JournalIsEmpty())
	{
		if (EEPROM_Storage_FormatJournalHeader(s_u16EepromStorageSeq))
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		}
	}

	s_u8EepromStorageReady = 1;
	return 0;
}

static UINT8 EEPROM_Storage_AppendJournal(UINT16 addr, UINT16 value, UINT8 is_byte)
{
	UINT16 entry_seq;
	UINT16 entry_addr;
	UINT16 entry_crc;
	UINT32 entry_base;
	UINT16 entry[EEPROM_STORAGE_JOURNAL_ENTRY_WORDS];
	FLASH_Status status;

	if (s_u16EepromJournalNextWord + EEPROM_STORAGE_JOURNAL_ENTRY_WORDS > (FLASH_STORAGE_JOURNAL_BYTES / 2u))
	{
		if (EEPROM_Storage_CompactJournal())
		{
			return 1;
		}
	}

	entry_seq = (UINT16)(s_u16EepromStorageSeq + 1u);
	entry_addr = (UINT16)(addr & EEPROM_STORAGE_ADDR_MASK);
	if (is_byte)
	{
		entry_addr |= EEPROM_STORAGE_WRITE_FLAG_BYTE;
	}
	entry_crc = EEPROM_Storage_CalcJournalCrc(entry_seq, entry_addr, value);
	entry[0] = entry_seq;
	entry[1] = entry_addr;
	entry[2] = value;
	entry[3] = entry_crc;
	entry[4] = EEPROM_STORAGE_ENTRY_COMMIT;
	entry_base = FLASH_ADDR_STORAGE_JOURNAL + ((UINT32)s_u16EepromJournalNextWord << 1);

	FLASH_Unlock();
	status = EEPROM_Storage_ProgramWord(entry_base + 0u, entry[0]);
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(entry_base + 2u, entry[1]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(entry_base + 4u, entry[2]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(entry_base + 6u, entry[3]);
	}
	if (status == FLASH_COMPLETE)
	{
		status = EEPROM_Storage_ProgramWord(entry_base + 8u, entry[4]);
	}

	FLASH_Lock();
	if (status != FLASH_COMPLETE)
	{
		return 1;
	}

	s_u16EepromStorageSeq = entry_seq;
	s_u16EepromJournalNextWord = (UINT16)(s_u16EepromJournalNextWord + EEPROM_STORAGE_JOURNAL_ENTRY_WORDS);
	return 0;
}

static UINT8 EEPROM_Storage_CommitWord(UINT16 addr, UINT16 value)
{
	UINT16 old_value;

	old_value = EEPROM_Storage_ReadWordRaw(addr);
	if (old_value == value)
	{
		return 0;
	}

	if (EEPROM_Storage_AppendJournal(addr, value, 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 1;
	}

	return 0;
}

static UINT8 EEPROM_Storage_CommitByte(UINT16 addr, UINT8 value)
{
	UINT8 old_value;

	old_value = EEPROM_Storage_ReadByteRaw(addr);
	if (old_value == value)
	{
		return 0;
	}

	if (EEPROM_Storage_AppendJournal(addr, value, 1))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 1;
	}

	return 0;
}


void IIC_Start_SEE(void)
{
	SDA_OUT_SEE(); // sda线输出
	IIC_SDA_SEE = 1;
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SDA_SEE = 0; // START:when CLK is high,DATA change form high to low
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0; // 钳住I2C总线，准备发送或接收数据
}

// 产生IIC停止信号
void IIC_Stop_SEE(void)
{
	SDA_OUT_SEE(); // sda线输出
	IIC_SCL_SEE = 0;
	IIC_SDA_SEE = 0; // STOP:when CLK is high DATA change form low to high
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SDA_SEE = 1; // 发送I2C总线结束信号
	__delay_us(DELAY_US_IIC_EEPROM);
}

// 等待应答信号到来
// 返回值：1，接收应答失败
//         0，接收应答成功
UINT8 IIC_Wait_Ack_SEE(void)
{
	UINT8 ucErrTime = 0;
	SDA_IN_SEE(); // SDA设置为输入
	// IIC_SDA_SEE=1;__delay_us(4);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	while (READ_SDA_SEE)
	{
		ucErrTime++;
		if (ucErrTime > 250)
		{
			IIC_Stop_SEE();
			return 1;
		}
	}
	IIC_SCL_SEE = 0; // 时钟输出0
	__delay_us(DELAY_US_IIC_EEPROM);
	return 0;
}

// 产生ACK应答
void IIC_Ack_SEE(void)
{
	IIC_SCL_SEE = 0;
	SDA_OUT_SEE();
	IIC_SDA_SEE = 0;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0;
}

// 不产生ACK应答
void IIC_NAck_SEE(void)
{
	IIC_SCL_SEE = 0;
	SDA_OUT_SEE();
	IIC_SDA_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0;
}

// IIC发送一个字节
// 返回从机有无应答
// 1，有应答
// 0，无应答
void IIC_Send_Byte_SEE(UINT8 txd)
{
	UINT8 t;
	SDA_OUT_SEE();
	IIC_SCL_SEE = 0; // 拉低时钟开始数据传输
	for (t = 0; t < 8; t++)
	{
		// IIC_SDA=(txd&0x80)>>7;
		if ((txd & 0x80) >> 7)
			IIC_SDA_SEE = 1;
		else
			IIC_SDA_SEE = 0;
		txd <<= 1;
		__delay_us(DELAY_US_IIC_EEPROM); // 对TEA5767这三个延时都是必须的
		IIC_SCL_SEE = 1;
		__delay_us(DELAY_US_IIC_EEPROM);
		IIC_SCL_SEE = 0;
		__delay_us(DELAY_US_IIC_EEPROM);
	}
}

// 读1个字节，ack=1时，发送ACK，ack=0，发送nACK
UINT8 IIC_Read_Byte_SEE(unsigned char ack)
{
	unsigned char i, receive = 0;
	SDA_IN_SEE(); // SDA设置为输入
	for (i = 0; i < 8; i++)
	{
		IIC_SCL_SEE = 0;
		__delay_us(DELAY_US_IIC_EEPROM);
		IIC_SCL_SEE = 1;
		receive <<= 1;
		if (READ_SDA_SEE)
			receive++;
		__delay_us(DELAY_US_IIC_EEPROM);
	}
	if (!ack)
		IIC_NAck_SEE(); // 发送nACK
	else
		IIC_Ack_SEE(); // 发送ACK
	return receive;
}

// 后续维护人员禁止使用这个函数
UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val)
{
	Feed_IWatchDog;
	EEPROM_Storage_EnsureReady();
	if (EEPROM_Storage_CommitByte(addr, val))
	{
		return 1;
	}
	Feed_IWatchDog;
	return 0;
}

UINT8 ReadEEPROM_Byte(UINT16 addr)
{
	Feed_IWatchDog;
	EEPROM_Storage_EnsureReady();
	return EEPROM_Storage_ReadByteRaw(addr);
}

UINT16 ReadEEPROM_Word_NoZone(UINT16 addr)
{
	Feed_IWatchDog;
	EEPROM_Storage_EnsureReady();
	return EEPROM_Storage_ReadWordRaw(addr);
}

// 主要调这个，加了几句话
UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)
{
	Feed_IWatchDog;
	EEPROM_Storage_EnsureReady();
	if (EEPROM_Storage_CommitWord(addr, data))
	{
		return 1;
	}
	Feed_IWatchDog;
	return 0;
}


void ReadEEPROM_ByteData_StartUp(void)
{
	UINT16 i;
	// UINT16  j;
	UINT16 t_u16RdTemp;
	INT16 t_i16RdTemp;
	UINT16 t_u16TempMax, t_u16TempMin;

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
	{ // 保护点
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i));
		t_u16TempMax = (*(&PrtE2paras_Max.u16VcellOvp_First + i));
		t_u16TempMin = (*(&PrtE2paras_Min.u16VcellOvp_First + i));
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{ // 这样其实不太好，最好的办法是把ReadEEPROM_Word_WithZone()这个函数改造以下
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;		//重新改造了一下这个函数，最后失败告终，不改好过改
				System_ERROR_UserCallback(ERROR_EEPROM_STORE); // 只要确保通讯没问题，就是这个错误。
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_CALIB_K; ++i)
	{ // K值
		t_u16RdTemp = ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_K + (i << 1));
		g_u16CalibCoefK[i] = t_u16RdTemp;
		if ((t_u16RdTemp >= SYSKMIN) && (t_u16RdTemp <= SYSKMAX))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}

		t_i16RdTemp = ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_B + (i << 1));
		g_i16CalibCoefB[i] = t_i16RdTemp; // B值
		if ((t_i16RdTemp >= SYSBMIN) && (t_i16RdTemp <= SYSBMAX))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{ // Other_CanAdd
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&OtherElement_to_Pos.u16Balance_OpenVoltage + i));
		t_u16TempMax = (*(&OtherElement_to_Max.u16Balance_OpenVoltage + i));
		t_u16TempMin = (*(&OtherElement_to_Min.u16Balance_OpenVoltage + i));
		*(&OtherElement.u16Balance_OpenVoltage + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{ // HeatCool_element
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i));
		t_u16TempMax = (*(&HeatCoolEle_Max.u16Heat_OpenTemp + i));
		t_u16TempMin = (*(&HeatCoolEle_Min.u16Heat_OpenTemp + i));
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	ReadEEPROM_EventRecord_Parameters();
}

// Sci命令的数据
void EEPROM_ResetData_AllToDefault(void)
{
	const struct PRT_E2ROM_PARAS PrtE2PARAS_Default = E2P_PROTECT_DEFAULT_PRT;
	const struct OTHER_ELEMENT OtherElement_Default = OtherElement_default;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Default = HeatCoolElement_Default;

	UINT8 i;

	for (i = 0; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
	u8E2P_KB_WriteFlag = KB_NUM;
	u8E2P_KB_WritePos = 0;

	// Protect
	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&PrtE2PARAS_Default.u16VcellOvp_First + i);
	}
	u32E2P_Pro_VolCur_WriteFlag = E2P_PARA_ALL_VOLCUR_PROTECT;
	u32E2P_Pro_Temp_WriteFlag = E2P_PARA_ALL_TEM_PROTECT;
	u32E2P_Pro_Other_WriteFlag = E2P_PARA_ALL_OTHER_PROTECT;

	// Other_CanAdd_element
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&OtherElement_Default.u16Balance_OpenVoltage + i);
	}
	// CBC_Element_Calculate();
	u32E2P_OtherElement1_WriteFlag = E2P_PARA_ALL_OTHER_ELEMENT1;

	// HeatCool_element
	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = *(&HeatCoolEle_Default.u16Heat_OpenTemp + i);
	}
	u32E2P_HeatCool_WriteFlag = E2P_PARA_ALL_HEAT_COOL_ELE;
}

// 历史保护记录reset
void EEPROM_ResetData_OtherToDefault(void)
{
	EEPROM_ResetData_EventRecord_ToDefault();

	SystemMonitorResetData_EEPROM(); // 系统功能选取标志位存储
}

// Sci命令表中，因为STM8的缘故，决定全部从通讯中移出来写
void WriteEEPROM_ByteData_Circle(void)
{
	UINT8 i = 0;
	UINT8 u8temp;
	const struct PRT_E2ROM_PARAS PrtE2paras_Pos = E2P_ADDR_E2POS_PROTECT;
	const struct OTHER_ELEMENT OtherCanAdd_Pos = E2P_ADDR_E2POS_OTHER_ELEMENT1;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Pos = E2P_ADDR_E2POS_HEAT_COOL;
	// const struct RTC_ELEMENT RTC_Element_Pos = E2P_ADDR_E2POS_RTC;

	if (u8E2P_KB_WriteFlag)
	{ // 完美KB值操作，既可全部写一遍，也可以单独写其中一对KB值
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (u8E2P_KB_WritePos << 1)), g_u16CalibCoefK[u8E2P_KB_WritePos]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (u8E2P_KB_WritePos << 1)), g_i16CalibCoefB[u8E2P_KB_WritePos]);
		++u8E2P_KB_WritePos; // 如果u8E2P_KB_WriteFlag=0，则Pos就算错也没用，别的地方想修改KB值的话，这两者必须同时操作。
		--u8E2P_KB_WriteFlag;
	}
	else if (u32E2P_Pro_VolCur_WriteFlag & E2P_PARA_ALL_VOLCUR_PROTECT)
	{
		while (i < E2P_PARA_ALL_VOLCUR_PROTECT)
		{
			if ((u32E2P_Pro_VolCur_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i),
										  *(&PRT_E2ROMParas.u16VcellOvp_First + i));
				u32E2P_Pro_VolCur_WriteFlag -= ((long)1 << i); // 按位操作，有一个减一个。
				break;
			}
			i++;
		}
	}
	else if (u32E2P_Pro_Temp_WriteFlag & E2P_PARA_ALL_TEM_PROTECT)
	{
		while (i < E2P_PARA_ALL_TEM_PROTECT)
		{
			if ((u32E2P_Pro_Temp_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16TChgOTp_First + i),
										  *(&PRT_E2ROMParas.u16TChgOTp_First + i));
				u32E2P_Pro_Temp_WriteFlag -= ((long)1 << i);
				break;
			}
			i++;
		}
	}
	else if (u32E2P_Pro_Other_WriteFlag & E2P_PARA_ALL_OTHER_PROTECT)
	{
		while (i < E2P_PARA_ALL_OTHER_PROTECT)
		{
			if ((u32E2P_Pro_Other_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VdeltaOvp_First + i),
										  *(&PRT_E2ROMParas.u16VdeltaOvp_First + i));
				u32E2P_Pro_Other_WriteFlag -= ((long)1 << i);
				break;
			}
			i++;
		}
	}
	else if (u32E2P_OtherElement1_WriteFlag & E2P_PARA_ALL_OTHER_ELEMENT1)
	{
		while (i < E2P_PARA_ALL_OTHER_ELEMENT1)
		{
			if ((u32E2P_OtherElement1_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&OtherCanAdd_Pos.u16Balance_OpenVoltage + i),
										  *(&OtherElement.u16Balance_OpenVoltage + i));
				u32E2P_OtherElement1_WriteFlag -= ((long)1 << i);
				break;
			}
			i++;
		}
	}
	else if (u32E2P_HeatCool_WriteFlag)
	{
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			if ((u32E2P_HeatCool_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i), *(&Heat_Cool_Element.u16Heat_OpenTemp + i));
				u32E2P_HeatCool_WriteFlag -= ((long)1 << i);
				break;
			}
		}
	}
	else if (gu8_Reset_EventRecord)
	{
		u8temp = 100 - gu8_Reset_EventRecord;
		WriteEEPROM_Word_NoZone(E2P_ADDR_START_EVENT_RECORD + (u8temp << 1), 0);
		gu8_Reset_EventRecord--;
		if (gu8_Reset_EventRecord == 1)
		{
			WriteEEPROM_Word_NoZone(E2P_ADDR_E2POS_EVENT_POINT, 0);
		}
	}
}

// 初始化IIC
void InitE2PROM(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	// PB3_I2C_SCL_eeprom，PB4_I2C_SDA_eeprom
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_3 | GPIO_Pin_4); // 输出高

	// PA15_E2PR_WP
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	__delay_ms(100);

	EEPROM_Storage_EnsureReady();
	InitData_E2prom();
}

void InitData_E2prom(void)
{
	if (EEPROM_VALUE_BEGIN_FLAG == ReadEEPROM_Word_NoZone(EEPROM_ADDR_PASS))
	{ // 第二次上电就会执行这个
		ReadEEPROM_ByteData_StartUp();
	}
	else
	{ // 第一次上电，用于量产
		EEPROM_ResetData_AllToDefault();
		while (u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag || u32E2P_Pro_Other_WriteFlag || u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u32E2P_RTC_Element_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag)
		{ // 0x2000,0x2100,0x2200,0x2300
			WriteEEPROM_ByteData_Circle();
		}
		EEPROM_ResetData_OtherToDefault(); // 把E2P_BEGIN_FLAG写进头地址，
										   // 如果有别的添加，可以往这个函数写，目前加了保护记录初始化
		WriteProID_Default();
		
		WriteEEPROM_Word_NoZone(812, 0xffff); // 第一次上电初始化完成
		WriteEEPROM_Word_NoZone(EEPROM_ADDR_PASS, EEPROM_VALUE_BEGIN_FLAG); // 第一次上电初始化完成
	}
}

void App_E2promDeal(void)
{
	if (u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag || u32E2P_Pro_Other_WriteFlag || u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u32E2P_RTC_Element_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag)
	{ // 0x2000,0x2100,0x2200,0x2300
		WriteEEPROM_ByteData_Circle();
	}

	if (gu8_Reset_EventRecord)
	{ // 补充在这里吧
		WriteEEPROM_ByteData_Circle();
	}
}

// 问题找出来，就是BC区写不进去，返回0xFF
void EEPROM_test(void)
{
#if 0
	if(0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2) {
		return;
	}
#endif

#if 1
	WriteEEPROM_Word_NoZone(0x20, EEPROM_VALUE_FLASHUPDATE);
	g_stCellInfoReport.u16VCell[30] = ReadEEPROM_Word_NoZone(0x20);

	WriteEEPROM_Word_NoZone(0x22, EEPROM_VALUE_FLASHUPDATE_RESET);
	g_stCellInfoReport.u16VCell[31] = ReadEEPROM_Word_NoZone(0x22) & 0x000F;
// g_stCellInfoReport.u16VCell[31] = 111;
#endif

#if 0
	WriteEEPROM_Byte(0x20, 0x11);
	g_stCellInfoReport.u16VCell[5] = ReadEEPROM_Byte(0x20);

	WriteEEPROM_Byte(0x22, 0x12);
	g_stCellInfoReport.u16VCell[6] = ReadEEPROM_Byte(0x22);
#endif
}
