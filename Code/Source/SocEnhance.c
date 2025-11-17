#include "SocEnhance.h"
#include "conf.h"
#include "Sci_Upper.h"

#define SILENT_POWER (10 / 100) // 百毫安

#define SOC_100_VAL (4180)
#define SOC_0_VAL (3000)

#define VCELLMAX g_stCellInfoReport.u16VCellMax
#define VCELLMIN g_stCellInfoReport.u16VCellMin

#define SOC_OCV_UPDATE 3000 // 暂定200*6000 = 1200s = 20min
							// 暂定200*3000 = 600s = 10min

#define SOC_VIRTUAL_CURRENT_CHG (UINT16)2 // A*10，1和2都认为是0，带=号，0.2就开始算了
#define SOC_VIRTUAL_CURRENT_DSG (UINT16)2 // A*10，1和2都认为是0，这个不能为0的同时，把=号判断上去，不然就会卡在DSG那里计算出不来。

#define DELAYB1000MS_5MIN 300  // 默认通讯周期为1s一次
#define DELAYB1000MS_10MIN 600 // 默认通讯周期为1s一次

// #define CHG_CUR_1C							2100	//A*10恒流充电为1C，恒压充电为1C-0.1C(SOC=95%)，涓流充电也为0.1C

#define EEPROM_VALUE_SLEEP_FLAG ((UINT16)0x1234)
#define EEPROM_VALUE_POWEROFF_FLAG ((UINT16)0x5678)
#define EEPROM_VALUE_DATA_UPDATE_FLAG ((UINT16)0x9ABC)
#define EEPROM_VALUE_STORE_RESET ((UINT16)0xFFFF)

uint16_t time_soc1_100_100mA_unit;

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;		// 对外交互结构体,lib文件的桥梁
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element; // 内部计算结构体
struct SOC_ENHANCE_E2PROM_PAR SOC_E2prom_Par;		// EEPROM保存关键数据结构体
struct SOC_ENHANCE_E2PROM_PAR SOC_E2prom_Adress;	// EEPROM地址结构体

enum SOC_CALI_STATE SOC_Cali_Flag = SOC_CALI_DATA_INIT; // 妈的，忘了这个？		SOC计算状态机，记得初始化
enum CAP_FULL_STATE CapFull_Cali_Flag = CAP_FULL_INIT;	// 容量更新计算状态机。

UINT16 ChgValue = 0;
UINT16 DsgValue = 0;
// UINT16 SeriousFaultFlag = 0;

uint8_t get_soc_real(void)
{
	return SOC_Calculate_Element.u8SOC_Now;
}

void set_calsoc(uint8_t _soc)
{
	SOC_Calculate_Element.u8SOC_Now = _soc;
	SOC_Calculate_Element.u32CapNow = get_soc_real() * SOC_Calculate_Element.u32CapFull / 100;
}

static void Inc_real_soc(void)
{
	SOC_Calculate_Element.u8SOC_Now += 1;
	SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFull / 100;
}
static void Dec_real_soc(void)
{
#if 0
	SOC_Calculate_Element.u8SOC_Now -= 1;
	SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFull / 100;
#endif
}

// 充电可以提前充满，但是不能卡死
// #define _CAL_SLOW_DOWN_CHG

// 古瑞瓦特
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336,
	100,
	3332,
	90,
	3330,
	80,
	3327,
	75,
	3316,
	70,
	3301,
	65,
	3294,
	60,
	3291,
	55,
	3290,
	50,
	3288,
	45,
	3286,
	40,
	3279,
	35,
	3266,
	30,
	3254,
	25,
	3236,
	20,
	3212,
	15,
	3198,
	10,
	3112,
	5,
	2526,
	0,
	1000,
	0,
	1000,
	0,
};

// 单位为mV和SOC
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4126,
	100,
	4066,
	95,
	4011,
	90,
	3955,
	85,
	3888,
	80,
	3837,
	75,
	3793,
	70,
	3756,
	65,
	3724,
	60,
	3699,
	55,
	3675,
	50,
	3658,
	45,
	3632,
	40,
	3605,
	35,
	3584,
	30,
	3557,
	25,
	3535,
	20,
	3497,
	15,
	3475,
	10,
	3371,
	5,
	3136,
	0,
};

// 单位为mV和SOC
const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650,
	100,
	3600,
	98,
	3550,
	95,
	3500,
	92,
	3400,
	90,
	3350,
	87,
	3340,
	85,
	3335,
	82,
	3330,
	80,
	3325,
	78,
	3320,
	75,
	3300,
	70,
	3275,
	65,
	3250,
	60,
	3200,
	50,
	3150,
	45,
	3100,
	30,
	3000,
	20,
	2850,
	10,
	2750,
	5,
	2650,
	0,
};

// 求绝对值
UINT32 ModulusSubb(UINT32 Data1, UINT32 Data2)
{
	return (UINT32)(Data1 > Data2 ? Data1 - Data2 : Data2 - Data1);
}

UINT16 GetEndValuee(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat)
{
	UINT16 i, t_linenum;
	UINT32 x1 = 0, y1 = 0, x2 = 1, y2 = 1;
	const UINT16 *p;
	UINT16 t_tmp16a, t_tmp16b;
	INT32 t_tmp32a, t_tmp32b;
	UINT32 k, b;
	INT32 ret;
	p = ptbl;

	t_linenum = tblsize - 1;
	for (i = 0; i < tblsize - 2; i = i + 2)
	{
		t_tmp16a = p[i];
		t_tmp16b = p[i + 2];

		if (((dat >= t_tmp16a) && (dat <= t_tmp16b)) || ((dat <= t_tmp16a) && (dat >= t_tmp16b)))
		{
			x1 = t_tmp16a;
			x2 = t_tmp16b;
			y1 = p[i + 1];
			y2 = p[i + 3];
			break;
		}
	}

	if (i >= t_linenum - 1)
	{
		p = ptbl;
		t_tmp16a = p[0];
		t_tmp16b = p[tblsize - 2];

		if (t_tmp16a <= t_tmp16b)
		{
			if (dat >= t_tmp16b)
			{
				t_tmp16a = p[tblsize - 1];
			}
			else
			{
				t_tmp16a = p[1];
			}
		}
		else
		{
			if (dat >= t_tmp16a)
			{
				t_tmp16a = p[1];
			}
			else
			{
				t_tmp16a = p[tblsize - 1];
			}
		}
		return t_tmp16a;
	}
	else
	{
		if (x2 < x1)
		{
			ret = x2;
			x2 = x1;
			x1 = ret;
			ret = y2;
			y2 = y1;
			y1 = ret;
		}

		if (y2 >= y1)
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y2 - y1;
			ret = ret * k;
			if (t_tmp32a >= t_tmp32b)
			{
				b = t_tmp32a - t_tmp32b;
				ret = ret + b;
			}
			else
			{
				b = t_tmp32b - t_tmp32a;
				ret = ret - b;
			}
			ret = ret / (x2 - x1);
		}
		else
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y1 - y2;
			ret = ret * k;
			b = t_tmp32a - t_tmp32b;
			ret = b - ret;
			ret = ret / (x2 - x1);
		}
		return (ret & 0xffff);
	}
}

UINT8 Get_OpenCircuit_Value(void)
{
	UINT8 result = 0;
	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TEST:
		result = GetEndValuee(SOC_Enhance_Element.SOC_Table_CanSet, (UINT16)SOC_Size_TableCanSet, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_LIFEPO:
		result = GetEndValuee(SOC_Table_LiFePO, (UINT16)SOC_Size_LiFePO, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_TERNARYLI:
		result = GetEndValuee(SocTable_TernaryLi, (UINT16)SOC_Size_TernaryLi, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_LIFEPO2:
		result = GetEndValuee(SocTable_LiFePO2, (UINT16)SOC_Size_LiFePO2, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	default:
		result = GetEndValuee(SOC_Table_LiFePO, (UINT16)SOC_Size_TableCanSet, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	}
	return result;
}

// 末端校准
// 以锂智慧为范本
// 基于第一个末端SOC值总充不满，前提条件，校准后的电流值，宁愿偏大也不能偏小
#if 0
void CorrectionTerminal_CV(enum _CUR CurrentType)
{
	static UINT16 su16_SocChgCal_L1_Tcnt = 0;
	static UINT16 su16_SocChgCal_L2_Tcnt = 0;
	static UINT16 su16_SocChgCal_L3_Tcnt = 0;
	static UINT16 su16_SocChgCal_L4_Tcnt = 0;

	static UINT16 su16_SocDsgCal_L1_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L2_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L3_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L4_Tcnt = 0;
	switch (CurrentType)
	{
	case CurCHG:
		// SOC实际认为是100%的点，接近过充保护的时候
		// 本来想把内环校准值加上去的，但是想想这个系数不可控，算了算了，直接骗。
		// 以下这个点，假设我SOC相对不准，例如，大家都从0%开始计算，我最后算得SOC有90%(电流不准+板子本身功耗+时序有点误差)
		// 但实际已经满了，这个点一直没法处理。
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol - 100 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 95)
		{ // 和放电电流对应，第一段，必须拉到95%以内
			if (++su16_SocChgCal_L1_Tcnt >= 10)
			{
				su16_SocChgCal_L1_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now += 1;
				SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (SOC_Calculate_Element.u8SOC_Now > 95)
			{
				if (++su16_SocChgCal_L2_Tcnt >= 8)
				{
					su16_SocChgCal_L2_Tcnt = 0;
					SOC_Calculate_Element.u8SOC_Now += 1;
					SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
			else
			{
				if (++su16_SocChgCal_L3_Tcnt >= 4)
				{
					su16_SocChgCal_L3_Tcnt = 0;
					SOC_Calculate_Element.u8SOC_Now += 1;
					SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
		}

		// 这是基于充电必须能达到100%的终极做法，2S + 1%
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol + 50 && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (++su16_SocChgCal_L4_Tcnt >= 2)
			{
				su16_SocChgCal_L4_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now += 1;
				SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
			}
		}

#ifdef _CAL_SLOW_DOWN_CHG
		// 这里会出现回退的现象，就是末端，断开管子瞬间，电压下降200mV(类似)，此时SOC已经100%，
		// 但是由于电流计算是有权重的，变为0可能需要几秒，此时会回退到98，也即从100-98
		// 如果执行以上的几个情况，这个就不会执行，
		if (SOC_Calculate_Element.u8SOC_Now >= 98 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol)
		{
			// SOC_Calculate_Element.u8SOC_Now = 98;
			SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now; // SOC保持不变
			SOC_Calculate_Element.u32CapChange = 0;							   // 把这个累加量清零便可，还有这个漏洞，会回退1
			SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		}
#endif

		if (su16_SocDsgCal_L1_Tcnt)
			su16_SocDsgCal_L1_Tcnt = 0;
		if (su16_SocDsgCal_L2_Tcnt)
			su16_SocDsgCal_L2_Tcnt = 0;
		if (su16_SocDsgCal_L3_Tcnt)
			su16_SocDsgCal_L3_Tcnt = 0;
		if (su16_SocDsgCal_L4_Tcnt)
			su16_SocDsgCal_L4_Tcnt = 0;
		break;

	case CurDSG:
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol + 100 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol && SOC_Calculate_Element.u8SOC_Now > 5)
		{
			if (++su16_SocDsgCal_L1_Tcnt >= 10)
			{ // 第一级校准
				su16_SocDsgCal_L1_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now -= 1;
				SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol && SOC_Calculate_Element.u8SOC_Now > 0)
		{ // 我也不知道为什么要5%，想想，直接0%，与下面两个行成闭循环
			if (SOC_Calculate_Element.u8SOC_Now < 5)
			{ // 第二级校准
				if (++su16_SocDsgCal_L2_Tcnt >= 8)
				{										  // 电科大电流还是有一定的概率留下1%，从10改为8吧。
					su16_SocDsgCal_L2_Tcnt = 0;			  // 但是兼顾小电流能放久一些，不能改为6
					SOC_Calculate_Element.u8SOC_Now -= 1; // 客户好像对放电末端，如果只剩2%以内貌似可以接受，但是充电必须100%
					SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
			else
			{ // 快没电了，还有很大的SOC
				if (++su16_SocDsgCal_L3_Tcnt >= 4)
				{ // 第三级校准
					su16_SocDsgCal_L3_Tcnt = 0;
					SOC_Calculate_Element.u8SOC_Now -= 1;
					SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
		}

		// 这是基于放电必须能达到0%的终极做法，2S - 1%
		// 但实际上放电要求没充电高
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol - 50 && SOC_Calculate_Element.u8SOC_Now > 0)
		{
			if (++su16_SocDsgCal_L4_Tcnt >= 2)
			{
				su16_SocDsgCal_L4_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now -= 1;
				SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
			}
		}

		if (SOC_Calculate_Element.u8SOC_Now <= 1 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol)
		{
			// SOC_Calculate_Element.u8SOC_Now = 2;
			SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now; // SOC保持不变
			SOC_Calculate_Element.u32CapChange = 0;							   // 把这个累加量清零便可，还有这个漏洞，会回退1
			SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		}

		if (su16_SocChgCal_L1_Tcnt)
			su16_SocChgCal_L1_Tcnt = 0;
		if (su16_SocChgCal_L2_Tcnt)
			su16_SocChgCal_L2_Tcnt = 0;
		if (su16_SocChgCal_L3_Tcnt)
			su16_SocChgCal_L3_Tcnt = 0;
		if (su16_SocChgCal_L4_Tcnt)
			su16_SocChgCal_L4_Tcnt = 0;
		break;

	default:
		break;
	}
}
#endif

#if 1
void CorrectionTerminal_CV(enum _CUR CurrentType)
{
	static uint16_t su16_SocChgCal_L1_Tcnt = 0;
	static uint16_t su16_SocChgCal_L2_Tcnt = 0;
	static uint16_t su16_SocChgCal_L3_Tcnt = 0;
	static uint16_t su16_SocChgCal_L4_Tcnt = 0;

	static uint16_t su16_SocDsgCal_L1_Tcnt = 0;
	static uint16_t su16_SocDsgCal_L2_Tcnt = 0;
	static uint16_t su16_SocDsgCal_L3_Tcnt = 0;
	static uint16_t su16_SocDsgCal_L4_Tcnt = 0;
	switch (CurrentType)
	{
	case CurCHG:
		if (VCELLMAX >= SOC_100_VAL - 100 && VCELLMAX < SOC_100_VAL && get_soc_real() < 95)
		{ // ????????????????Σ?????????95%????
			if (++su16_SocChgCal_L1_Tcnt >= 10)
			{
				su16_SocChgCal_L1_Tcnt = 0;
				Inc_real_soc();
			}
		}
		else if (VCELLMAX >= SOC_100_VAL && get_soc_real() < 100)
		{
			if (get_soc_real() > 95)
			{
				if (++su16_SocChgCal_L2_Tcnt >= 8)
				{
					su16_SocChgCal_L2_Tcnt = 0;
					Inc_real_soc();
				}
			}
			else
			{
				if (++su16_SocChgCal_L3_Tcnt >= 4)
				{
					su16_SocChgCal_L3_Tcnt = 0;
					Inc_real_soc();
				}
			}
		}

		// ???????????????100%???????????2S + 1%
		if (VCELLMAX >= SOC_100_VAL + 50 && get_soc_real() < 100)
		{
			if (++su16_SocChgCal_L4_Tcnt >= 2)
			{
				su16_SocChgCal_L4_Tcnt = 0;
				Inc_real_soc();
			}
		}

#ifdef _CAL_SLOW_DOWN_CHG
		if (get_soc_real() >= 99 && VCELLMAX < SOC_100_VAL)
		{
			// SOC_Calculate_Element.u8SOC_Now = 98;
			SOC_Calculate_Element.u8SOC_Now = get_soc_real(); // SOC???????
			SOC_Calculate_Element.u32CapChange = 0;			  // ??????????????????????????????????1
			SOC_Calculate_Element.u32CapNow = (UINT32)get_soc_real() * SOC_Calculate_Element.u32CapFull / 100;
		}
#endif

		su16_SocDsgCal_L1_Tcnt = 0;
		su16_SocDsgCal_L2_Tcnt = 0;
		su16_SocDsgCal_L3_Tcnt = 0;
		su16_SocDsgCal_L4_Tcnt = 0;
		break;

	case CurDSG:
		//???容量加速？？？
		SOC_Calculate_Element.acc_cap_delta = 1;

		if (VCELLMIN < SOC_0_VAL + 200)
		{
			if (VCELLMIN < SOC_0_VAL)
			{
				if (get_soc_real() > 0)
				{
					SOC_Calculate_Element.acc_cap_delta = 3;

					su16_SocDsgCal_L4_Tcnt += g_stCellInfoReport.u16IDischg;

					if (su16_SocDsgCal_L4_Tcnt >= time_soc1_100_100mA_unit)
					{
						su16_SocDsgCal_L4_Tcnt = 0;
						Dec_real_soc();
					}
				}
			}
			else if (VCELLMIN < SOC_0_VAL + 50)
			{
				if (get_soc_real() > 5)
				{
					SOC_Calculate_Element.acc_cap_delta = 2;

					su16_SocDsgCal_L3_Tcnt += g_stCellInfoReport.u16IDischg;

					if (su16_SocDsgCal_L3_Tcnt >= time_soc1_100_100mA_unit)
					{
						su16_SocDsgCal_L3_Tcnt = 0;
						Dec_real_soc();
					}
				}
			}
			else if (VCELLMIN < SOC_0_VAL + 100)
			{
				if (get_soc_real() > 10)
				{
					SOC_Calculate_Element.acc_cap_delta = 1.2;

					su16_SocDsgCal_L2_Tcnt += g_stCellInfoReport.u16IDischg;

					if (su16_SocDsgCal_L2_Tcnt >= time_soc1_100_100mA_unit)
					{
						su16_SocDsgCal_L2_Tcnt = 0;
						Dec_real_soc();
					}
				}
			}
			else
			{
				if (get_soc_real() > 20)
				{
					SOC_Calculate_Element.acc_cap_delta = 1.1;
					su16_SocDsgCal_L1_Tcnt += g_stCellInfoReport.u16IDischg;

					if (su16_SocDsgCal_L1_Tcnt >= time_soc1_100_100mA_unit)
					{
						su16_SocDsgCal_L1_Tcnt = 0;
						Dec_real_soc();
					}
				}
			}

			if (get_soc_real() <= 1 && VCELLMIN > SOC_0_VAL)
			{
				SOC_Calculate_Element.u8SOC_Now = get_soc_real(); // SOC???????
				SOC_Calculate_Element.u32CapChange = 0;			  // ??????????????????????????????????1
				SOC_Calculate_Element.u32CapNow = (UINT32)get_soc_real() * SOC_Calculate_Element.u32CapFull / 100;
			}
		}

		su16_SocChgCal_L1_Tcnt = 0;
		su16_SocChgCal_L2_Tcnt = 0;
		su16_SocChgCal_L3_Tcnt = 0;
		su16_SocChgCal_L4_Tcnt = 0;
		break;

	default:
		break;
	}
}
#endif

// 末端大电流恒流充，调用的函数
// 本来打算合成一个函数，但是想想后续可能会有不同的策略，决定分开
// 多级保护，有个BUG，就是电压上涨太快，算不过来
void CorrectionTerminal_CC(enum _CUR CurrentType)
{
}

void Correction_Terminal(enum _CUR CurrentType)
{

	switch (CurrentType)
	{
	case CurCHG:
		switch (SOC_Enhance_Element.u8_LargeCurFlag_Chg)
		{
		case 0:
			CorrectionTerminal_CV(CurrentType);
			break;
		case 1:
			CorrectionTerminal_CC(CurrentType);
			break;
		default:
			break;
		}
		break;

	case CurDSG:
		switch (SOC_Enhance_Element.u8_LargeCurFlag_Dsg)
		{
		case 0:
			CorrectionTerminal_CV(CurrentType);
			break;
		case 1:
			CorrectionTerminal_CC(CurrentType);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

// 写完这个函数，EEPROM那里要记得补充
// 这个函数能解决，电池一致性不好，用久，电池衰减的问题。
// 一个循环，指的是，先把电池目前的电放完，再从0电到100电。
// 两个循环，指的是，从0电到100电。然后到0电，再到100电。
void Correction_CapacityFull(void)
{
}

void SOC_Cont_AH_Int_CHG(void)
{
	UINT32 C_change_per;
	static UINT8 s_u8_CHG200msCnt = 0;
	static UINT8 s_u8_Transfer200msCnt = 0;
	if (SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG)
	{
		// if(g_stCellInfoReport.u16Ichg > 0) {
		if (++s_u8_CHG200msCnt >= 5)
		{
			s_u8_CHG200msCnt = 0;
			SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 1;
		}
		if (s_u8_Transfer200msCnt)
			s_u8_Transfer200msCnt = 0;
	}
	else
	{
		if (++s_u8_Transfer200msCnt >= 2)
		{ // 防止瞬间跳动问题
			s_u8_Transfer200msCnt = 0;
			s_u8_CHG200msCnt = 0;
			SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
			return;
		}
		--s_u8_CHG200msCnt;
	}

#if 1 // 原来的计算方式着实太拖沓，下面的三句搞定，还清晰明了，例如，容量没到100%前，都是99%，到达那一瞬间才是100%
	  // 这个的效果和优化的没啥差别，基于放电没操作，这个也不改了吧。
	if (SOC_Calculate_Element.u8CHG_AHCalcu_Flag)
	{
		Correction_Terminal(CurCHG);
		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Ichg * 1+50)/100;	//As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Ichg * 1+50)/100;  			//剩余容量实时跟踪
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Ichg * 1; // As*10*100(库伦效率100)
		SOC_Calculate_Element.u32CapNow += (UINT32)SOC_Enhance_Element.u16_Ichg * 1;	// 剩余容量实时跟踪

		if (SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFactory)
			SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
		C_change_per = SOC_Calculate_Element.u32CapChange * 100 / SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old + C_change_per;
		if (SOC_Calculate_Element.u8SOC_Now > 100)
			SOC_Calculate_Element.u8SOC_Now = 100;
		SOC_Calculate_Element.u32CapChange = (((SOC_Calculate_Element.u32CapChange * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100;
		SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;

		// 计算实际容量专用值。
		SOC_Calculate_Element.u32CapFull_Cal_As += (UINT32)SOC_Enhance_Element.u16_Ichg * 1;
	}
#endif

#if 0
	if(SOC_Calculate_Element.u8CHG_AHCalcu_Flag) {
		Correction_Terminal(CurCHG);
		SOC_Calculate_Element.u32CapNow += (UINT32)SOC_Enhance_Element.u16_Ichg * 1;	//剩余容量实时跟踪
		if(SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFull) SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFull;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u32CapNow * 100 / SOC_Calculate_Element.u32CapFull;
		SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
		SOC_Calculate_Element.Last_Had_Done = SOC_CALI_CONT_CHG;
	}
#endif
}

void SOC_Cont_AH_Int_DSG(void)
{
	UINT32 C_change_per;
	static UINT8 s_u8_DSG200msCnt = 0;
	static UINT8 s_u8_Transfer200msCnt = 0;
	if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		if (++s_u8_DSG200msCnt >= 5)
		{
			s_u8_DSG200msCnt = 0;
			SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 1;
		}
		if (s_u8_Transfer200msCnt)
			s_u8_Transfer200msCnt = 0;
	}
	else
	{
		if (++s_u8_Transfer200msCnt >= 2)
		{
			s_u8_Transfer200msCnt = 0;
			s_u8_DSG200msCnt = 0;
			SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
			return;
		}
		--s_u8_DSG200msCnt;
	}

	// todo 自耗 单位？？？
#if 1 // 这个计算方式还是妥一些，满减1%，SOC才显示99，客户体验会更好一些
	if (SOC_Calculate_Element.u8DSG_AHCalcu_Flag)
	{
		Correction_Terminal(CurDSG);

		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; //As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow-= ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; 	//剩余容量实时跟踪
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		// SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Enhance_Element.u16_Idsg * 1 * SOC_Calculate_Element.acc_cap_delta;
		if(SOC_Enhance_Element.u16_Idsg)
			SOC_Calculate_Element.delata_cap = SOC_Calculate_Element.acc_cap_delta * SOC_Enhance_Element.u16_Idsg * 1;
		else
			SOC_Calculate_Element.delata_cap = SOC_Calculate_Element.acc_cap_delta * SOC_Calculate_Element.silent_power * 1 * 60;
		SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Calculate_Element.delata_cap;

		if (SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFactory)
			SOC_Calculate_Element.u32CapNow = 0;
		C_change_per = SOC_Calculate_Element.u32CapChange * 100 / SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old - C_change_per;
		if (SOC_Calculate_Element.u8SOC_Now > 100)
			SOC_Calculate_Element.u8SOC_Now = 0;
		SOC_Calculate_Element.u32CapChange = (((SOC_Calculate_Element.u32CapChange * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100; // 四舍五入，关键
		SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

		// 循环次数统计
		// 如果是SOC=0还在疯狂减的话，在校准期间会出现循环次数统计出错，特别是标称容量小，实际容量特别大的时候
		// 上面的也是一个BUG，通过循环次数暴露出来了。
		if (SOC_Calculate_Element.u8SOC_Now != 0)
		{
			SOC_Calculate_Element.u8DSG_SOC_Int += C_change_per;
			// SOC_Calculate_Element.u8DSG_SOC_Int += 1;
			if (SOC_Calculate_Element.u8DSG_SOC_Int >= 80)
			{
				SOC_Calculate_Element.u8DSG_SOC_Int = 0;
				SOC_Calculate_Element.u32Cycle_times += 100;
			}
		}
	}
#endif
}

void SOC_State_Transfer(void)
{
	static UINT8 s_u8SOC_State_CHG = 0;
	static UINT8 s_u8SOC_State_DSG = 0;
	static UINT8 s_u8SOC_State_OCV = 0;
	if (SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG)
	{
		if (++s_u8SOC_State_CHG >= 3)
		{
			s_u8SOC_State_CHG = 0;
			SOC_Cali_Flag = SOC_CALI_CONT_CHG;
		}
		if (s_u8SOC_State_DSG)
			s_u8SOC_State_DSG = 0;
		if (s_u8SOC_State_OCV)
			s_u8SOC_State_OCV = 0;
	}
	else if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		if (++s_u8SOC_State_DSG >= 3)
		{
			s_u8SOC_State_DSG = 0;
			SOC_Cali_Flag = SOC_CALI_CONT_DSG;
		}
		if (s_u8SOC_State_CHG)
			s_u8SOC_State_CHG = 0;
		if (s_u8SOC_State_OCV)
			s_u8SOC_State_OCV = 0;
	}
	else
	{
		if (++s_u8SOC_State_OCV >= 3)
		{
			s_u8SOC_State_OCV = 0;
		}
		if (s_u8SOC_State_CHG)
			s_u8SOC_State_CHG = 0;
		if (s_u8SOC_State_DSG)
			s_u8SOC_State_DSG = 0;
	}
}

void SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command)
{
	UINT16 temp = 0;

	switch (Command)
	{
	case EEPROM_DATA_REFRESH:
		SOC_E2prom_Par.u16_SOC_Temp = 0;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SOC_Temp, SOC_E2prom_Par.u16_SOC_Temp);

		*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp) = SOC_Calculate_Element.u8SOC_Now;
		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp),
								*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp));

		SOC_E2prom_Par.u16_DsgSOC_Temp = 0;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_DsgSOC_Temp, SOC_E2prom_Par.u16_DsgSOC_Temp);

		*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp) = SOC_Calculate_Element.u8DSG_SOC_Int;
		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp),
								*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp));

		SOC_E2prom_Par.u16_Cycle_Times = SOC_Calculate_Element.u32Cycle_times / 100;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_Cycle_Times, SOC_E2prom_Par.u16_Cycle_Times);

		SOC_E2prom_Par.u16CapFull_Cal_Ah = SOC_Calculate_Element.u32CapFactory / 3600;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16CapFull_Cal_Ah, SOC_E2prom_Par.u16CapFull_Cal_Ah);

		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG; // 回归到PowerOFF地方取
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag, SOC_E2prom_Par.u16_SeriousFaultFlag);
		break;

	case EEPROM_DATA_READ:
		// SOC_E2prom_Par.u16_SeriousFaultFlag = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag);	//不能在这里
		// 取SOC
		SOC_E2prom_Par.u16_SOC_Temp = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SOC_Temp);
		if (SOC_E2prom_Par.u16_SOC_Temp < 5)
		{
			*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp) =
				ReadEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp));
		}
		else
		{
			SOC_E2prom_Par.u16_SOC_Temp = 0;
			WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SOC_Temp, SOC_E2prom_Par.u16_SOC_Temp);
			// 取SOC
			*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp) = Get_OpenCircuit_Value();
		}

		// 取循环下降积累量
		SOC_E2prom_Par.u16_DsgSOC_Temp = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_DsgSOC_Temp);
		if (SOC_E2prom_Par.u16_DsgSOC_Temp < 3)
		{
			*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp) =
				ReadEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp));
		}
		else
		{
			SOC_E2prom_Par.u16_DsgSOC_Temp = 0;
			WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_DsgSOC_Temp, SOC_E2prom_Par.u16_DsgSOC_Temp);
			// 取循环下降积累量，初始化为0
			*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp) = 0;
		}

		// 取循环次数
		temp = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_Cycle_Times);
		if (temp != 0xFFFF)
		{
			SOC_E2prom_Par.u16_Cycle_Times = temp;
		}
		else
		{
			// 有问题
			SOC_E2prom_Par.u16_Cycle_Times = SOC_Calculate_Element.u32Cycle_times / 100;
			WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_Cycle_Times, SOC_E2prom_Par.u16_Cycle_Times);
		}

		// 取满电容量
		temp = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16CapFull_Cal_Ah);
		if (temp != 0xFFFF)
		{
			SOC_E2prom_Par.u16CapFull_Cal_Ah = temp;
		}
		else
		{
			// 有问题
			SOC_E2prom_Par.u16CapFull_Cal_Ah = SOC_Calculate_Element.u32CapFactory / 3600;
			WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16CapFull_Cal_Ah, SOC_E2prom_Par.u16CapFull_Cal_Ah);
		}
		break;

	default:
		break;
	}

	/*
	//这段代码是为了解决，以前，没有循环次数，这次加上循环次数，但是读出来的SOC_E2prom_Par.u16_DsgSOC_Temp为
	//0xFFFF，然后下面ReadEEPROM_Word_WithZone()就溢出导致硬件错误了。
	//后续：这个写法其实有点问题，会把原来的数据全部清空，不太好。
	if(FaultFlag) {
		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_STORE_RESET;		//重新处理数据
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag, SOC_E2prom_Par.u16_SeriousFaultFlag);
		NVIC_SystemReset();
	}
	*/
}

void SOC_Update_StartUp(void)
{
	switch (SOC_E2prom_Par.u16_SeriousFaultFlag)
	{
	case EEPROM_VALUE_POWEROFF_FLAG: // 别的情况就在掉电位置取
		SOC_DealEEPROM_Data(EEPROM_DATA_READ);
		SOC_Calculate_Element.u8SOC_Now = (UINT8) * (&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp);
		SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8) * (&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp);
		SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_E2prom_Par.u16_Cycle_Times * 100;
		SOC_Calculate_Element.u32CapFull = (UINT32)SOC_E2prom_Par.u16CapFull_Cal_Ah * 3600;
		break;

	case EEPROM_VALUE_SLEEP_FLAG: // 如果出现休眠，会在这里取，这个其实可以删掉，意义不大
		SOC_DealEEPROM_Data(EEPROM_DATA_READ);
		SOC_Calculate_Element.u8SOC_Now = (UINT8) * (&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp);
		SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8) * (&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp);
		SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_E2prom_Par.u16_Cycle_Times * 100;
		SOC_Calculate_Element.u32CapFull = (UINT32)SOC_E2prom_Par.u16CapFull_Cal_Ah * 3600;

		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG; // 回归到PowerOFF地方取
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag, SOC_E2prom_Par.u16_SeriousFaultFlag);
		break;

	case EEPROM_VALUE_DATA_UPDATE_FLAG:
		// 这几个数的EEPROM可以不管，如果不一样自己更新就vans了
		switch (SOC_Enhance_Element.u16_RefreshData_Flag)
		{
		case 1:
			SOC_Calculate_Element.u8SOC_Now = Get_OpenCircuit_Value();
			break;

		case 2: // SOC归零类型，改为循环次数归初始化
				// 添加容量初始化
			// SOC_Calculate_Element.u8SOC_Now = 0;
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
			SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600;
			SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
			SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;
			// 上面SOC_Calculate_Element.u32CapFactory已经初始化
			SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
			break;

		case 3:
			SOC_Calculate_Element.u8SOC_Now = SOC_Enhance_Element.u8_SetSocOnce;
			break;

		default:
			break;
		}
		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
		SOC_Calculate_Element.u8_DataUpdateOK = 1;
		break;

	default:
		// 第一次上电的值不一定是0xFFFF，有可能0x0000？
		// 这个只会在第一次烧代码才会运行那么一次		，第二次上电不会用这个
		// 第一次烧代码，上位机升级都会跑这个，用keil或者脱机烧写工具第二次烧写只会跑POWEROFF的路，目前这个问题无解
		// SOC_Calculate_Element.u8SOC_Now = GetEndValue(SOC_Table_LiFePO, (UINT16)SOC_Size_LiFePO, (UINT16)g_stCellInfoReport.u16VCellMin);
		// SOC_Calculate_Element.u8SOC_Now = Get_OpenCircuit_Value();
		SOC_Calculate_Element.u8SOC_Now = 80;
		// InitSOC_IntEnhance()已处理这两个
		// SOC_Calculate_Element.u8DSG_SOC_Int = 0;
		// SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever*100;
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;

		// 初始化EEPROM的值
		SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
		break;
	}

	// time_soc1_100_100mA_unit = (float)SOC_Calculate_Element.u32CapFactory / 3600 / 10 / 0.1 * 3600 / 100;
	time_soc1_100_100mA_unit = (float)SOC_Calculate_Element.u32CapFactory / 100;

	SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
	SOC_Enhance_Element.u16_SOC_InitOver = 1; // Soc初始化完毕
	SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
}

/*
1，存SOC值，变化1%即存
2，循环次数下降数值，少1%即存
3，循环次数，多一个循环即存
4，关于这些EEPROM的数值的问题
   A，如果是第一次用这个EEPROM怎么处理？
   B，如果期间换电池了呢？
5，目前就这三个需要处理，后续关于运行期间掉电怎么处理后续再说，系数之类的一定要存的
*/
void SOC_EEPROM_Deal_Monitor(void)
{
	static UINT8 su8_TimeCnt = 0;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{ // 初始化完才开始这个函数
		return;
	}

	if (++su8_TimeCnt < 5)
	{
		return;
	}
	su8_TimeCnt = 0;

	if (SOC_Calculate_Element.u8SOC_Now != *(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp))
	{
		if (++SOC_E2prom_Par.u16_SOC_Temp >= 4)
		{
			SOC_E2prom_Par.u16_SOC_Temp = 0;
		}
		*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp) = SOC_Calculate_Element.u8SOC_Now;

		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp),
								*(&SOC_E2prom_Par.u16_SOC_E2P0 + SOC_E2prom_Par.u16_SOC_Temp));
		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_SOC_Temp), SOC_E2prom_Par.u16_SOC_Temp);
	}

	if (SOC_Calculate_Element.u8DSG_SOC_Int != *(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp))
	{
		if (++SOC_E2prom_Par.u16_DsgSOC_Temp >= 2)
		{
			SOC_E2prom_Par.u16_DsgSOC_Temp = 0;
		}
		*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp) = SOC_Calculate_Element.u8DSG_SOC_Int;

		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp),
								*(&SOC_E2prom_Par.u16_DsgSOC_Int0 + SOC_E2prom_Par.u16_DsgSOC_Temp));
		WriteEEPROM_Word_NoZone(*(&SOC_E2prom_Adress.u16_DsgSOC_Temp), SOC_E2prom_Par.u16_DsgSOC_Temp);
	}

	if ((UINT16)(SOC_Calculate_Element.u32Cycle_times / 100) != SOC_E2prom_Par.u16_Cycle_Times)
	{
		SOC_E2prom_Par.u16_Cycle_Times = SOC_Calculate_Element.u32Cycle_times / 100;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_Cycle_Times, SOC_E2prom_Par.u16_Cycle_Times);
	}

	// 这个不能乘，不然就经常写了
	if ((UINT16)(SOC_Calculate_Element.u32CapFull / 3600) != SOC_E2prom_Par.u16CapFull_Cal_Ah)
	{
		SOC_E2prom_Par.u16CapFull_Cal_Ah = SOC_Calculate_Element.u32CapFull / 3600;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16CapFull_Cal_Ah, SOC_E2prom_Par.u16CapFull_Cal_Ah);
	}
}

void SOC_RefreshData_Monitor(void)
{
	static UINT8 su8_DataRefreshFlag = 0;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	switch (su8_DataRefreshFlag)
	{
	case 0:
		if (SOC_Enhance_Element.u16_RefreshData_Flag)
		{
			SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_DATA_UPDATE_FLAG;
			SOC_Cali_Flag = SOC_CALI_STARTUP;
			su8_DataRefreshFlag = 1;
		}
		break;

	case 1:
		if (SOC_Calculate_Element.u8_DataUpdateOK == 1)
		{											   // 3个地方初始化
			SOC_Calculate_Element.u8_DataUpdateOK = 0; // 这个思路留着。不改
			SOC_Enhance_Element.u16_RefreshData_Flag = 0;
			su8_DataRefreshFlag = 0;
		}
		break;
	default:
		break;
	}
}

void SOC_Result_Pass(void)
{
	static UINT8 su8_TimeCnt = 0;
	if (++su8_TimeCnt < 5)
	{
		return;
	}
	su8_TimeCnt = 0;

	SOC_Enhance_Element.u8_SOC = SOC_Calculate_Element.u8SOC_Now;
	if (SOC_Calculate_Element.u32CapFull >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Enhance_Element.u8_SOH = 100;
	}
	else
	{
		SOC_Enhance_Element.u8_SOH = (UINT8)((100 * SOC_Calculate_Element.u32CapFull / SOC_Calculate_Element.u32CapFactory) & 0xFF);
	}
	SOC_Enhance_Element.u16_CapacityNow = SOC_Calculate_Element.u32CapNow * 1 / 360;
	SOC_Enhance_Element.u16_CapacityFull = SOC_Calculate_Element.u32CapFull * 1 / 360;
	SOC_Enhance_Element.u16_CapacityFactory = SOC_Calculate_Element.u32CapFactory * 1 / 360;
	SOC_Enhance_Element.u16_Cycle_times = SOC_Calculate_Element.u32Cycle_times / 100;

	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Calculate_Element.u8DSG_SOC_Int; // 留着，自己知道
}

void SOC_Data_Filter(void)
{
	static UINT8 su8_StartUp_Flag = 0;

	static UINT16 su16_Filter_Tcnt1 = 0;

	static UINT16 su16_VcellMax_hold = 0;
	static UINT16 su16_Vcellmin_hold = 0;

	// 电压突变滤波。例如5V和500mV，则下面计算就出问题了，满电容量变0或者很小的值。
	// 如果真的是的话，下面计算瞬间让满电容量和当前容量为0，问题不大。
	// 如果能通过这个滤波这种情况一般只会在电容爆掉，啥的，硬件出问题。
	// 如果通不过，就是瞬间变化，过滤掉不需要管。(有可能是采样，或者AFE出问题，海诚hs012出现)
	if (ModulusSubb(SOC_Enhance_Element.u16_VCellMax, SOC_Enhance_Element.u16_VCellMin) < 600)
	{
		su16_VcellMax_hold = SOC_Enhance_Element.u16_VCellMax;
		su16_Vcellmin_hold = SOC_Enhance_Element.u16_VCellMin;
		su8_StartUp_Flag = 1;
		if (su16_Filter_Tcnt1)
			su16_Filter_Tcnt1 = 0;
	}
	else
	{
		if (++su16_Filter_Tcnt1 < 5 * 10)
		{ // 延时10s
			if (!su8_StartUp_Flag)
			{ // 如果开局就进来这里，则赋值一下。
				su16_VcellMax_hold = SOC_Enhance_Element.u16_VCellMax;
				su16_Vcellmin_hold = SOC_Enhance_Element.u16_VCellMin;
			}
			SOC_Enhance_Element.u16_VCellMax = su16_VcellMax_hold;
			SOC_Enhance_Element.u16_VCellMin = su16_Vcellmin_hold;
		}
		else
		{
			su16_Filter_Tcnt1 = 50;
		}
	}

	// TODO
	// 还有两种突变。
	// 1，突然整体暴涨几百mV。换电池。那得重新循环学习就好。
	// 2，电流突变，这个正常。
}

void InitSOC_IntEnhance(void)
{
	UINT8 i;

	// SOC_Calculate_Element.C0 = (UINT32)OtherElement.u16Soc_Ah*3600 *10;  //开始不加(UINT32)出现严重计算错误
	// 外部获取的数据初始化
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600; // 去掉*10;改单位这里进来的单位稍微修改一下便可，如此快捷
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
	SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;

	// SOC的EEPROM位置初始化
	for (i = 0; i < E2P_AdressNum; ++i)
	{
		*(&SOC_E2prom_Adress.u16_SOC_E2P0 + i) = SOC_Enhance_Element.SOC_E2P_Adress[i];
	}

	SOC_Calculate_Element.u32CapChange = 0;
	SOC_Calculate_Element.u8OCV_Cali_Flag = 0; // 第一次写置1出现了开机严重错误的问题
	SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
	SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

	SOC_Calculate_Element.u8SOC_Now = 0; // 以上均为0，因为模拟前端还没读回电压
	SOC_Calculate_Element.u32CapNow = 0;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0;
	SOC_Calculate_Element.u32CapFull = 0;

#if !defined(__ONLY_UPDATE_NO_REFREH_PARAM__)
	SOC_E2prom_Par.u16_SeriousFaultFlag = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag);
#else
	SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
#endif

	SOC_Calculate_Element.silent_power = 0.1;
	SOC_Calculate_Element.acc_cap_delta = 1;

	SOC_Enhance_Element.u16_SOC_InitOver = 0; // 对外标志位初始化
	SOC_Cali_Flag = SOC_CALI_STARTUP;		  // 跳到下一步
}

void soc_cali(void)
{
#if 0

#ifdef _SOC_OCV_Fix2_func_
	SOC_OCV_Fix2();
#endif

#endif
	extern enum status_sys sys_status;

#if 0
	//???只触发一次
	if ((sys_status = s_CHG) && (g_stCellInfoReport.u16VCellTotle * 10 >= 4100 * SNum) && (isCOV || g_stCellInfoReport.u16VCellMax >= SOC_100_VAL))
	{
		set_soc_param(100, 1, 1);
	}
	// else if ((g_stCellInfoReport.u16VCellTotle * 10 <= 2900 * SNum) && (gcel))
	else if ((sys_status = s_DSG) && isCUV && (g_stCellInfoReport.u16VCellMin >= 2000))
	{
		set_soc_param(0, 1, 1);
	}
#endif
}
/*
>>后记：
1，这个做法会出现一个问题，SOC加速，容量膨胀，然后静置之后，SOC保持不变，但是满电容量减少(因为满电容量是实打实计算的)。
   这样，剩余容量就突然减少了，会有分歧。如果此时SOC计算还没有100%(差距过大，当前满电容量太大)，直到40%这个样子，剩余容量会更少。
2，回到实际情况，用久衰减的电池，也会出现同样的情况，但是末端一定要小电流操作，使其充到100%。
   这样的话，当前容量虽然减少了，但是乘以100%，也差距不会太大。
3，结合1和2，容量最好不要显示，只显示SOC，SOH和出厂容量为妙。
*/
void SOC_IntEnhance_Ctrl(UINT8 TimeBase_200ms)
{
	static uint16_t silent_power_delay = 0;

	SOC_Calculate_Element.acc_cap_delta = 1.0;

	switch (SOC_Cali_Flag)
	{
	case SOC_CALI_DATA_INIT:
		InitSOC_IntEnhance();
		break;
	case SOC_CALI_STARTUP:
		SOC_Update_StartUp();
		break;
	case SOC_CALI_STATE_TRANSFER:
		SOC_State_Transfer();
		break;
	case SOC_CALI_CONT_CHG:
		SOC_Cont_AH_Int_CHG();
		break;
	case SOC_CALI_CONT_DSG:
		SOC_Cont_AH_Int_DSG();
		break;
	default:
		SOC_Cali_Flag = SOC_CALI_STARTUP;
		break;
	}

	if (++silent_power_delay >= 5 * 60)
	{
		UINT32 C_change_per;
		silent_power_delay = 0;

		// SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 1;
		// SOC_Calculate_Element.delata_cap = SOC_Calculate_Element.acc_cap_delta * SOC_Calculate_Element.silent_power * 1 * 60;
		// SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Calculate_Element.delata_cap;
		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; //As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow-= ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; 	//剩余容量实时跟踪
		// SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		// SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Enhance_Element.u16_Idsg * 1 * SOC_Calculate_Element.acc_cap_delta;
		SOC_Calculate_Element.delata_cap = SOC_Calculate_Element.acc_cap_delta * SOC_Calculate_Element.silent_power * 1 * 60;
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Calculate_Element.delata_cap;
		SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Calculate_Element.delata_cap;

		if (SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFactory)
			SOC_Calculate_Element.u32CapNow = 0;
		C_change_per = SOC_Calculate_Element.u32CapChange * 100 / SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old - C_change_per;
		if (SOC_Calculate_Element.u8SOC_Now > 100)
			SOC_Calculate_Element.u8SOC_Now = 0;
		SOC_Calculate_Element.u32CapChange = (((SOC_Calculate_Element.u32CapChange * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100; // 四舍五入，关键
		SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;
	}

	soc_cali();

	SOC_EEPROM_Deal_Monitor();
	SOC_RefreshData_Monitor(); // 有顺序，放最后>>应该没顺序了
	SOC_Result_Pass();
	// Correction_CapacityFull();
}
