#include "main.h"

static UINT16 P12_GetChecksum(const UINT8 *buffer, UINT16 length)
{
	UINT32 sum = 0;
	UINT16 i;

	for (i = 0; i < length; ++i)
	{
		sum += buffer[i];
	}

	return (UINT16)(sum & 0xFFFF);
}

static void P12_PutU16(UINT8 *buffer, UINT8 *index, UINT16 value)
{
	buffer[(*index)++] = (UINT8)(value & 0x00FF);
	buffer[(*index)++] = (UINT8)(value >> 8);
}

static void P12_PutU32(UINT8 *buffer, UINT8 *index, UINT32 value)
{
	buffer[(*index)++] = (UINT8)(value & 0xFF);
	buffer[(*index)++] = (UINT8)((value >> 8) & 0xFF);
	buffer[(*index)++] = (UINT8)((value >> 16) & 0xFF);
	buffer[(*index)++] = (UINT8)((value >> 24) & 0xFF);
}

static void P12_PutI32(UINT8 *buffer, UINT8 *index, INT32 value)
{
	P12_PutU32(buffer, index, (UINT32)value);
}

static UINT16 P12_GetAddress(const struct RS485MSG *s)
{
	return (UINT16)(s->u16Buffer[2] | ((UINT16)s->u16Buffer[3] << 8));
}

static UINT8 P12_GetDataLength(const struct RS485MSG *s)
{
	return s->u16Buffer[5];
}

static UINT8 P12_GetSeriesCount(void)
{
	if ((SeriesNum == 0) || (SeriesNum > 16))
	{
		return 8;
	}

	return SeriesNum;
}

static UINT16 P12_GetRatedCapacitymAh(void)
{
	UINT32 capacitymAh = (UINT32)g_stCellInfoReport.SocElement.u16CapacityFactory * 10U;

	if (capacitymAh > 0xFFFFU)
	{
		capacitymAh = 0xFFFFU;
	}

	return (UINT16)capacitymAh;
}

static UINT16 P12_ParseVersionValue(void)
{
	const UINT8 *version = ProductionInfor.BMS_SoftWareVersion;
	UINT8 i;
	UINT16 parts[3] = {0};
	UINT8 partIndex = 0;

	for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
	{
		UINT8 ch = version[i];

		if ((ch >= '0') && (ch <= '9'))
		{
			parts[partIndex] = (UINT16)(parts[partIndex] * 10U + (UINT16)(ch - '0'));
		}
		else if ((ch == '.') || (ch == '_') || (ch == '-'))
		{
			if (partIndex < 2)
			{
				++partIndex;
			}
		}
		else if (ch == '\0')
		{
			break;
		}
	}

	if (parts[0] > 65U)
	{
		parts[0] = 65U;
	}
	if (parts[1] > 9U)
	{
		parts[1] = 9U;
	}
	if (parts[2] > 99U)
	{
		parts[2] = 99U;
	}

	return (UINT16)(parts[0] * 1000U + parts[1] * 100U + parts[2]);
}

static UINT32 P12_BuildStatusFlags(void)
{
	UINT32 flags = 0;
	const union MDLCHGFAULT_REG *fault = &g_stCellInfoReport.unMdlFault_Third;

	if (fault->bits.b1SocLow)
	{
		flags |= (1UL << 0);
	}
	if (fault->bits.b1IdischgOcp)
	{
		flags |= (1UL << 1);
	}
	if (fault->bits.b1CellDischgOtp)
	{
		flags |= (1UL << 2);
	}
	if (fault->bits.b1IchgOcp)
	{
		flags |= (1UL << 3);
	}
	if (fault->bits.b1CellChgOtp)
	{
		flags |= (1UL << 4);
	}
	if (fault->bits.b1CellDischgUtp)
	{
		flags |= (1UL << 5);
	}
	if (fault->bits.b1CellChgUtp)
	{
		flags |= (1UL << 6);
	}
	if (fault->bits.b1BatOvp)
	{
		flags |= (1UL << 7);
	}
	if (fault->bits.b1BatUvp)
	{
		flags |= (1UL << 8);
	}
	if (fault->bits.b1CellOvp)
	{
		flags |= (1UL << 9);
	}
	if (fault->bits.b1CellUvp)
	{
		flags |= (1UL << 10);
	}
	if (fault->bits.b1VcellDeltaBig)
	{
		flags |= (1UL << 11);
	}
	if (fault->bits.b1TmosOtp)
	{
		flags |= (1UL << 12);
	}

	return flags;
}

static void P12_FillStaticPayload(UINT8 *payload, UINT8 *payloadLength)
{
	UINT8 i;

	*payloadLength = 0;
	P12_PutU16(payload, payloadLength, P12_GetRatedCapacitymAh());
	P12_PutU16(payload, payloadLength, P12_ParseVersionValue());

	for (i = 0; i < 14; ++i)
	{
		if ((i < ProductionInfor.BMS_SerialNumberLength) && (ProductionInfor.BMS_SerialNumber[i] != '\0'))
		{
			payload[(*payloadLength)++] = ProductionInfor.BMS_SerialNumber[i];
		}
		else
		{
			payload[(*payloadLength)++] = 0;
		}
	}

	payload[(*payloadLength)++] = 0;
}

static void P12_FillDynamicPayload(UINT8 *payload, UINT8 *payloadLength)
{
	UINT8 i;
	UINT8 seriesCount = P12_GetSeriesCount();
	UINT16 totalVoltagemV = (UINT16)(g_stCellInfoReport.u16VCellTotle * 10U);
	INT32 currentmA;
	UINT8 soc;
	UINT32 statusFlags = P12_BuildStatusFlags();

	*payloadLength = 0;

	if (g_stCellInfoReport.u16Ichg > 0U)
	{
		currentmA = (INT32)g_stCellInfoReport.u16Ichg * 100;
	}
	else
	{
		currentmA = -((INT32)g_stCellInfoReport.u16IDischg * 100);
	}

	soc = (g_stCellInfoReport.SocElement.u16Soc > 100U) ? 100U : (UINT8)g_stCellInfoReport.SocElement.u16Soc;

	P12_PutU16(payload, payloadLength, totalVoltagemV);
	P12_PutI32(payload, payloadLength, currentmA);
	P12_PutU16(payload, payloadLength, g_stCellInfoReport.u16TempMax);
	P12_PutU16(payload, payloadLength, g_stCellInfoReport.SocElement.u16Cycle_times);
	payload[(*payloadLength)++] = soc;
	payload[(*payloadLength)++] = seriesCount;

	for (i = 0; i < seriesCount; ++i)
	{
		P12_PutU16(payload, payloadLength, g_stCellInfoReport.u16VCell[i]);
	}

	P12_PutU32(payload, payloadLength, statusFlags);
}

static UINT8 P12_BuildFrame(struct RS485MSG *s, UINT8 responseCmd)
{
	UINT8 index = 0;
	UINT8 payload[64];
	UINT8 payloadLength = 0;
	UINT8 i;
	UINT16 checksum;

	switch (responseCmd)
	{
	case P12_CMD_RSP_STATIC:
		P12_FillStaticPayload(payload, &payloadLength);
		break;

	case P12_CMD_RSP_DYNAMIC:
		P12_FillDynamicPayload(payload, &payloadLength);
		break;

	default:
		return 0;
	}

	s->u16Buffer[index++] = P12_FRAME_START_0;
	s->u16Buffer[index++] = P12_FRAME_START_1;
	s->u16Buffer[index++] = (UINT8)(P12_HOST_ADDRESS & 0x00FF);
	s->u16Buffer[index++] = (UINT8)(P12_HOST_ADDRESS >> 8);
	s->u16Buffer[index++] = responseCmd;
	s->u16Buffer[index++] = payloadLength;

	for (i = 0; i < payloadLength; ++i)
	{
		s->u16Buffer[index++] = payload[i];
	}

	checksum = P12_GetChecksum(s->u16Buffer, index);
	P12_PutU16(s->u16Buffer, &index, checksum);

	s->AckLenth = index;
	s->AckType = RS485_ACK_POS;
	s->ptr_no = 0;
	return 1;
}

UINT8 P12_IsStartByte(UINT8 firstByte)
{
	return (UINT8)(firstByte == P12_FRAME_START_0);
}

UINT8 P12_CheckHeader(const struct RS485MSG *s)
{
	return (UINT8)((s->u16Buffer[0] == P12_FRAME_START_0) && (s->u16Buffer[1] == P12_FRAME_START_1));
}

UINT8 P12_IsRxComplete(const struct RS485MSG *s)
{
	if (s->ptr_no < 5U)
	{
		return 0;
	}

	return (UINT8)(s->ptr_no == (UINT8)(P12_GetDataLength(s) + 7U));
}

UINT8 P12_PrepareResponse(struct RS485MSG *s)
{
	UINT16 frameLength = s->ptr_no;
	UINT16 checksum;
	UINT16 frameChecksum;

	if (frameLength < P12_FRAME_MIN_LENGTH)
	{
		return 0;
	}

	if ((UINT16)P12_GetDataLength(s) + P12_FRAME_MIN_LENGTH != frameLength)
	{
		return 0;
	}

	checksum = P12_GetChecksum(s->u16Buffer, (UINT16)(frameLength - 2U));
	frameChecksum = (UINT16)(s->u16Buffer[frameLength - 2U] | ((UINT16)s->u16Buffer[frameLength - 1U] << 8));
	if (checksum != frameChecksum)
	{
		return 0;
	}

	if (P12_GetAddress(s) != P12_BMS_ADDRESS_DEFAULT)
	{
		return 0;
	}

	switch (s->u16Buffer[4])
	{
	case P12_CMD_REQ_STATIC:
		return P12_BuildFrame(s, P12_CMD_RSP_STATIC);

	case P12_CMD_REQ_DYNAMIC:
		return P12_BuildFrame(s, P12_CMD_RSP_DYNAMIC);

	default:
		return 0;
	}
}
