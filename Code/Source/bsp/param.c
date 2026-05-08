#include "param.h"

PARAM_T g_tParam;

void Param_SyncFromRuntime(void)
{
	g_tParam.ParamVer = PARAM_VER;
	g_tParam.protect = PRT_E2ROMParas;
	g_tParam.other = OtherElement;
	g_tParam.heat = Heat_Cool_Element;
}

void LoadParam(void)
{
	sys_time.test_sizeof_g_tParam = sizeof(g_tParam);
	Param_SyncFromRuntime();
}

void SaveParam(void)
{
	Param_SyncFromRuntime();
	(void)EEPROM_SaveRWParametersToFlash();
}
