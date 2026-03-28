#include "SocEnhance.h"
#include "conf.h"

#define SOC_OCV_UPDATE  					3000     	//暂定200*6000 = 1200s = 20min
														//暂定200*3000 = 600s = 10min

#define SOC_VIRTUAL_CURRENT_CHG (UINT16)	2		//A*10，1和2都认为是0，带=号，0.2就开始算了
#define SOC_VIRTUAL_CURRENT_DSG (UINT16)	2		//A*10，1和2都认为是0，这个不能为0的同时，把=号判断上去，不然就会卡在DSG那里计算出不来。

#define DELAYB1000MS_5MIN					300		//默认通讯周期为1s一次
#define DELAYB1000MS_10MIN					600		//默认通讯周期为1s一次

//#define CHG_CUR_1C							2100	//A*10恒流充电为1C，恒压充电为1C-0.1C(SOC=95%)，涓流充电也为0.1C

#define EEPROM_VALUE_SLEEP_FLAG			((UINT16)0x1234)
#define EEPROM_VALUE_POWEROFF_FLAG		((UINT16)0x5678)
#define EEPROM_VALUE_DATA_UPDATE_FLAG 	((UINT16)0x9ABC)
#define EEPROM_VALUE_STORE_RESET 		((UINT16)0xFFFF)

//充电可以提前充满，但是不能卡死
//#define _CAL_SLOW_DOWN_CHG


typedef enum _CUR {
CurCHG = 0, CurDSG
}_Cur;


enum CHG_CURVE_STATUS {
	CHG_CURVE_STARTUP = 0,
	CHG_CURVE_BEGIN,
	CHG_CURVE_CONSTANT_CUR,
	CHG_CURVE_CONSTANT_VOR,
	CHG_CURVE_TRICKLE_CUR,
	CHG_CURVE_OVER,
	CHG_CURVE_ERROR_DEAL
};

enum SOC_CALI_STATE {
	SOC_CALI_DATA_INIT = 0,
	SOC_CALI_STARTUP,
	SOC_CALI_STATE_TRANSFER,
	SOC_CALI_CONT_CHG,
	SOC_CALI_CONT_DSG,
};

enum CAP_FULL_STATE {
	CAP_FULL_INIT = 0,
	CAP_FULL_STARTUP,
	CAP_FULL_CALCU,
	CAP_FULL_SUCCESS,
	CAP_FULL_FAIL,
};


enum EEPROM_COMMAND {
	EEPROM_DATA_REFRESH = 0,
	EEPROM_DATA_READ
};


struct SOC_CALCULATE_ELEMENT {	
	//InitSOC_IntEnhance赋值类型
	UINT32  u32CapFactory;  	//电池初始总容量(出厂容量)As*10 =        Ah*3600*10
	UINT32  u32CycleT_Limit;    //可循环次数
	//以下置零
	UINT32	u32CapChange;		//电池容量变化	   As*10，叠加类型
	UINT8   u8OCV_Cali_Flag;    //开路电压法可使用标志
	UINT8   u8CHG_AHCalcu_Flag;	//充电安时积分可使用标志
	UINT8   u8DSG_AHCalcu_Flag;	//放电安时积分可使用标志
	
	//InitSOC_IntEnhance赋值，其后SOC_Update_StartUp再次赋值类型
	UINT8   u8SOC_Now;          //当前电池SOC     0—100 为相对容量百分比
	UINT32  u32CapNow;		 	//电池剩余总容量As*10
	UINT8	u8DSG_SOC_Int;		//循环次数只算放电量，已放电量积累量百分比，90%算一个循环		
	UINT32  u32Cycle_times;     //循环次数*100，本来只打算用用一个变量直接叠加去处理，但是太损耗EEPROM发现不行
	UINT32  u32CapFull;	 		//电池衰减后总容量As*10(SOH)，我的显示SOH要改一改，算错了

	//运行过程长期修改类型
	UINT8   u8SOC_Old;          //初始SOC    0-100 为相对容量百分比
	//UINT8   u8a_BurnIn;         //老化因素α的修正系数，系数乘以100
	//UINT8   u8b_CapC;      		//电池容量修正因子δ，与充放电循环次数相关δ = f(Cycle_times)
	UINT8	u8_DataUpdateOK;	//更新记录
	UINT32  u32CapFull_Cal_As;	//长期运行，更新容量，As*10
};


struct SOC_ENHANCE_E2PROM_PAR {
	UINT16  u16_SOC_E2P0;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P1;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P2;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改
	UINT16  u16_SOC_E2P3;    		//保存最近的SOC，以用于上电即可显示，不能通过上位机修改

	UINT16  u16_SOC_Temp;			//记录哪个SOC是最新的
	UINT16  u16_DsgSOC_Int0;		//记录已放电量积累量百分比
	UINT16  u16_DsgSOC_Int1;		//记录已放电量积累量百分比
	UINT16  u16_DsgSOC_Temp;		//记录哪个电量积累量是最新的

	UINT16  u16_Cycle_Times;		//记录循环次数
	UINT16  Res1;					//上一次做的任务，虽然取消了，但是位置不能变，原版升级问题
	UINT16  Res5;					//上一次的纠正系数
	UINT16 	u16_SeriousFaultFlag;	//严重错误标志位保存

	UINT16  u16CapFull_Cal_Ah;		//Ah*10
	UINT16  Res2;					//Res2
	UINT16  Res3;					//Res3
	UINT16 	Res4;					//Res4
};


struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;				//对外交互结构体,lib文件的桥梁
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element;			//内部计算结构体
struct SOC_ENHANCE_E2PROM_PAR SOC_E2prom_Par;				//EEPROM保存关键数据结构体
struct SOC_ENHANCE_E2PROM_PAR SOC_E2prom_Adress;			//EEPROM地址结构体

enum SOC_CALI_STATE SOC_Cali_Flag = SOC_CALI_DATA_INIT;		//妈的，忘了这个？		SOC计算状态机，记得初始化
enum CAP_FULL_STATE CapFull_Cali_Flag = CAP_FULL_INIT;		//容量更新计算状态机。

UINT16 ChgValue = 0;
UINT16 DsgValue = 0;
//UINT16 SeriousFaultFlag = 0;

#define SOC_PERSIST_SLOT_WORDS          ((UINT8)7)
#define SOC_PERSIST_SLOT_COUNT          ((UINT8)2)
#define SOC_PERSIST_MAGIC               ((UINT16)0x534F)
#define SOC_INIT_CONFIDENCE_LOW         ((UINT8)1)
#define SOC_INIT_CONFIDENCE_MEDIUM      ((UINT8)2)
#define SOC_INIT_CONFIDENCE_HIGH        ((UINT8)3)
#define SOC_RESTORE_REASON_FALLBACK     ((UINT8)1)
#define SOC_RESTORE_REASON_OCV          ((UINT8)2)
#define SOC_RESTORE_REASON_STORE        ((UINT8)3)
#define SOC_RESTORE_REASON_PARAM        ((UINT8)4)
#define SOC_RESTORE_REASON_MANUAL       ((UINT8)5)

struct SOC_PERSIST_SNAPSHOT {
	UINT16 u16Magic;
	UINT16 u16Seq;
	UINT16 u16Soc;
	UINT16 u16DsgSocInt;
	UINT16 u16CycleTimes;
	UINT16 u16CapFullAh;
	UINT16 u16Checksum;
};

static const UINT8 SOC_PersistWordMap[SOC_PERSIST_SLOT_COUNT * SOC_PERSIST_SLOT_WORDS] = {
	0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 12, 13, 14
};

struct SOC_RUNTIME_STATE {
	UINT8 u8SocReal;
	UINT8 u8SocDisplay;
	UINT8 u8InitConfidence;
	UINT8 u8RestoreReason;
	UINT16 u16PersistSeq;
	UINT8 u8PersistSoc;
	UINT8 u8PersistDsg;
	UINT16 u16PersistCycle;
	UINT16 u16PersistCapFullAh;
	UINT8 u8PersistReady;
};

static struct SOC_RUNTIME_STATE SOC_Runtime_State;


//古瑞瓦特
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
    3336	,	100	,
    3332	,	90	,
    3330    ,   80  ,
    3327    ,   75  ,
    3316    ,   70  ,
    3301    ,   65  ,
    3294    ,   60  ,
    3291    ,   55  ,
    3290    ,   50  ,
    3288    ,   45  ,
    3286    ,   40  ,
    3279    ,   35  ,
    3266    ,   30  ,
    3254    ,   25  ,
    3236    ,   20  ,
    3212    ,   15  ,
    3198    ,   10  ,
    3112    ,    5  ,
    2526    ,    0  ,
    1000    ,    0  ,
    1000    ,    0  ,
};


//单位为mV和SOC
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
    4126	,	100	,
    4066	,	95	,
    4011    ,   90  ,
    3955    ,   85  ,
    3888    ,   80  ,
    3837    ,   75  ,
    3793    ,   70  ,
    3756    ,   65  ,
    3724    ,   60  ,
    3699    ,   55  ,
    3675    ,   50  ,
    3658    ,   45  ,
    3632    ,   40  ,
    3605    ,   35  ,
    3584    ,   30  ,
    3557    ,   25  ,
    3535    ,   20  ,
    3497    ,   15  ,
    3475    ,   10  ,
    3371    ,    5  ,
    3136    ,    0  ,
};


//单位为mV和SOC
const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
    3650	,	100	,
    3600	,	98	,
    3550    ,   95  ,
    3500    ,   92  ,
    3400    ,   90  ,
    3350    ,   87  ,
    3340    ,   85  ,
    3335    ,   82  ,
    3330    ,   80  ,
    3325    ,   78  ,
    3320    ,   75  ,
    3300    ,   70  ,
    3275    ,   65  ,
    3250    ,   60  ,
    3200    ,   50  ,
    3150    ,   45  ,
    3100    ,   30  ,
    3000    ,   20  ,
    2850    ,   10  ,
    2750    ,    5  ,
    2650    ,    0  ,
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

UINT8 Get_OpenCircuit_Value(void);
UINT8 isCHG(void);
UINT8 isDSG(void);

static UINT16 SOC_PersistAddr(UINT8 slot, UINT8 index)
{
	return SOC_Enhance_Element.SOC_E2P_Adress[SOC_PersistWordMap[(UINT16)slot * SOC_PERSIST_SLOT_WORDS + index]];
}

static UINT16 SOC_PersistChecksum(const struct SOC_PERSIST_SNAPSHOT *ptSnap)
{
	UINT32 sum = 0;
	sum += ptSnap->u16Magic;
	sum += ptSnap->u16Seq;
	sum += ptSnap->u16Soc;
	sum += ptSnap->u16DsgSocInt;
	sum += ptSnap->u16CycleTimes;
	sum += ptSnap->u16CapFullAh;
	return (UINT16)(sum & 0xFFFF);
}

static UINT8 SOC_LoadPersistSnapshot(UINT8 slot, struct SOC_PERSIST_SNAPSHOT *ptSnap)
{
	UINT8 i;
	UINT16 *pData;
	pData = &ptSnap->u16Magic;
	for (i = 0; i < SOC_PERSIST_SLOT_WORDS; ++i)
	{
		pData[i] = ReadEEPROM_Word_NoZone(SOC_PersistAddr(slot, i));
	}

	if (ptSnap->u16Magic != SOC_PERSIST_MAGIC)
	{
		return 0;
	}
	if (SOC_PersistChecksum(ptSnap) != ptSnap->u16Checksum)
	{
		return 0;
	}
	if (ptSnap->u16Soc > 100 || ptSnap->u16DsgSocInt > 100)
	{
		return 0;
	}
	if (ptSnap->u16CapFullAh == 0 || ptSnap->u16CapFullAh == 0xFFFF)
	{
		return 0;
	}
	return 1;
}

static UINT8 SOC_LoadLatestPersistSnapshot(struct SOC_PERSIST_SNAPSHOT *ptSnap)
{
	struct SOC_PERSIST_SNAPSHOT snap0;
	struct SOC_PERSIST_SNAPSHOT snap1;
	UINT8 valid0;
	UINT8 valid1;

	valid0 = SOC_LoadPersistSnapshot(0, &snap0);
	valid1 = SOC_LoadPersistSnapshot(1, &snap1);
	if (valid0 && valid1)
	{
		if ((UINT16)(snap1.u16Seq - snap0.u16Seq) < 0x8000)
		{
			*ptSnap = snap1;
		}
		else
		{
			*ptSnap = snap0;
		}
		return 1;
	}
	if (valid0)
	{
		*ptSnap = snap0;
		return 1;
	}
	if (valid1)
	{
		*ptSnap = snap1;
		return 1;
	}
	return 0;
}

static UINT8 SOC_LoadLegacySnapshot(struct SOC_PERSIST_SNAPSHOT *ptSnap)
{
	UINT16 temp;
	UINT16 soc_index;
	UINT16 dsg_index;
	UINT16 soc_value;
	UINT16 dsg_value;
	UINT16 cap_full_ah;

	soc_index = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[4]);
	if (soc_index >= 4)
	{
		return 0;
	}
	soc_value = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[soc_index]);
	if (soc_value > 100)
	{
		return 0;
	}

	dsg_index = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[7]);
	if (dsg_index >= 2)
	{
		dsg_value = 0;
	}
	else
	{
		dsg_value = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[5 + dsg_index]);
		if (dsg_value > 100)
		{
			dsg_value = 0;
		}
	}

	temp = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[8]);
	if (temp == 0xFFFF)
	{
		temp = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100);
	}

	cap_full_ah = ReadEEPROM_Word_NoZone(SOC_Enhance_Element.SOC_E2P_Adress[12]);
	if (cap_full_ah == 0xFFFF || cap_full_ah == 0)
	{
		cap_full_ah = (UINT16)(SOC_Calculate_Element.u32CapFactory / 3600);
	}

	ptSnap->u16Magic = SOC_PERSIST_MAGIC;
	ptSnap->u16Seq = 0;
	ptSnap->u16Soc = soc_value;
	ptSnap->u16DsgSocInt = dsg_value;
	ptSnap->u16CycleTimes = temp;
	ptSnap->u16CapFullAh = cap_full_ah;
	ptSnap->u16Checksum = SOC_PersistChecksum(ptSnap);
	return 1;
}

static UINT8 SOC_GetLimitedOcvSoc(void)
{
	UINT8 soc;
	if (SOC_Enhance_Element.u16_VCellMin < 2000 || SOC_Enhance_Element.u16_VCellMin > 4500)
	{
		return 50;
	}
	soc = Get_OpenCircuit_Value();
	if (soc > 100)
	{
		soc = 100;
	}
	return soc;
}

static UINT8 SOC_CanUseOcvAtStartup(void)
{
	if (SOC_Enhance_Element.u16_VCellMin < 2000 || SOC_Enhance_Element.u16_VCellMin > 4500)
	{
		return 0;
	}
	if (SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG)
	{
		return 0;
	}
	if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		return 0;
	}
	return 1;
}

static void SOC_UpdatePersistMirror(void)
{
	SOC_Runtime_State.u8PersistSoc = SOC_Calculate_Element.u8SOC_Now;
	SOC_Runtime_State.u8PersistDsg = SOC_Calculate_Element.u8DSG_SOC_Int;
	SOC_Runtime_State.u16PersistCycle = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100);
	SOC_Runtime_State.u16PersistCapFullAh = (UINT16)(SOC_Calculate_Element.u32CapFull / 3600);
	SOC_Runtime_State.u8PersistReady = 1;
}

static void SOC_ApplySnapshot(const struct SOC_PERSIST_SNAPSHOT *ptSnap)
{
	UINT32 cap_full;
	SOC_Calculate_Element.u8SOC_Now = (UINT8)ptSnap->u16Soc;
	SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)ptSnap->u16DsgSocInt;
	SOC_Calculate_Element.u32Cycle_times = (UINT32)ptSnap->u16CycleTimes * 100;
	cap_full = (UINT32)ptSnap->u16CapFullAh * 3600;
	if (cap_full == 0)
	{
		cap_full = SOC_Calculate_Element.u32CapFactory;
	}
	SOC_Calculate_Element.u32CapFull = cap_full;
	SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
	SOC_Runtime_State.u8SocReal = SOC_Calculate_Element.u8SOC_Now;
	SOC_Runtime_State.u8SocDisplay = SOC_Calculate_Element.u8SOC_Now;
	SOC_Runtime_State.u16PersistSeq = ptSnap->u16Seq;
	SOC_UpdatePersistMirror();
}

static void SOC_SavePersistSnapshot(UINT8 reason, UINT8 confidence)
{
	struct SOC_PERSIST_SNAPSHOT snap;
	UINT8 i;
	UINT8 slot;
	UINT16 *pData;

	slot = (UINT8)((SOC_Runtime_State.u16PersistSeq + 1) & 0x01);
	snap.u16Magic = SOC_PERSIST_MAGIC;
	snap.u16Seq = (UINT16)(SOC_Runtime_State.u16PersistSeq + 1);
	snap.u16Soc = SOC_Calculate_Element.u8SOC_Now;
	snap.u16DsgSocInt = SOC_Calculate_Element.u8DSG_SOC_Int;
	snap.u16CycleTimes = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100);
	snap.u16CapFullAh = (UINT16)(SOC_Calculate_Element.u32CapFull / 3600);
	if (snap.u16CapFullAh == 0)
	{
		snap.u16CapFullAh = (UINT16)(SOC_Calculate_Element.u32CapFactory / 3600);
	}
	snap.u16Checksum = SOC_PersistChecksum(&snap);

	pData = &snap.u16Magic;
	for (i = 0; i < SOC_PERSIST_SLOT_WORDS; ++i)
	{
		WriteEEPROM_Word_NoZone(SOC_PersistAddr(slot, i), pData[i]);
	}
	SOC_Runtime_State.u16PersistSeq = snap.u16Seq;
	SOC_Runtime_State.u8InitConfidence = confidence;
	SOC_Runtime_State.u8RestoreReason = reason;
	SOC_UpdatePersistMirror();
	SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
	WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag, SOC_E2prom_Par.u16_SeriousFaultFlag);
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

#if 1 // 这个计算方式还是妥一些，满减1%，SOC才显示99，客户体验会更好一些
	if (SOC_Calculate_Element.u8DSG_AHCalcu_Flag)
	{
		Correction_Terminal(CurDSG);

		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; //As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow-= ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; 	//剩余容量实时跟踪
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Enhance_Element.u16_Idsg * 1;

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
	if (Command == EEPROM_DATA_REFRESH)
	{
		SOC_SavePersistSnapshot(SOC_RESTORE_REASON_STORE,
			SOC_Runtime_State.u8InitConfidence ? SOC_Runtime_State.u8InitConfidence : SOC_INIT_CONFIDENCE_MEDIUM);
	}
}

void SOC_Update_StartUp(void)
{
	struct SOC_PERSIST_SNAPSHOT snap;
	UINT8 need_save = 0;
	UINT8 startup_soc;
	UINT8 confidence;
	UINT8 reason;

	confidence = SOC_INIT_CONFIDENCE_LOW;
	reason = SOC_RESTORE_REASON_FALLBACK;

	switch (SOC_E2prom_Par.u16_SeriousFaultFlag)
	{
	case EEPROM_VALUE_POWEROFF_FLAG:
	case EEPROM_VALUE_SLEEP_FLAG:
		if (SOC_LoadLatestPersistSnapshot(&snap))
		{
			SOC_ApplySnapshot(&snap);
			reason = SOC_RESTORE_REASON_STORE;
			confidence = SOC_INIT_CONFIDENCE_HIGH;
		}
		else if (SOC_LoadLegacySnapshot(&snap))
		{
			SOC_ApplySnapshot(&snap);
			reason = SOC_RESTORE_REASON_STORE;
			confidence = SOC_INIT_CONFIDENCE_MEDIUM;
			need_save = 1;
		}
		else
		{
			startup_soc = SOC_GetLimitedOcvSoc();
			SOC_Calculate_Element.u8SOC_Now = startup_soc;
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
			SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
			SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
			SOC_Calculate_Element.u32CapNow = (UINT32)startup_soc * SOC_Calculate_Element.u32CapFactory / 100;
			SOC_Runtime_State.u8SocReal = startup_soc;
			SOC_Runtime_State.u8SocDisplay = startup_soc;
			reason = SOC_RESTORE_REASON_OCV;
			confidence = SOC_CanUseOcvAtStartup() ? SOC_INIT_CONFIDENCE_HIGH : SOC_INIT_CONFIDENCE_LOW;
			need_save = 1;
		}
		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
		WriteEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag, SOC_E2prom_Par.u16_SeriousFaultFlag);
		break;

	case EEPROM_VALUE_DATA_UPDATE_FLAG:
		switch (SOC_Enhance_Element.u16_RefreshData_Flag)
		{
		case 1:
			startup_soc = SOC_GetLimitedOcvSoc();
			SOC_Calculate_Element.u8SOC_Now = startup_soc;
			reason = SOC_RESTORE_REASON_OCV;
			confidence = SOC_CanUseOcvAtStartup() ? SOC_INIT_CONFIDENCE_HIGH : SOC_INIT_CONFIDENCE_MEDIUM;
			break;

		case 2:
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
			SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600;
			SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
			SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;
			SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
			startup_soc = SOC_GetLimitedOcvSoc();
			SOC_Calculate_Element.u8SOC_Now = startup_soc;
			reason = SOC_RESTORE_REASON_PARAM;
			confidence = SOC_INIT_CONFIDENCE_MEDIUM;
			break;

		case 3:
			SOC_Calculate_Element.u8SOC_Now = SOC_Enhance_Element.u8_SetSocOnce;
			reason = SOC_RESTORE_REASON_MANUAL;
			confidence = SOC_INIT_CONFIDENCE_HIGH;
			break;

		default:
			break;
		}
		SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		SOC_Runtime_State.u8SocReal = SOC_Calculate_Element.u8SOC_Now;
		SOC_Runtime_State.u8SocDisplay = SOC_Calculate_Element.u8SOC_Now;
		SOC_Calculate_Element.u8_DataUpdateOK = 1;
		need_save = 1;
		SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
		break;

	default:
		startup_soc = SOC_GetLimitedOcvSoc();
		SOC_Calculate_Element.u8SOC_Now = startup_soc;
		SOC_Calculate_Element.u8DSG_SOC_Int = 0;
		SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u32CapNow = (UINT32)startup_soc * SOC_Calculate_Element.u32CapFactory / 100;
		SOC_Runtime_State.u8SocReal = startup_soc;
		SOC_Runtime_State.u8SocDisplay = startup_soc;
		reason = SOC_RESTORE_REASON_OCV;
		confidence = SOC_CanUseOcvAtStartup() ? SOC_INIT_CONFIDENCE_HIGH : SOC_INIT_CONFIDENCE_LOW;
		need_save = 1;
		break;
	}

	if (SOC_Calculate_Element.u32CapFull == 0)
	{
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	}
	if (SOC_Calculate_Element.u8SOC_Now > 100)
	{
		SOC_Calculate_Element.u8SOC_Now = 100;
	}
	SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
	SOC_Runtime_State.u8SocReal = SOC_Calculate_Element.u8SOC_Now;
	SOC_Runtime_State.u8SocDisplay = SOC_Calculate_Element.u8SOC_Now;
	SOC_Runtime_State.u8InitConfidence = confidence;
	SOC_Runtime_State.u8RestoreReason = reason;
	SOC_UpdatePersistMirror();

	if (need_save)
	{
		SOC_SavePersistSnapshot(reason, confidence);
	}

	SOC_Enhance_Element.u16_SOC_InitOver = 1;
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
	static UINT8 su8_SaveDelay = 0;
	UINT8 dirty = 0;
	UINT16 cycle_now;
	UINT16 cap_full_now;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if (++su8_TimeCnt < 5)
	{
		return;
	}
	su8_TimeCnt = 0;

	cycle_now = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100);
	cap_full_now = (UINT16)(SOC_Calculate_Element.u32CapFull / 3600);
	if (cap_full_now == 0)
	{
		cap_full_now = (UINT16)(SOC_Calculate_Element.u32CapFactory / 3600);
	}

	if (!SOC_Runtime_State.u8PersistReady)
	{
		SOC_UpdatePersistMirror();
	}

	if (SOC_Calculate_Element.u8SOC_Now != SOC_Runtime_State.u8PersistSoc)
	{
		dirty = 1;
	}
	if (SOC_Calculate_Element.u8DSG_SOC_Int != SOC_Runtime_State.u8PersistDsg)
	{
		dirty = 1;
	}
	if (cycle_now != SOC_Runtime_State.u16PersistCycle)
	{
		dirty = 1;
	}
	if (cap_full_now != SOC_Runtime_State.u16PersistCapFullAh)
	{
		dirty = 1;
	}

	if (!dirty)
	{
		su8_SaveDelay = 0;
		return;
	}

	if (++su8_SaveDelay < 10)
	{
		return;
	}
	su8_SaveDelay = 0;
	SOC_SavePersistSnapshot(SOC_RESTORE_REASON_STORE,
		SOC_Runtime_State.u8InitConfidence ? SOC_Runtime_State.u8InitConfidence : SOC_INIT_CONFIDENCE_MEDIUM);
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
	UINT8 real_soc;

	if (++su8_TimeCnt < 5)
	{
		return;
	}
	su8_TimeCnt = 0;

	real_soc = SOC_Calculate_Element.u8SOC_Now;
	if (real_soc > 100)
	{
		real_soc = 100;
		SOC_Calculate_Element.u8SOC_Now = 100;
	}
	SOC_Runtime_State.u8SocReal = real_soc;

	if (SOC_Runtime_State.u8SocDisplay > 100)
	{
		SOC_Runtime_State.u8SocDisplay = real_soc;
	}
	if (SOC_Runtime_State.u8SocDisplay < real_soc)
	{
		SOC_Runtime_State.u8SocDisplay += (UINT8)((isCHG() && (real_soc - SOC_Runtime_State.u8SocDisplay) > 3) ? 2 : 1);
		if (SOC_Runtime_State.u8SocDisplay > real_soc)
		{
			SOC_Runtime_State.u8SocDisplay = real_soc;
		}
	}
	else if (SOC_Runtime_State.u8SocDisplay > real_soc)
	{
		if (!isCHG() || (SOC_Runtime_State.u8SocDisplay - real_soc) > 1)
		{
			SOC_Runtime_State.u8SocDisplay -= 1;
		}
	}

	SOC_Enhance_Element.u8_SOC = SOC_Runtime_State.u8SocDisplay;
	if (SOC_Calculate_Element.u32CapFull >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Enhance_Element.u8_SOH = 100;
	}
	else
	{
		SOC_Enhance_Element.u8_SOH = (UINT8)((100 * SOC_Calculate_Element.u32CapFull / SOC_Calculate_Element.u32CapFactory) & 0xFF);
	}
	SOC_Enhance_Element.u16_CapacityNow = ((UINT32)SOC_Runtime_State.u8SocDisplay * SOC_Calculate_Element.u32CapFactory / 100) / 360;
	SOC_Enhance_Element.u16_CapacityFull = SOC_Calculate_Element.u32CapFull * 1 / 360;
	SOC_Enhance_Element.u16_CapacityFactory = SOC_Calculate_Element.u32CapFactory * 1 / 360;
	SOC_Enhance_Element.u16_Cycle_times = SOC_Calculate_Element.u32Cycle_times / 100;
	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Runtime_State.u8InitConfidence;
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
	SOC_Runtime_State.u8SocReal = 0;
	SOC_Runtime_State.u8SocDisplay = 0;
	SOC_Runtime_State.u8InitConfidence = 0;
	SOC_Runtime_State.u8RestoreReason = 0;
	SOC_Runtime_State.u16PersistSeq = 0;
	SOC_Runtime_State.u8PersistReady = 0;

#if !defined(__ONLY_UPDATE_NO_REFREH_PARAM__)
	SOC_E2prom_Par.u16_SeriousFaultFlag = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag);
#else
	SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
#endif

	SOC_Enhance_Element.u16_SOC_InitOver = 0; // 对外标志位初始化
	SOC_Cali_Flag = SOC_CALI_STARTUP;		  // 跳到下一步
}

UINT8 isCHG(void)
{
	return SOC_Enhance_Element.u16_Ichg > SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
}

UINT8 isDSG(void)
{
	return SOC_Enhance_Element.u16_Idsg > SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
}

void soc_cali(void)
{
	static uint8_t dsg_soc0_delay = 0;
// todo 实时校准 待完善
#ifdef _SOC_OCV_Fix2_func_
	SOC_OCV_Fix2();
#endif

#ifdef TERNARYLI
#define Totle_soc100 (4000)
#elif (defined(LIFEPO))
#define Totle_soc100 (3300)
#endif

	if (isCHG())
	{
		if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) && SOC_Enhance_Element.u16_VCellMin >= Totle_soc100)
		{
			SOC_Calculate_Element.u8SOC_Now = 100;
			SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
		}
	}
	else
	{
		if ((SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol) && (SOC_Enhance_Element.u16_VCellMin >= 2000))
		{
			if (++dsg_soc0_delay >= (5 * 10))
			{
				dsg_soc0_delay = 0;
				SOC_Calculate_Element.u8SOC_Now = 0;
				SOC_Calculate_Element.u32CapNow = 0;
			}
		}
		else
		{
			dsg_soc0_delay = 0;
		}
	}
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
	// SOC_Data_Filter();

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
	
	soc_cali();

	// 这几个函数的写法真的难，因为害怕长期循环所以运行一次必须不能再被运行一次的规避
	SOC_EEPROM_Deal_Monitor();
	SOC_RefreshData_Monitor(); // 有顺序，放最后>>应该没顺序了
	SOC_Result_Pass();
	// Correction_CapacityFull();
}
