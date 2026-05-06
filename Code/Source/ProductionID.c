#include "main.h"

PRODUCTION_ID_INFO ProductionInfor;

static UINT16 ProductionID_ClampLength(UINT16 length)
{
	if (length == 0 || length == 0xFFFF || length > PRODUCT_ID_LENGTH_MAX)
	{
		return 0;
	}

	return length;
}

static void ProductionID_LoadWords(UINT16 head_address, UINT8 *dest)
{
	UINT8 i;

	memset(dest, 0, PRODUCT_ID_LENGTH_MAX);
	for (i = 0; i < (PRODUCT_ID_LENGTH_MAX >> 1); i++)
	{
		*((UINT16 *)(&dest[i * 2])) = ReadEEPROM_Word_NoZone(head_address + 2 + i * 2);
	}
}

void InitProID(void)
{
	UINT16 serial_length;
	UINT16 hardware_length;
	UINT16 software_length;

	ProductionInfor.BMS_SerialNumberHeadAdress = E2P_ADDR_E2POS_SERIAL_NUM;
	ProductionInfor.BMS_HardWareVersionHeadAdress = E2P_ADDR_E2POS_HAEDWARE_VER;
	ProductionInfor.BMS_SoftWareVersionHeadAdress = E2P_ADDR_E2POS_SOFTWARE_VER;

	serial_length = ProductionID_ClampLength(ReadEEPROM_Word_NoZone(ProductionInfor.BMS_SerialNumberHeadAdress));
	hardware_length = ProductionID_ClampLength(ReadEEPROM_Word_NoZone(ProductionInfor.BMS_HardWareVersionHeadAdress));
	software_length = ProductionID_ClampLength(ReadEEPROM_Word_NoZone(ProductionInfor.BMS_SoftWareVersionHeadAdress));

	if (serial_length == 0 || hardware_length == 0 || software_length == 0)
	{
		WriteProID_Default();
		return;
	}

	ProductionInfor.BMS_SerialNumberLength = serial_length;
	ProductionInfor.BMS_HardWareVersionLength = hardware_length;
	ProductionInfor.BMS_SoftWareVersionLength = software_length;

	ProductionID_LoadWords(ProductionInfor.BMS_SerialNumberHeadAdress, &ProductionInfor.BMS_SerialNumber[0]);
	ProductionID_LoadWords(ProductionInfor.BMS_HardWareVersionHeadAdress, &ProductionInfor.BMS_HardWareVersion[0]);
	ProductionID_LoadWords(ProductionInfor.BMS_SoftWareVersionHeadAdress, &ProductionInfor.BMS_SoftWareVersion[0]);
}

void WriteProID(void)
{
	UINT8 i;

	if (ProductionInfor.BMS_SerialNumber_WriteFlag)
	{
		WriteEEPROM_Word_NoZone(ProductionInfor.BMS_SerialNumberHeadAdress,
								ProductionID_ClampLength(ProductionInfor.BMS_SerialNumberLength));
		for (i = 0; i < (PRODUCT_ID_LENGTH_MAX >> 1); i++)
		{
			WriteEEPROM_Word_NoZone(ProductionInfor.BMS_SerialNumberHeadAdress + 2 + i * 2,
									  *((UINT16 *)(&ProductionInfor.BMS_SerialNumber[i * 2])));
		}
		ProductionInfor.BMS_SerialNumber_WriteFlag = 0;
	}

	if (ProductionInfor.BMS_HardWareVersion_WriteFlag)
	{
		WriteEEPROM_Word_NoZone(ProductionInfor.BMS_HardWareVersionHeadAdress,
								ProductionID_ClampLength(ProductionInfor.BMS_HardWareVersionLength));
		for (i = 0; i < (PRODUCT_ID_LENGTH_MAX >> 1); i++)
		{
			WriteEEPROM_Word_NoZone(ProductionInfor.BMS_HardWareVersionHeadAdress + 2 + i * 2,
									  *((UINT16 *)(&ProductionInfor.BMS_HardWareVersion[i * 2])));
		}
		ProductionInfor.BMS_HardWareVersion_WriteFlag = 0;
	}

	if (ProductionInfor.BMS_SoftWareVersion_WriteFlag)
	{
		WriteEEPROM_Word_NoZone(ProductionInfor.BMS_SoftWareVersionHeadAdress,
								ProductionID_ClampLength(ProductionInfor.BMS_SoftWareVersionLength));
		for (i = 0; i < (PRODUCT_ID_LENGTH_MAX >> 1); i++)
		{
			WriteEEPROM_Word_NoZone(ProductionInfor.BMS_SoftWareVersionHeadAdress + 2 + i * 2,
									  *((UINT16 *)(&ProductionInfor.BMS_SoftWareVersion[i * 2])));
		}
		ProductionInfor.BMS_SoftWareVersion_WriteFlag = 0;
	}
}

void WriteProID_Default(void)
{
	UINT8 harewareCount = sizeof(BMS_HARDWARE_VERDION_DEFAULT) > 32 ? 32 : sizeof(BMS_HARDWARE_VERDION_DEFAULT);
	UINT8 softwareCount = sizeof(BMS_SOFTWARE_VERDION_DEFAULT) > 32 ? 32 : sizeof(BMS_SOFTWARE_VERDION_DEFAULT);
	UINT8 serialNumberCount = sizeof(BMS_SERIAL_NUMBER_DEFAULT) > 32 ? 32 : sizeof(BMS_SERIAL_NUMBER_DEFAULT);

	memset(&ProductionInfor, 0, sizeof(PRODUCTION_ID_INFO));

	ProductionInfor.BMS_SerialNumberHeadAdress = E2P_ADDR_E2POS_SERIAL_NUM;
	ProductionInfor.BMS_HardWareVersionHeadAdress = E2P_ADDR_E2POS_HAEDWARE_VER;
	ProductionInfor.BMS_SoftWareVersionHeadAdress = E2P_ADDR_E2POS_SOFTWARE_VER;

	ProductionInfor.BMS_SerialNumberLength = serialNumberCount;
	ProductionInfor.BMS_HardWareVersionLength = harewareCount;
	ProductionInfor.BMS_SoftWareVersionLength = softwareCount;

	memcpy(&ProductionInfor.BMS_HardWareVersion[0], BMS_HARDWARE_VERDION_DEFAULT, harewareCount);
	memcpy(&ProductionInfor.BMS_SoftWareVersion[0], BMS_SOFTWARE_VERDION_DEFAULT, softwareCount);
	memcpy(&ProductionInfor.BMS_SerialNumber[0], BMS_SERIAL_NUMBER_DEFAULT, serialNumberCount);

	ProductionInfor.BMS_SerialNumber_WriteFlag = 1;
	ProductionInfor.BMS_HardWareVersion_WriteFlag = 1;
	ProductionInfor.BMS_SoftWareVersion_WriteFlag = 1;
	WriteProID();
}

void App_ProID_Deal(void)
{
	static UINT8 su8_StartUpFlag = 0;

	switch (su8_StartUpFlag)
	{
	case 0:
		InitProID();
		su8_StartUpFlag = 1;
		break;

	case 1:
		WriteProID();
		break;

	default:
		su8_StartUpFlag = 0;
		break;
	}
}
