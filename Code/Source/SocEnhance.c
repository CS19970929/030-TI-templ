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

// 返回A*10数值
// A，SOC<20，自动充电
// B，别的时间人为自动充，没设置就不充
// C，充电曲线已经确认，模型为y=A/(B(x-C)+D)，基于必须通过(95,10),(SOC0,100)两个点，这么一想豁然开朗
UINT16 InverterChgCurve(void)
{
	UINT16 u16_ChgCurrent = 0; // 单位：A*10

	UINT32 u32_Curve_X;
	UINT16 u16_Coef_Begin;		   // 恒压充电开始百分比，%
	UINT16 u16_Coef_Over;		   // 恒压充电结束百分比，%
	UINT32 u32_Curve_95SocCap;	   // Ah*100
	UINT32 u32_ConstVor_StartCap0; // Ah*100
	static UINT32 su32_Curve_A = 0;
	static INT32 si32_Curve_B = 0;
	static INT32 si32_Curve_C = 0;
	static UINT32 su32_Curve_D = 0;
	static UINT8 su8_ConstVor_StartFlag = 0;

	static UINT8 su8_Curve_CaliCoef = 100; //%
	static UINT8 su8_ConstVor_CaliCoefFlag1 = 0;
	static UINT8 su8_ConstVor_CaliCoefFlag2 = 0;

	static UINT8 su8_ChgStatus = CHG_CURVE_STARTUP;
	// static UINT8 su8_ChgStatus_Last = CHG_CURVE_STARTUP;	//是否真的要这么干？
	static UINT16 su16_ChgOver_JudgeTcnt = 0;

	switch (su8_ChgStatus)
	{
	case CHG_CURVE_STARTUP: // 防止开始误触发，因为开始数据不稳，从begin到别的路再回begin需要5min
#if 0
			if(!SystemStatus.bits.b1StartUpBMS) {
				su8_ChgStatus = CHG_CURVE_BEGIN;
			}
#endif
		su8_ChgStatus = CHG_CURVE_BEGIN;
		break;

	case CHG_CURVE_BEGIN: // 因为会自动充或者没设置不会充电，所以这个判断一下跳到下面一直
		if (SOC_Calculate_Element.u8SOC_Now >= 95)
		{
			su8_ChgStatus = CHG_CURVE_TRICKLE_CUR;
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= 3500)
		{									// 用于重新装电池第一次会在这里出现，防止给大电流
			su8_ChgStatus = CHG_CURVE_OVER; // 如果3596mV同时给一个0.5C充电，立刻过压保护，都没反应过来(客户不校SOC，SOC为0的情况会出现)
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= 3400)
		{										   // 如果静置的时候都这么大，开始的电流不能太大了
			su8_ChgStatus = CHG_CURVE_TRICKLE_CUR; // 其实没必要纠结这几个数，三种可能：
		}										   // 1，正常完完全全充完一轮。2，上电开机。3，无电流或小电流静置5min。4，大电流后不充静置5min。
		else if (SOC_Enhance_Element.u16_VCellMax >= 3350)
		{ // 所以综上考虑，就基于3便好，
			su8_ChgStatus = CHG_CURVE_CONSTANT_VOR;
		}
		else
		{
			su8_ChgStatus = CHG_CURVE_CONSTANT_CUR;
		}

		if (su8_ConstVor_StartFlag)
		{								// 放在这里初始化就豁然开朗没这么复杂了
			su8_ConstVor_StartFlag = 0; // 只要在源头恢复正常便可
		}								// 这个函数的思维必须记住
		if (su8_ConstVor_CaliCoefFlag1)
		{ // 该校准参数完整生命周期到这里结束
			su8_ConstVor_CaliCoefFlag1 = 0;
			su8_Curve_CaliCoef = 100;
		}
		if (su8_ConstVor_CaliCoefFlag2)
		{ // 该校准参数完整生命周期到这里结束
			su8_ConstVor_CaliCoefFlag2 = 0;
			su8_Curve_CaliCoef = 100;
		}
		break;
	// 0.5C充电
	case CHG_CURVE_CONSTANT_CUR:							  // 大电流的情况下，
		u16_ChgCurrent = SOC_Enhance_Element.u16_SOC_Ah >> 1; // 因为是基于电压，所以不需要判断错误的情况
		if (SOC_Enhance_Element.u16_VCellMax >= 3415)
		{
			su8_ChgStatus = CHG_CURVE_CONSTANT_VOR;
		}

		if (SOC_Enhance_Element.u16_Ichg <= SOC_VIRTUAL_CURRENT_CHG)
		{ // 10min没电流(去除虚电流)则回去再判断，如果处于放电卡死在这里会很尴尬
			if (++su16_ChgOver_JudgeTcnt > DELAYB1000MS_5MIN)
			{ // 默认为1s发一次
				su16_ChgOver_JudgeTcnt = 0;
				su8_ChgStatus = CHG_CURVE_BEGIN;
			}
		}
		else
		{
			if (su16_ChgOver_JudgeTcnt)
				su16_ChgOver_JudgeTcnt = 0;
		}

		if (SOC_Enhance_Element.u16_VCellMax >= 3600)
		{ // 充电期间出现电压快速抬升错误
			u16_ChgCurrent = 0;
			su8_ChgStatus = CHG_CURVE_ERROR_DEAL;
		}
		break;
	// 0.5C-0.1C充电
	// 恒压充电，基于函数，与SOC相关，就算电流变小使电压低于3400mV，也不能回溯要继续在该模式充下去
	// 本函数基于大电流高速充电情况再慢慢降下来的
	case CHG_CURVE_CONSTANT_VOR:
		if (!su8_ConstVor_StartFlag)
		{
			// u32_ConstVor_StartCap0 = (UINT32)g_stCellInfoReport.SocElement.u16CapacityNow;
			// u32_Curve_95SocCap = (UINT32)g_stCellInfoReport.SocElement.u16CapacityFull*95/100;
			// u32_ConstVor_StartCap0 = (UINT32)SOC_Calculate_Element.u32CapNow*10/3600;	//单位Ah*100
			u32_ConstVor_StartCap0 = (UINT32)SOC_Calculate_Element.u32CapNow; // 单位改为As*10可以吗，以上的数据*360
			u32_Curve_95SocCap = (UINT32)SOC_Calculate_Element.u32CapFull * 95 / 100;

			// 改为安时单位，曲线更平滑更多点
			//(100(95Cap-Cap0)/(95Cap-10Cap0+9Xcap)
			//(100(95Cap-Cap0)/(95Cap-5Cap0+4Xcap)。这才是0.1C，上面是0.05C
			// 再次优化为通过u16_Coef_Begin和u16_Coef_Over计算C和D的系数
			u16_Coef_Begin = 50;															  // 0.5C
			u16_Coef_Over = 10;																  // 0.1C
			su32_Curve_A = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * su8_Curve_CaliCoef / 100; // A*10
			si32_Curve_B = (INT32)u32_Curve_95SocCap - (INT32)u32_ConstVor_StartCap0;		  // 不过必须要大于0
			si32_Curve_C = (INT32)100 * u32_Curve_95SocCap / u16_Coef_Begin - (INT32)100 * u32_ConstVor_StartCap0 / u16_Coef_Over;
			su32_Curve_D = (UINT32)100 * (u16_Coef_Begin - u16_Coef_Over) / (u16_Coef_Begin * u16_Coef_Over);

			su8_ConstVor_StartFlag = 1;
		}
		if (!su8_ConstVor_CaliCoefFlag1)
		{ // 如果期间出现SOC和预估电压差距过大的问题，则作处理
			if (SOC_Enhance_Element.u16_VCellMax >= 3460 && SOC_Calculate_Element.u8SOC_Now < 60)
			{
				su8_Curve_CaliCoef = 70;
				su8_ConstVor_CaliCoefFlag1 = 1;
			}
		}
		if (!su8_ConstVor_CaliCoefFlag2)
		{ // 这两个是否有用再说
			if (SOC_Enhance_Element.u16_VCellMax >= 3490 && SOC_Calculate_Element.u8SOC_Now < 80)
			{
				su8_Curve_CaliCoef = 50;
				su8_ConstVor_CaliCoefFlag2 = 1;
			}
		}

#if 0 // 不用指数函数了，太复杂了亲，耗时150us，与参数复杂程度无关
			u16_Curve_A = 368;			//A*10
			u16_Curve_B = 1;			//增大太陡了不好
			u16_Curve_X = g_stCellInfoReport.SocElement.u16Soc - su8_ConstVor_StartSoc + 1;
			u16_ChgCurrent = (UINT16)(u16_Curve_A*pow(2.718, (double)u16_Curve_B/u16_Curve_X));
#endif

#if 0
			//原版
			u32_Curve_A = (UINT32)CHG_CUR_1C*su8_Curve_CaliCoef/100;			//A*10
			u32_Curve_B = ((UINT32)9<<20)/(su32_Curve_95SocCap-su32_ConstVor_StartCap0);	//Q20
			u32_Curve_C = su32_Curve_95SocCap;
			u32_Curve_D = 10;
			u32_Curve_X = (UINT32)g_stCellInfoReport.SocElement.u16CapacityNow;
			u16_ChgCurrent = u32_Curve_A/(u32_Curve_D-((u32_Curve_B*(u32_Curve_C-u32_Curve_X)+(2<<19))>>20));	//不能减出负数，这里的X-C要反过来C-X才对
																												//必须加个0.5四舍五入，10-8和10-9天堑之别
#endif
#if 0
			//以下为分解简化后的算法，更合适程序运行
			//(100(95-SOC0)/(95-10SOC0+9Xsoc)
			u32_Curve_A = (UINT32)CHG_CUR_1C*su8_Curve_CaliCoef/100;			//A*10
			u32_Curve_B = 95 - su32_ConstVor_StartSoc0;
			u32_Curve_C = 95 - 10*su32_ConstVor_StartSoc0;				//其实这么算是错的，为负数溢出，但是结果为什么对了呢
			u32_Curve_D = 9;
			u32_Curve_X = (UINT32)g_stCellInfoReport.SocElement.u16Soc;
			u16_ChgCurrent = (u32_Curve_A*u32_Curve_B)/(u32_Curve_C + u32_Curve_D*u32_Curve_X);
#endif

		// 把系数计算放到上面，只算一次优化程序
		u32_Curve_X = (UINT32)SOC_Calculate_Element.u32CapNow;
		u16_ChgCurrent = (su32_Curve_A * si32_Curve_B) / (su32_Curve_D * u32_Curve_X + si32_Curve_C);

		if (SOC_Calculate_Element.u8SOC_Now >= 95)
		{ // SOC>=95或者当前最高电压大于3470mV开始涓流充电
			su8_ChgStatus = CHG_CURVE_TRICKLE_CUR;
		}
		if (SOC_Enhance_Element.u16_VCellMax >= 3500)
		{ // 小电流的情况下这个是不可能触发的，后续再思考优化
			su8_ChgStatus = CHG_CURVE_TRICKLE_CUR;
		}

		if (SOC_Enhance_Element.u16_Ichg <= SOC_VIRTUAL_CURRENT_CHG)
		{
			if (++su16_ChgOver_JudgeTcnt > DELAYB1000MS_5MIN)
			{ // 10min没电流则回去再判断，如果处于放电卡死在这里会很尴尬
				su16_ChgOver_JudgeTcnt = 0;
				su8_ChgStatus = CHG_CURVE_BEGIN;
			}
		}
		else
		{
			if (su16_ChgOver_JudgeTcnt)
				su16_ChgOver_JudgeTcnt = 0;
		}

		if (SOC_Enhance_Element.u16_VCellMax >= 3600)
		{ // 充电期间出现电压快速抬升错误
			u16_ChgCurrent = 0;
			su8_ChgStatus = CHG_CURVE_ERROR_DEAL;
		}
		break;
	// 0.1C充电
	case CHG_CURVE_TRICKLE_CUR:
		u16_ChgCurrent = SOC_Enhance_Element.u16_SOC_Ah * su8_Curve_CaliCoef / 1000; // 涓流充电，如果前期出现电压上升过快，则这里承接上面系数充电
		if (SOC_Calculate_Element.u8SOC_Now == 100)
		{ // SOC==100则停止充放电
			su8_ChgStatus = CHG_CURVE_OVER;
			u16_ChgCurrent = 0;
		}
		if (SOC_Enhance_Element.u16_VCellMax >= 3500)
		{ // SOC==100则停止充放电
			su8_ChgStatus = CHG_CURVE_OVER;
		}

#if 0
			if(g_stCellInfoReport.SocElement.u16Soc >= 99) {		//不能放在这里，放在SOC计算那一块，和曲线操作分开，功能分开
				if(g_stCellInfoReport.u16VCellDelta < 20 && g_stCellInfoReport.u16VCellMin < 3330) {
					g_stCellInfoReport.SocElement.u16Soc = 98;		//防止进入SOC=100停止充电
				}
			}
#endif

		if (SOC_Enhance_Element.u16_Ichg <= SOC_VIRTUAL_CURRENT_CHG)
		{
			if (++su16_ChgOver_JudgeTcnt > DELAYB1000MS_5MIN)
			{ // 10min没电流则回去再判断，如果处于放电卡死在这里会很尴尬
				su16_ChgOver_JudgeTcnt = 0;
				su8_ChgStatus = CHG_CURVE_BEGIN;
			}
		}
		else
		{
			if (su16_ChgOver_JudgeTcnt)
				su16_ChgOver_JudgeTcnt = 0;
		}

		if (SOC_Enhance_Element.u16_VCellMax >= 3600)
		{ // 充电期间出现电压快速抬升错误
			u16_ChgCurrent = 0;
			su8_ChgStatus = CHG_CURVE_ERROR_DEAL;
		}
		break;

	case CHG_CURVE_OVER:
		u16_ChgCurrent = 0;
		if (SOC_Calculate_Element.u8SOC_Now < 100)
		{
			su8_ChgStatus = CHG_CURVE_BEGIN;
		}
		break;

	case CHG_CURVE_ERROR_DEAL:
		u16_ChgCurrent = 0;
		if (SOC_Enhance_Element.u16_VCellMax < 3400)
		{ // 只有回复到3450mV(静置或者放电都行)，才回复正常判断
			su8_ChgStatus = CHG_CURVE_BEGIN;
		}
		break;

	default:
		su8_ChgStatus = CHG_CURVE_BEGIN;
		break;
	}
	ChgValue = u16_ChgCurrent;

	return u16_ChgCurrent;
}

// 返回A*10数值
UINT16 InverterDsgCurve(void)
{
	static UINT8 su8_DsgOverFlag = 0;
	UINT16 u16_DsgCurrent = 0; // 单位：A*10
	if (SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_DsgVcell_Limit && !su8_DsgOverFlag)
	{
		u16_DsgCurrent = 1200;
	}
	else
	{
		u16_DsgCurrent = 0;
		su8_DsgOverFlag = 1;
	}

	if (su8_DsgOverFlag)
	{
		if (SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_DsgVcell_Limit + 200)
		{
			su8_DsgOverFlag = 0;
		}
	}

	DsgValue = u16_DsgCurrent;
	return u16_DsgCurrent;
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
		/*也添加20mV，无所谓，因为有条件限制，卡到95就完事了*/
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol - 120 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol - 50 && SOC_Calculate_Element.u8SOC_Now < 95)
		{ // 和放电电流对应，第一段，必须拉到95%以内
			if (++su16_SocChgCal_L1_Tcnt >= 8)
			{ // 3500-3600，0.55C，大概1min
				su16_SocChgCal_L1_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now += 1;
				SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol - 50 && SOC_Calculate_Element.u8SOC_Now < 100)
		{ // 大电流情况下，提前50mV为满电电压
			if (SOC_Calculate_Element.u8SOC_Now > 95)
			{
				if (++su16_SocChgCal_L2_Tcnt >= 3)
				{								// 主要起作用是这一段，这个数值必须慎重
					su16_SocChgCal_L2_Tcnt = 0; // 狠一些，不解释
					SOC_Calculate_Element.u8SOC_Now += 1;
					SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
			else
			{
				if (++su16_SocChgCal_L3_Tcnt >= 3)
				{ // 还是3吧，为了日志能体现平滑
					su16_SocChgCal_L3_Tcnt = 0;
					SOC_Calculate_Element.u8SOC_Now += 1;
					SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
		}

		/*
		//把上面做极端一些，这个舍弃。
		//这是基于充电必须能达到100%的终极做法，2S + 1%
		if(SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol\
			&& SOC_Calculate_Element.u8SOC_Now < 100) {
			if(++su16_SocChgCal_L4_Tcnt >= 2) {				//为什么是3而不是2呢，因为
				su16_SocChgCal_L4_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now += 1;
				SOC_Calculate_Element.u32CapNow += SOC_Calculate_Element.u32CapFull/100;
			}
		}
		*/

#ifdef _CAL_SLOW_DOWN_CHG
		if (SOC_Calculate_Element.u8SOC_Now >= 98 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol - 50)
		{ // 大电流情况下，提前50mV为满电电压
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
		/*也添加20mV，无所谓，因为有条件限制，卡到5就完事了*/
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol + 120 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol + 50 && SOC_Calculate_Element.u8SOC_Now > 5)
		{
			if (++su16_SocDsgCal_L1_Tcnt >= 8)
			{ // 第一级校准
				su16_SocDsgCal_L1_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now -= 1;
				SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol + 50 && SOC_Calculate_Element.u8SOC_Now > 0)
		{ // 我也不知道为什么要5%，想想，直接0%，与下面两个行成闭循环
			if (SOC_Calculate_Element.u8SOC_Now < 3)
			{ // 第二级校准
				if (++su16_SocDsgCal_L2_Tcnt >= 3)
				{										  // 电科大电流还是有一定的概率留下1%，从10改为8吧。
					su16_SocDsgCal_L2_Tcnt = 0;			  // 但是兼顾小电流能放久一些，不能改为6
					SOC_Calculate_Element.u8SOC_Now -= 1; // 客户好像对放电末端，如果只剩2%以内貌似可以接受，但是充电必须100%
					SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
			else
			{ // 快没电了，还有很大的SOC
				if (++su16_SocDsgCal_L3_Tcnt >= 3)
				{ // 第三级校准
					su16_SocDsgCal_L3_Tcnt = 0;
					SOC_Calculate_Element.u8SOC_Now -= 1;
					SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory / 100;
				}
			}
		}

		/*
		//把上面做极端一些，这个舍弃。
		//这是基于放电必须能达到0%的终极做法，2S - 1%
		//但实际上放电要求没充电高
		if(SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol - 70\
			&& SOC_Calculate_Element.u8SOC_Now > 0) {
			if(++su16_SocDsgCal_L4_Tcnt >= 2) {
				su16_SocDsgCal_L4_Tcnt = 0;
				SOC_Calculate_Element.u8SOC_Now -= 1;
				SOC_Calculate_Element.u32CapNow -= SOC_Calculate_Element.u32CapFactory/100;
			}
		}
		*/

		if (SOC_Calculate_Element.u8SOC_Now <= 1 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol + 50)
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
	static UINT16 su16_ChgCur_Tcnt = 0;
	static UINT16 su16_DsgCur_Tcnt = 0;
	static UINT16 su16_CalErr_Tcnt = 0;

	switch (CapFull_Cali_Flag)
	{
	case CAP_FULL_INIT:
		SOC_Calculate_Element.u32CapFull_Cal_As = 0;
		CapFull_Cali_Flag = CAP_FULL_STARTUP;
		break;

	case CAP_FULL_STARTUP:
		// 先假设这个条件肯定能达到。
		// 后续结合以前的代码，回溯整个生命周期，是否会有BUG。
		// 假设刚开始的时候，最极端的情况:
		// SOC=100，电池容量几乎为0，则必须先充电满电(放一下也没问题)，此时能达到电压和SOC匹配，再放电到0，此时，再充满，便是一次完美循环，能计算。
		// SOC=0，电池容量为满。这个时候，先放电(充一下也没问题)，则操作相反。
		// 原则是让SOC和电压匹配上。
		// 但是在优化容量的时候，不能跟SOC挂钩，有两个好处
		// 只需要明确从低压电充到，高压电，一次循环便可。如果不是的话，有可能要循环多两次，满足以上的条件。
		// SOC = 100%可能还不能满足。
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol /*&& SOC_Calculate_Element.u8SOC_Now == 0*/)
		{
			// 等待没有放电电流便可？假设有充电电流，不慌，立刻计算
			// 这里必须控制放电电流为0，抓取充电的转折点。
			// 害怕有虚电流，还是用个2吧
			if (SOC_Enhance_Element.u16_Idsg <= SOC_VIRTUAL_CURRENT_DSG)
			{
				if (++su16_DsgCur_Tcnt > 5)
				{
					su16_DsgCur_Tcnt = 0;
					CapFull_Cali_Flag = CAP_FULL_CALCU;
					SOC_Calculate_Element.u32CapFull_Cal_As = 0; // 容器初始化，开始计算
				}
			}
			else
			{
				su16_DsgCur_Tcnt = 0;
			}
		}
		break;

	case CAP_FULL_CALCU:
		// 取消SOC = 100%这个条件，在容量不明确的情况下：
		// 1，如果标称容量偏小，没有减缓机制，问题不大，直接上100%还能继续充。
		// 2，如果标称容量偏大，则SOC有可能冲不上100%。这个时候，会出现，例如SOC计算为60%。
		// 基于以上，所以条件是电压达到100%点，然后充电电流减小，则说明更新成功。
		// 基于1，如果换电池，标称容量偏小，没问题。
		// 基于2，如果标称容量差别不大或者偏大(没换电池，衰减系列)，因为SOC末端有拉快操作，所以问题不大
		// 但是最极端的话，容量计算完毕，SOC计算为60%，则需要第二次循环能处理了。绝对不能末端跳。
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol /*&& SOC_Calculate_Element.u8SOC_Now == 100*/)
		{
			if (SOC_Enhance_Element.u16_Ichg <= SOC_VIRTUAL_CURRENT_CHG && SOC_Enhance_Element.u16_Idsg <= SOC_VIRTUAL_CURRENT_DSG)
			{
				if (++su16_ChgCur_Tcnt > 5)
				{
					su16_ChgCur_Tcnt = 0;
					CapFull_Cali_Flag = CAP_FULL_SUCCESS;
				}
			}
			else
			{
				su16_ChgCur_Tcnt = 0;
			}
		}

		// 如果还没统计到满电电压，静置很久那种，久了则表示失败，设置10min
		// 如果长期静置，待机功耗，又开始充电，会有问题。
		// 0.1以内吧。设置10min，如果是0.2A，搞个20min也行。
		if (SOC_Enhance_Element.u16_Ichg < SOC_VIRTUAL_CURRENT_CHG)
		{
			if (++su16_CalErr_Tcnt >= 5 * 60 * 10)
			{
				su16_CalErr_Tcnt = 0;
				CapFull_Cali_Flag = CAP_FULL_FAIL;
			}
		}
		else
		{
			su16_CalErr_Tcnt = 0;
		}

		// 期间不允许有放电电流出现，出现意味着失败
		if (SOC_Enhance_Element.u16_Idsg > SOC_VIRTUAL_CURRENT_DSG)
		{
			if (++su16_DsgCur_Tcnt > 5)
			{
				su16_DsgCur_Tcnt = 0;
				CapFull_Cali_Flag = CAP_FULL_FAIL;
			}
		}
		else
		{
			su16_DsgCur_Tcnt = 0;
		}
		break;

	case CAP_FULL_SUCCESS:
		// 最近出现满电容量为0的情况，不知道是旧版刷新代码导致，还是运行期间真有问题，先优化一波
		// 还真有这种情况，新BMS装配的时候，低串数或者高串数接上去，然后运行起来，此时最低压为0
		// 然后就进入这个循环了，就出事了。
		if (SOC_Calculate_Element.u32CapFull_Cal_As == 0)
		{
			SOC_Calculate_Element.u32CapFull_Cal_As = SOC_Calculate_Element.u32CapFactory;
		}
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFull_Cal_As; // 满电容量更新
		SOC_Calculate_Element.u32CapFull_Cal_As = 0;
		// 当前容量必须改变。差点忘了。
		// 1，如果当前容量偏小，此时满电，SOC肯定是100%(提前到达)，然后SOC不变，当前容量直接刷到计算值。
		// 2，如果当前容量偏大，此时满电，SOC有可能达不到100%(看加速是否跟得上)，SOC不变，直接按百分比刷上。等下个循环调控。
		// 基于目前情况，不换电池的情况(单个电池用久衰减)，是2情况，则有可能一个循环没搞定，两个循环肯定搞定。
		// 所以会出现这种情况，SOC不变，满电容量减少(此时计算的满电容量比目前指少)，然后乘以一个未满的SOC，会出现当前容量突然下降的情况
		// 这种情况是可以规避的，只需要末端小电流充放电便可(完美处理一切问题，自动加速回来)
		// SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		CapFull_Cali_Flag = CAP_FULL_STARTUP;
		break;

	case CAP_FULL_FAIL:
		SOC_Calculate_Element.u32CapFull_Cal_As = 0;
		CapFull_Cali_Flag = CAP_FULL_STARTUP;
		break;

	default:
		break;
	}

	// 容量改变只会在上面的switch语句中改变。
	// 如果还是0，说明出问题了，直接回到原厂值。
	if (SOC_Calculate_Element.u32CapFull == 0)
	{
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	}
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

#if 0 // 优化后的计算方式也不太妥，也即100%的时候，稍微放下电就变成了99%，也不太好，这个放电，则用回旧的方式
	if(SOC_Calculate_Element.u8DSG_AHCalcu_Flag) {
		Correction_Terminal(CurDSG);

		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		if(SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFull) SOC_Calculate_Element.u32CapNow = 0;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u32CapNow * 100 / SOC_Calculate_Element.u32CapFull;
		SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;
		SOC_Calculate_Element.Last_Had_Done = SOC_CALI_CONT_DSG;

		SOC_Calculate_Element.u8DSG_SOC_Int += (SOC_Calculate_Element.u8SOC_Old - SOC_Calculate_Element.u8SOC_Now);		//循环次数统计
		if(SOC_Calculate_Element.u8DSG_SOC_Int >= 80) {
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
			SOC_Calculate_Element.u32Cycle_times += 100;
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

	// Par_BurnIn_Calcu();
	// Par_CapacityC_Calcu();

#if !defined(__ONLY_UPDATE_NO_REFREH_PARAM__)
	SOC_E2prom_Par.u16_SeriousFaultFlag = ReadEEPROM_Word_NoZone(SOC_E2prom_Adress.u16_SeriousFaultFlag);
#else
	SOC_E2prom_Par.u16_SeriousFaultFlag = EEPROM_VALUE_POWEROFF_FLAG;
#endif

	SOC_Enhance_Element.u16_SOC_InitOver = 0; // 对外标志位初始化
	SOC_Cali_Flag = SOC_CALI_STARTUP;		  // 跳到下一步
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
	if (0 == TimeBase_200ms)
	{
		return;
	}

	SOC_Data_Filter();

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

	// 这几个函数的写法真的难，因为害怕长期循环所以运行一次必须不能再被运行一次的规避
	SOC_EEPROM_Deal_Monitor();
	SOC_RefreshData_Monitor(); // 有顺序，放最后>>应该没顺序了
	SOC_Result_Pass();
	Correction_CapacityFull();
}
