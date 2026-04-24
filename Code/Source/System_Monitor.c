#include "main.h"

volatile struct SYSTEM_ERROR System_ErrFlag;
volatile union System_OnOFF_Function System_OnOFF_Func;
volatile union System_OnOFF_Function System_OnOFF_Func_StartUpRec;
volatile union System_Status SystemStatus;
volatile union System_Function_StartUp System_Func_StartUp;

void InitSystemMonitorData_EEPROM(void)
{
	// 系统功能控制类型，原则上，关闭功能不用管下面的，只需要这个置零就好
	System_OnOFF_Func.all = 0;
	System_OnOFF_Func.bits.b1OnOFF_Balance = 1;
	System_OnOFF_Func.bits.b1OnOFF_BMS_Source = 1;
	System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; // 核心功能
	// System_OnOFF_Func.bits.b1OnOFF_Relay_Rec = 1;
	System_OnOFF_Func.bits.b1OnOFF_AFE1 = 1;
	// System_OnOFF_Func.bits.b1OnOFF_AFE2 = 1;
	System_OnOFF_Func.bits.b1OnOFF_Sleep = 1;
	System_OnOFF_Func.bits.b1OnOFF_Heat = 1;
	System_OnOFF_Func.bits.b1OnOFF_Cool = 0;
	System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed = 0;

	// 系统开机时序控制类型
	System_Func_StartUp.all = 0; // 置1的原因是需要初始化的意思，意思是这个功能需要初始化，所以不用0
	System_Func_StartUp.bits.b1StartUpFlag_SOC = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Balance = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Protect = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Relay = 1; // 没用继电器用MOS，这个值就不管就好，因为初始化建立是||，其中一个建立便可
	System_Func_StartUp.bits.b1StartUpFlag_MOS = 1;
	System_Func_StartUp.bits.b1StartUpFlag_ADC = 1;
	System_Func_StartUp.bits.b1StartUpFlag_CAN = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Cool = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Heat = 0;  // 原则上不用，先留着
	System_Func_StartUp.bits.b1StartUpFlag_BlueT = 0; //

	// 系统状态跟踪类型
	SystemStatus.all = 0;
	// SystemStatus.bits.b1Status_SysLimits = 0;		//默认无密码，不限制
	SystemStatus.bits.b1StartUpBMS = 1;			  // MOS或者接触器能打开意味着初始化完毕
	SystemStatus.bits.b1Status_Relay_PRE = CLOSE; // 默认是不打开，但是CLOSE不一定是低电平
	SystemStatus.bits.b1Status_Relay_CHG = CLOSE; // 全部写，方便上传给上位机，不需要选通再改
	SystemStatus.bits.b1Status_Relay_DSG = CLOSE;
	SystemStatus.bits.b1Status_Relay_MAIN = CLOSE;
	SystemStatus.bits.b1Status_MOS_PRE = CLOSE;
	SystemStatus.bits.b1Status_MOS_CHG = CLOSE;
	SystemStatus.bits.b1Status_MOS_DSG = CLOSE;

	// 总感觉，这个存在很麻烦，如果板子以前烧了代码，则这个关闭加热冷凝功能则没法处理。
	// 但是屏蔽了，假设均衡关掉，再次启动又打开很麻烦(但是基本都是要求均衡的)
	// 综上所述，先屏蔽观察一下
	// System_OnOFF_Func.all = (UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT);
	// System_OnOFF_Func.all |= ((UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2)<<16);		//先扩大为32位再移位

	System_OnOFF_Func_StartUpRec.all = System_OnOFF_Func.all; // 如果某功能开机不打开，后续运行中途打开，则需要初始化，该位为记录位
	if (!System_OnOFF_Func.bits.b1OnOFF_Balance)
		System_Func_StartUp.bits.b1StartUpFlag_Balance = 0;
	if (!System_OnOFF_Func.bits.b1OnOFF_Heat)
		System_Func_StartUp.bits.b1StartUpFlag_Heat = 0;
	if (!System_OnOFF_Func.bits.b1OnOFF_Cool)
		System_Func_StartUp.bits.b1StartUpFlag_Cool = 0;
	if (!System_OnOFF_Func.bits.b1OnOFF_MOS_Relay)
	{ // 没打开MOS功能，则不会完成初始化，打开才能把SystemStatus.bits.b1StartUpBMS关掉同时打开MOS
		System_Func_StartUp.bits.b1StartUpFlag_MOS = 0;
		System_Func_StartUp.bits.b1StartUpFlag_Relay = 0;
	}

	// 系统错误控制类型
	// 这个没有
}

// 这个函数修改了，要修改EEPROM的上电标志位
void SystemMonitorResetData_EEPROM(void)
{
	// 系统功能控制类型
	System_OnOFF_Func.all = 0;
	System_OnOFF_Func.bits.b1OnOFF_Balance = 1;
	System_OnOFF_Func.bits.b1OnOFF_BMS_Source = 1;
	System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; // 核心功能
	// System_OnOFF_Func.bits.b1OnOFF_Relay_Rec = 1;
	System_OnOFF_Func.bits.b1OnOFF_AFE1 = 1;
	// System_OnOFF_Func.bits.b1OnOFF_AFE2 = 1;
	System_OnOFF_Func.bits.b1OnOFF_Sleep = 1;
	System_OnOFF_Func.bits.b1OnOFF_Heat = 1;
	System_OnOFF_Func.bits.b1OnOFF_Cool = 1;
	System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed = 0;

	WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF));
	WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16));
}

UINT16 Sys_FindProtectFilterMax(void)
{
	UINT8 i;
	static UINT16 s_u16ProtectFilterMax = 0;
	if (0 != s_u16ProtectFilterMax)
	{
		return s_u16ProtectFilterMax + 2; // 推迟20ms
	}
	for (i = 0; i < 13; ++i)
	{
		if (*(&PRT_E2ROMParas.u16VcellOvp_Filter + 5 * i) > s_u16ProtectFilterMax)
		{
			s_u16ProtectFilterMax = *(&PRT_E2ROMParas.u16VcellOvp_Filter + 5 * i);
		}
	}
	s_u16ProtectFilterMax = 0;
	return s_u16ProtectFilterMax + 2; // 推迟20ms
}

// 前面初始化，硬件3s，读EEPROM要1s
// 开机时序汇总，AD稳定500ms，SOC初始化400ms，保护点判断1s(最大值)，打开管子30ms
// 加起来大概6s
StartUp_Status System_FUNC_StartUp(enum SYSTEM_FUNC_STARTUP_COMMAND SystemFunction)
{
	return STARTUP_OVER;
}

// 0:没错误，X：有错误
UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 result = 0;

	switch (errorCode)
	{
	case ERROR_AFE1:
		System_ErrFlag.u8ErrFlag_Com_AFE1++;
		break;
	case ERROR_AFE2:
		System_ErrFlag.u8ErrFlag_Com_AFE2++;
		break;
	case ERROR_CAN:
		System_ErrFlag.u8ErrFlag_Com_Can++;
		break;
	case ERROR_EEPROM_COM:
		System_ErrFlag.u8ErrFlag_Com_EEPROM++;
		break;
	case ERROR_SPI:
		System_ErrFlag.u8ErrFlag_Com_SPI++;
		break;
	case ERROR_UPPER:
		System_ErrFlag.u8ErrFlag_Com_Upper++;
		break;
	case ERROR_CLIENT:
		System_ErrFlag.u8ErrFlag_Com_Client++;
		break;
	case ERROR_SCREEN:
		System_ErrFlag.u8ErrFlag_Com_Screen++;
		break;
	case ERROR_WIFI:
		System_ErrFlag.u8ErrFlag_Com_Wifi++;
		break;
	case ERROR_BLUETOOTH:
		System_ErrFlag.u8ErrFlag_Com_BlueTooth++;
		break;
	case ERROR_APP:
		System_ErrFlag.u8ErrFlag_Com_App++;
		break;
	case ERROR_CBC_CHG:
		System_ErrFlag.u8ErrFlag_CBC_CHG++;
		break;
	case ERROR_CBC_DSG:
		System_ErrFlag.u8ErrFlag_CBC_DSG++;
		break;
	case ERROR_EEPROM_STORE:
		System_ErrFlag.u8ErrFlag_Store_EEPROM++;
		break;
	case ERROR_HSE:
		System_ErrFlag.u8ErrFlag_HSE++;
		break;
	case ERROR_LSE:
		System_ErrFlag.u8ErrFlag_LSE++;
		break;
	case ERROR_VDEATLE_OVER:
		System_ErrFlag.u8ErrFlag_Vdelta_OVER++;
		break;
	case ERROR_BALANCED:
		System_ErrFlag.u8ErrFlag_Balanced++;
		break;
	case ERROR_ADC:
		System_ErrFlag.u8ErrFlag_ADC++;
		break;
	case ERROR_SOC_CAIL:
		System_ErrFlag.u8ErrFlag_SOC_Cail++;
		break;
	case ERROR_HEAT:
		System_ErrFlag.u8ErrFlag_Heat++;
		break;
	case ERROR_COOL:
		System_ErrFlag.u8ErrFlag_Cool++;
		break;
	case ERROR_TEMP_BREAK:
		System_ErrFlag.u8ErrFlag_TempBreak = 1;
		break;
	case ERROR_DSG_SHORT:
		System_ErrFlag.u8ErrFlag_DsgShort++;
		break;

	case ERROR_REMOVE_AFE1:
		System_ErrFlag.u8ErrFlag_Com_AFE1 = 0;
		break;
	case ERROR_REMOVE_AFE2:
		System_ErrFlag.u8ErrFlag_Com_AFE2 = 0;
		break;
	case ERROR_REMOVE_CAN:
		System_ErrFlag.u8ErrFlag_Com_Can = 0;
		break;
	case ERROR_REMOVE_EEPROM_COM:
		System_ErrFlag.u8ErrFlag_Com_EEPROM = 0;
		break;
	case ERROR_REMOVE_SPI:
		System_ErrFlag.u8ErrFlag_Com_SPI = 0;
		break;
	case ERROR_REMOVE_UPPER:
		System_ErrFlag.u8ErrFlag_Com_Upper = 0;
		break;
	case ERROR_REMOVE_CLIENT:
		System_ErrFlag.u8ErrFlag_Com_Client = 0;
		break;
	case ERROR_REMOVE_SCREEN:
		System_ErrFlag.u8ErrFlag_Com_Screen = 0;
		break;
	case ERROR_REMOVE_WIFI:
		System_ErrFlag.u8ErrFlag_Com_Wifi = 0;
		break;
	case ERROR_REMOVE_BLUETOOTH:
		System_ErrFlag.u8ErrFlag_Com_BlueTooth = 0;
		break;
	case ERROR_REMOVE_APP:
		System_ErrFlag.u8ErrFlag_Com_App = 0;
		break;
	case ERROR_REMOVE_CBC_CHG:
		System_ErrFlag.u8ErrFlag_CBC_CHG = 0;
		break;
	case ERROR_REMOVE_CBC_DSG:
		System_ErrFlag.u8ErrFlag_CBC_DSG = 0;
		break;
	case ERROR_REMOVE_EEPROM_STORE:
		System_ErrFlag.u8ErrFlag_Store_EEPROM = 0;
		break;
	case ERROR_REMOVE_HSE:
		System_ErrFlag.u8ErrFlag_HSE = 0;
		break;
	case ERROR_REMOVE_LSE:
		System_ErrFlag.u8ErrFlag_LSE = 0;
		break;
	case ERROR_REMOVE_VDEATLE_OVER:
		System_ErrFlag.u8ErrFlag_Vdelta_OVER = 0;
		break;
	case ERROR_REMOVE_BALANCED:
		System_ErrFlag.u8ErrFlag_Balanced = 0;
		break;
	case ERROR_REMOVE_ADC:
		System_ErrFlag.u8ErrFlag_ADC = 0;
		break;
	case ERROR_REMOVE_HEAT:
		System_ErrFlag.u8ErrFlag_Heat = 0;
		break;
	case ERROR_REMOVE_COOL:
		System_ErrFlag.u8ErrFlag_Cool = 0;
		break;
	case ERROR_REMOVE_SOC_CAIL:
		System_ErrFlag.u8ErrFlag_SOC_Cail = 0;
		break;
	case ERROR_REMOVE_TEMP_BREAK:
		System_ErrFlag.u8ErrFlag_TempBreak = 0;
		break;
	case ERROR_REMOVE_DSG_SHORT:
		System_ErrFlag.u8ErrFlag_DsgShort = 0;
		break;

	case ERROR_STATUS_AFE1:
		result = System_ErrFlag.u8ErrFlag_Com_AFE1;
		break;
	case ERROR_STATUS_AFE2:
		result = System_ErrFlag.u8ErrFlag_Com_AFE2;
		break;
	case ERROR_STATUS_CAN:
		result = System_ErrFlag.u8ErrFlag_Com_Can;
		break;
	case ERROR_STATUS_EEPROM_COM:
		result = System_ErrFlag.u8ErrFlag_Com_EEPROM;
		break;
	case ERROR_STATUS_SPI:
		result = System_ErrFlag.u8ErrFlag_Com_SPI;
		break;
	case ERROR_STATUS_UPPER:
		result = System_ErrFlag.u8ErrFlag_Com_Upper;
		break;
	case ERROR_STATUS_CLIENT:
		result = System_ErrFlag.u8ErrFlag_Com_Client;
		break;
	case ERROR_STATUS_SCREEN:
		result = System_ErrFlag.u8ErrFlag_Com_Screen;
		break;
	case ERROR_STATUS_WIFI:
		result = System_ErrFlag.u8ErrFlag_Com_Wifi;
		break;
	case ERROR_STATUS_BLUETOOTH:
		result = System_ErrFlag.u8ErrFlag_Com_BlueTooth;
		break;
	case ERROR_STATUS_APP:
		result = System_ErrFlag.u8ErrFlag_Com_App;
		break;
	case ERROR_STATUS_CBC_CHG:
		result = System_ErrFlag.u8ErrFlag_CBC_CHG;
		break;
	case ERROR_STATUS_CBC_DSG:
		result = System_ErrFlag.u8ErrFlag_CBC_DSG;
		break;
	case ERROR_STATUS_EEPROM_STORE:
		result = System_ErrFlag.u8ErrFlag_Store_EEPROM;
		break;
	case ERROR_STATUS_HSE:
		result = System_ErrFlag.u8ErrFlag_HSE;
		break;
	case ERROR_STATUS_LSE:
		result = System_ErrFlag.u8ErrFlag_LSE;
		break;
	case ERROR_STATUS_VDEATLE_OVER:
		result = System_ErrFlag.u8ErrFlag_Vdelta_OVER;
		break;
	case ERROR_STATUS_BALANCED:
		result = System_ErrFlag.u8ErrFlag_Balanced;
		break;
	case ERROR_STATUS_ADC:
		result = System_ErrFlag.u8ErrFlag_ADC;
		break;
	case ERROR_STATUS_SOC_CAIL:
		result = System_ErrFlag.u8ErrFlag_SOC_Cail;
		break;
	case ERROR_STATUS_HEAT:
		result = System_ErrFlag.u8ErrFlag_Heat;
		break;
	case ERROR_STATUS_COOL:
		result = System_ErrFlag.u8ErrFlag_Cool;
		break;
	case ERROR_STATUS_TEMP_BREAK:
		result = System_ErrFlag.u8ErrFlag_TempBreak;
		break;
	case ERROR_STATUS_DSG_SHORT:
		result = System_ErrFlag.u8ErrFlag_DsgShort;
		break;
	default:
		break;
	}

	return result;
}
