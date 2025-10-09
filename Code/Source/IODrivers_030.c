#include "IODrivers_030.h"

#define _RELAY_SAME_DOOR_NO_PRECHG
#define _RELAY_SAME_DOOR_HAVE_PRECHG
#define _RELAY_DIFF_DOOR_NO_PRECHG
#define _RELAY_DIFF_DOOR_HAVE_PRECHG

#define _MOS_SAME_DOOR_NO_PRECHG
#define _MOS_SAME_DOOR_HAVE_PRECHG
#define _MOS_BOOTSTRAP_CIR
#define _MOS_BOOTSTRAP_CIR_NO_PRECHG

#define DELAYB10MS_2S ((UINT16)200)		// 2s
#define DELAYB10MS_5S ((UINT16)500)		// 5s
#define DELAYB10MS_10S ((UINT16)1000)	// 10s
#define DELAYB10MS_30S ((UINT16)3000)	// 30s
#define DELAYB10MS_2MIN ((UINT16)12000) // 30s

#define PreRelayCLOSE_MODET 1  // 打开主接触器后，预充继电器关闭时间
#define PreDsgMOSCLOSE_MODET 1 // 打开主放电管后，预充放电管关闭时间

// 用于同口MOS方案，过充放状态，如果电流大于2A，则同时开启另外一个MOS
// 原来是虚电流挂钩，但是不合理，现在改为写死
// 在预放期间，充电电流大于一定的值，也直接跳过预放，直接打开放电MOS
// MOS同口方案，过流保护之后，例如充电过流，关闭充电管，此时放电电流大于2A，不等30s直接打开充电MOS
#define CHG_MOS_OPEN_CUR 20
#define DSG_MOS_OPEN_CUR 20

typedef enum _FUNC_STATUS
{
	CONT = 0,
	RECOVER = 1
} FUNC_STATUS;

typedef enum RELAY_CTRL_COMMAND
{
	RELAY_PRE_DET,
	RELAY_PRE_OPEN_MODE,
	RELAY_PRE_CLOSE_MODE,
	RELAY_MAIN_OPEN_MODE,
	RELAY_MAIN_CLOSE_MODE,
	RELAY_ALL_CLOSE_MODE
} RelayCtrl_Command;

typedef enum MOS_CTRL_COMMAND
{
	MOS_PRE_DET,
	MOS_PRE_OPEN_MODE,
	MOS_PRE_CLOSE_MODE,
	MOS_MAIN_OPEN_MODE,
	MOS_MAIN_CLOSE_MODE,
	MOS_ALL_CLOSE_MODE
} MosCtrl_Command;

typedef struct DRIVER_GPIO
{
	GPIO_TypeDef *GPIOx_PreChg;
	UINT16 PinX_PreChg;

	GPIO_TypeDef *GPIOx_CHG;
	UINT16 PinX_CHG;

	GPIO_TypeDef *GPIOx_DSG;
	UINT16 PinX_DSG;

	GPIO_TypeDef *GPIOx_MAIN;
	UINT16 PinX_MAIN;
} DriverGPIO;

RelayCtrl_Command Relay_Command_SameDoor_HavePreChg = RELAY_PRE_DET;
RelayCtrl_Command Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_DET;

MosCtrl_Command MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_DET;
MosCtrl_Command MosCtrl_Command_BootStrap_Cir = MOS_PRE_DET;

DriverElement Driver_Element;
DriverGPIO Driver_GPIO;

#define UPDNLMT16(Var, Max, Min)                  \
	{                                             \
		(Var) = ((Var) >= (Max)) ? (Max) : (Var); \
		(Var) = ((Var) <= (Min)) ? (Min) : (Var); \
	}

void DriversOnOFF(DriversStatus IoStatus, GPIO_Type GpioType);
UINT8 PreChg_Ctrl(FUNC_STATUS FuncStatus);

#ifdef _RELAY_SAME_DOOR_NO_PRECHG
/*
//原来是放下面的，突然发现了一个问题
//假设在过压保护，然后放电，出现过流保护(短路出现，或者客户没设置好)
//这个时候，如果我不改动，则出现过流保护30s恢复后，进入过压保护一栏循环(这个时候标志位没恢复，所以会立刻打开管子)
//但是，下面通过su8_FR_ChgOcp_Flag执行的一系列次数+1之类的操作便没有
//所以过压状态会出现连续好多次放电过流也不会锁死管子的BUG。
//只会在过压保护复原之后，才会次数计算+1
//不改问题也不大，比较极端使用。放电过流和充电过流实际上比较少触发，而且放电过大，过几下一般就恢复，不再过压了。
//极端，大容量，恢复电压比较低，差距大，能保持过压状态很久，则容易出现这个问题。
//解决这个问题，采用隔开法，把过流保护提上来，先处理完，再处理过压保护。不相互影响
......
......
......
>>后续，为了解决一堆报错出现的各种问题的可能性，决定修改代码架构，改为各个子模块独立判断，最后加权集合判断的模式。
1，虽然这种可能性比较小，例如过压低压存在的情况(电池或者板子坏了)，没触发压差过大，必须关了两个管子,
   但实际关了过压，低压没关，还能放电。，又或者上面说的，过压和过流在一块。
2，改了架构之后，逻辑更清晰，后续维护更容易。修改添加新功能也容易。
*/
void RelayCtrl_SameDoor_NoPreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp;

	static UINT8 su8_FR_ChgOcp_Flag = 0;
	// static UINT16 su16_FR_ChgOcp_Tcnt = 0;
	static UINT8 su8_FR_ChgOcp_RecTimes = 0;
	static UINT32 su32_FR_ChgOcp_RecNormalCnt = 0;

	static UINT8 su8_FR_DsgOcp_Flag = 0;
	// static UINT16 su16_FR_DsgOcp_Tcnt = 0;
	static UINT8 su8_FR_DsgOcp_RecTimes = 0;
	static UINT32 su32_FR_DsgOcp_RecNormalCnt = 0;

	static UINT16 su16_FR_OVP_Tcnt = 0;
	static UINT8 su8_FR_OVP_RecTimes = 0;
	static UINT32 su32_FR_OVP_RecNormalCnt = 0;

	static UINT16 su16_FR_UVP_Tcnt = 0;
	static UINT8 su8_FR_UVP_RecTimes = 0;
	static UINT32 su32_FR_UVP_RecNormalCnt = 0;

	static UINT16 su16_OVPCLOSE_MODECnt = 0;
	static UINT16 su16_UVPCLOSE_MODECnt = 0;

	// 重要性从高到低
	static DriversStatus s_Main_Status_Normal = OPEN_MODE;
	static DriversStatus s_Main_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_Main_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_Main_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_Main_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_Main_Status_VolUvp = OPEN_MODE;
	// static DriversStatus s_IO_Status_SocLow = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_Main_Status_Normal = OPEN_MODE;
	s_Main_Status_Vdelta = OPEN_MODE;
	s_Main_Status_ChgOcp = OPEN_MODE;
	s_Main_Status_DsgOcp = OPEN_MODE;
	s_Main_Status_VolOvp = OPEN_MODE;
	s_Main_Status_VolUvp = OPEN_MODE;

	// 普通类型没有复原机制，直接一个if语句
	if ((Driver_Element.Fault_Flag.all & 0x2800) || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_Main_Status_Normal = CLOSE_MODE;
	}

	// 压差过大也不需要switch，直接关
	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_Main_Status_Vdelta = CLOSE_MODE;
		Driver_Element.u8_FuncOFF_Flag = 1;
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	// 思前考后一大堆，决定简化，优化，既然外部能自行延时30s再释放标志位，我把权利交出去
	// 自行不再延时30s，跟随标志位变化，能极大简化代码复杂度。
	// 同时也把叠加保护的问题同时处理掉
	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_Main_Status_ChgOcp = CLOSE_MODE;
		if (!su8_FR_ChgOcp_Flag)
		{
			su8_FR_ChgOcp_Flag = 1;
			if (++su8_FR_ChgOcp_RecTimes >= 3)
			{
				su8_FR_ChgOcp_RecTimes = 0;			// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_ChgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_ChgOcp_RecNormalCnt)
			su32_FR_ChgOcp_RecNormalCnt = 0;
		break;

	case 0:
		// s_Main_Status_DsgOcp = OPEN_MODE;
		su8_FR_ChgOcp_Flag = 0; // 复原
		if (su8_FR_ChgOcp_RecTimes >= 1)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_ChgOcp_RecNormalCnt > (UINT32)(DELAYB10MS_2MIN))
			{
				su32_FR_ChgOcp_RecNormalCnt = 0;
				su8_FR_ChgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_Main_Status_DsgOcp = CLOSE_MODE;
		if (!su8_FR_DsgOcp_Flag)
		{
			su8_FR_DsgOcp_Flag = 1;
			if (++su8_FR_DsgOcp_RecTimes >= 3)
			{
				su8_FR_DsgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_DsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_DsgOcp_RecNormalCnt)
			su32_FR_DsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_DsgOcp_Flag = 0;
		if (su8_FR_DsgOcp_RecTimes >= 1)
		{
			if (++su32_FR_DsgOcp_RecNormalCnt > (UINT32)(DELAYB10MS_2MIN))
			{
				su32_FR_DsgOcp_RecNormalCnt = 0;
				su8_FR_DsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	// 以下这个写法很正确，如果充电不能回溯到过压保护消失，则30s打开，如果回溯到消失则立刻打开
	// 要加强标志位的处理还有这个3次恢复正常BUG
	// 讨论完毕，出现跳的情况只会在跨过恢复点的电流值(大电流)，打开后立刻充进大电流电压又虚高又跳
	// 最后结论维持，A：目前场景不会出现打开瞬间出现大电流现象。B，现在没保护立刻打开客户用得挺爽的，如果有个30s可能又要说。
	// 恢复点设差大一些
	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	switch (temp)
	{
	case 1:
		s_Main_Status_VolOvp = CLOSE_MODE;
		if (++su16_FR_OVP_Tcnt >= Driver_Element.u16_10msForceOpenT_Ovp)
		{
			su16_FR_OVP_Tcnt = Driver_Element.u16_10msForceOpenT_Ovp;
			s_Main_Status_VolOvp = OPEN_MODE; // 强制打开。
		}
		if (Driver_Element.u16_CurChg > Driver_Element.u16_VirCur_Chg)
		{ // 打开主接触器，这里才会有电流
			if (++su16_OVPCLOSE_MODECnt > DELAYB10MS_5S)
			{ // 如果立刻关掉，导致Sleep因为1s任务到那时候电流已经几乎为0了
				su16_OVPCLOSE_MODECnt = 0;
				s_Main_Status_VolOvp = CLOSE_MODE;
				if (su16_FR_OVP_Tcnt)
					su16_FR_OVP_Tcnt = 0;
			}
		}
		else
		{
			if (su16_OVPCLOSE_MODECnt)
				su16_OVPCLOSE_MODECnt = 0;
		}

		// if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN == CLOSE_MODE && Status_RelayMain == OPEN_MODE) {	//因为是一次循环就会CLOSE_MODE，所以想下次来不会再被计数
		if (s_Main_Status_VolOvp == CLOSE_MODE && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN))
		{
			if (++su8_FR_OVP_RecTimes >= 3)
			{							 // 关于次数计算，抓住一个点，在主接触器打开的情况下，将要要关闭，算一次
				su8_FR_OVP_RecTimes = 0; // 这个想法完美避免三元里和磷酸铁锂的关于电压回落是否复原，计数次数如何清零的复杂分类讨论操作
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_OV = 1;
			}
		}

		if (su32_FR_OVP_RecNormalCnt)
			su32_FR_OVP_RecNormalCnt = 0;
		break;

	case 0:
		if (su8_FR_OVP_RecTimes)
		{
			if (++su32_FR_OVP_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_OVP_RecNormalCnt = 0;
				su8_FR_OVP_RecTimes = 0;
			}
		}
		if (su16_FR_OVP_Tcnt)
			su16_FR_OVP_Tcnt = 0;
		if (su16_OVPCLOSE_MODECnt)
			su16_OVPCLOSE_MODECnt = 0;
		break;

	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	switch (temp)
	{
	case 1:
		s_Main_Status_VolUvp = CLOSE_MODE;
		if (++su16_FR_UVP_Tcnt >= Driver_Element.u16_10msForceOpenT_Uvp)
		{
			su16_FR_UVP_Tcnt = Driver_Element.u16_10msForceOpenT_Uvp;
			s_Main_Status_VolUvp = OPEN_MODE;
		}
		if (Driver_Element.u16_CurDsg > Driver_Element.u16_VirCur_Dsg)
		{ // 打开主接触器，这里才会有电流
			if (++su16_UVPCLOSE_MODECnt > DELAYB10MS_5S)
			{
				su16_UVPCLOSE_MODECnt = 0;
				s_Main_Status_VolUvp = CLOSE_MODE;
				if (su16_FR_UVP_Tcnt)
					su16_FR_UVP_Tcnt = 0;
			}
		}
		else
		{
			if (su16_UVPCLOSE_MODECnt)
				su16_UVPCLOSE_MODECnt = 0;
		}
		// if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN == CLOSE_MODE\ && Status_RelayMain == OPEN_MODE) { //因为是一次循环就会CLOSE_MODE，所以想下次来不会再被计数
		if (s_Main_Status_VolUvp == CLOSE_MODE && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN))
		{
			if (++su8_FR_UVP_RecTimes >= 3)
			{							 // 关于次数计算，抓住一个点，在主接触器打开的情况下，将要要关闭，算一次
				su8_FR_UVP_RecTimes = 0; // 这个想法完美避免三元里和磷酸铁锂的关于电压回落是否复原，计数次数如何清零的复杂分类讨论操作
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV = 1;
			}
		}

		if (su32_FR_UVP_RecNormalCnt)
			su32_FR_UVP_RecNormalCnt = 0;
		break;

	case 0:
		if (su8_FR_UVP_RecTimes)
		{
			if (++su32_FR_UVP_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_UVP_RecNormalCnt = 0;
				su8_FR_UVP_RecTimes = 0;
			}
		}
		if (su16_FR_UVP_Tcnt)
			su16_FR_UVP_Tcnt = 0;
		if (su16_UVPCLOSE_MODECnt)
			su16_UVPCLOSE_MODECnt = 0;
		break;

	default:
		break;
	}

// Soc处理和低压保护处理一致，只是Soc以后不会再跳了，也就是触发后必定需要30s打开，连续3次的话就完全关掉继电器。
#if 0 // 去掉SOC保护封管子
	else if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		if(++su16_FR_SocUp_Tcnt >= DELAYB10MS_30S && g_stCellInfoReport.u16IDischg <= Virtual_Dsg_C_Inverter) { //这里不用滤波，因为1A以内虚电流已被忽略
			su16_FR_SocUp_Tcnt = DELAYB10MS_30S;
			RelayCtrl_Command = RELAY_PRE_OPEN_MODE;
		}
		else {
			RelayCtrl_Command = RELAY_PRE_CLOSE_MODE;
			if(su16_FR_SocUp_Tcnt >= DELAYB10MS_30S) {
				su16_FR_SocUp_Tcnt = 0;
				if(++su8_FR_SocUp_RecTimes >= 3) {
					su8_FR_SocUp_RecTimes = 0;
					System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0;
				}
			}
		}
		su8_FR_Flag = 1;
	}
#endif

	// 判断结果统筹起来，全部结果为OPEN才能OPEN
	temp = s_Main_Status_Normal & s_Main_Status_Vdelta & s_Main_Status_ChgOcp;
	temp &= s_Main_Status_DsgOcp & s_Main_Status_VolOvp & s_Main_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	// Status_RelayMain = Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN;
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN, GPIO_MAIN);
}

#endif

#ifdef _RELAY_SAME_DOOR_HAVE_PRECHG

void Main_Relay_SameDoor_HavePreChg(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			Relay_Command_SameDoor_HavePreChg = RELAY_PRE_OPEN_MODE;
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			Relay_Command_SameDoor_HavePreChg = RELAY_ALL_CLOSE_MODE; // 预充期间或者预充完毕出现保护现象，均同样处理便可
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

void PreRelay_OPEN_MODE_SameDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	UINT8 result = 0;

	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = CLOSE_MODE;
		// Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = OPEN_MODE;

		result = PreChg_Ctrl(CONT);
		if (result != 2)
		{
			Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = (DriversStatus)result;
		}
		else
		{
			Relay_Command_SameDoor_HavePreChg = RELAY_MAIN_OPEN_MODE;
		}

#if 0
			Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = (DriversStatus)(!Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE);
			if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE == OPEN_MODE) {
				++su32_PreRelayOPEN_MODE_Cnt;
			}
			if(su32_PreRelayOPEN_MODE_Cnt >= (UINT32)Driver_Element.u16_PreChg_Time) {
				su32_PreRelayOPEN_MODE_Cnt = 0;
				Relay_Command_SameDoor_HavePreChg = RELAY_MAIN_OPEN_MODE;
			}
#endif
		break;

	case RECOVER:
		PreChg_Ctrl(RECOVER);
		break;

	default:
		// su32_PreRelayOPEN_MODE_Cnt = 0;
		break;
	}
}

void MainRelay_OPEN_MODE_SameDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	static UINT8 su8_PreRelayCLOSE_MODE_Cnt = 0;
	;

	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = OPEN_MODE;
		++su8_PreRelayCLOSE_MODE_Cnt;
		if (su8_PreRelayCLOSE_MODE_Cnt >= PreRelayCLOSE_MODET)
		{
			su8_PreRelayCLOSE_MODE_Cnt = 0;
			Relay_Command_SameDoor_HavePreChg = RELAY_PRE_CLOSE_MODE;
		}
		break;

	case RECOVER:
		su8_PreRelayCLOSE_MODE_Cnt = 0;
		break;

	default:
		su8_PreRelayCLOSE_MODE_Cnt = 0;
		break;
	}
}

void RelayOnOFF_Det_SameDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp;

	static UINT8 su8_FR_ChgOcp_Flag = 0;
	// static UINT16 su16_FR_ChgOcp_Tcnt = 0;
	static UINT8 su8_FR_ChgOcp_RecTimes = 0;
	static UINT32 su32_FR_ChgOcp_RecNormalCnt = 0;

	static UINT8 su8_FR_DsgOcp_Flag = 0;
	// static UINT16 su16_FR_DsgOcp_Tcnt = 0;
	static UINT8 su8_FR_DsgOcp_RecTimes = 0;
	static UINT32 su32_FR_DsgOcp_RecNormalCnt = 0;

	static UINT16 su16_FR_OVP_Tcnt = 0;
	static UINT8 su8_FR_OVP_RecTimes = 0;
	static UINT32 su32_FR_OVP_RecNormalCnt = 0;

	static UINT16 su16_FR_UVP_Tcnt = 0;
	static UINT8 su8_FR_UVP_RecTimes = 0;
	static UINT32 su32_FR_UVP_RecNormalCnt = 0;

	static UINT16 su16_OVPCLOSE_MODECnt = 0;
	static UINT16 su16_UVPCLOSE_MODECnt = 0;

	// 重要性从高到低
	static DriversStatus s_Main_Status_Normal = OPEN_MODE;
	static DriversStatus s_Main_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_Main_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_Main_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_Main_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_Main_Status_VolUvp = OPEN_MODE;
	// static DriversStatus s_IO_Status_SocLow = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_Main_Status_Normal = OPEN_MODE;
	s_Main_Status_Vdelta = OPEN_MODE;
	s_Main_Status_ChgOcp = OPEN_MODE;
	s_Main_Status_DsgOcp = OPEN_MODE;
	s_Main_Status_VolOvp = OPEN_MODE;
	s_Main_Status_VolUvp = OPEN_MODE;

	// 普通类型没有复原机制，直接一个if语句
	if ((Driver_Element.Fault_Flag.all & 0x2800) || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_Main_Status_Normal = CLOSE_MODE;
	}

	// 压差过大也不需要switch，直接关
	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_Main_Status_Vdelta = CLOSE_MODE;
		Driver_Element.u8_FuncOFF_Flag = 1;
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	// 思前考后一大堆，决定简化，优化，既然外部能自行延时30s再释放标志位，我把权利交出去
	// 自行不再延时30s，跟随标志位变化，能极大简化代码复杂度。
	// 同时也把叠加保护的问题同时处理掉
	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_Main_Status_ChgOcp = CLOSE_MODE;
		if (!su8_FR_ChgOcp_Flag)
		{
			su8_FR_ChgOcp_Flag = 1;
			if (++su8_FR_ChgOcp_RecTimes >= 3)
			{
				su8_FR_ChgOcp_RecTimes = 0;			// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_ChgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_ChgOcp_RecNormalCnt)
			su32_FR_ChgOcp_RecNormalCnt = 0;
		break;

	case 0:
		// s_Main_Status_DsgOcp = OPEN_MODE;
		su8_FR_ChgOcp_Flag = 0; // 复原
		if (su8_FR_ChgOcp_RecTimes)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_ChgOcp_RecNormalCnt > (UINT32)(DELAYB10MS_2MIN))
			{
				su32_FR_ChgOcp_RecNormalCnt = 0;
				su8_FR_ChgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_Main_Status_DsgOcp = CLOSE_MODE;
		if (!su8_FR_DsgOcp_Flag)
		{
			su8_FR_DsgOcp_Flag = 1;
			if (++su8_FR_DsgOcp_RecTimes >= 3)
			{
				su8_FR_DsgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_DsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_DsgOcp_RecNormalCnt)
			su32_FR_DsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_DsgOcp_Flag = 0;
		if (su8_FR_DsgOcp_RecTimes)
		{
			if (++su32_FR_DsgOcp_RecNormalCnt > (UINT32)(DELAYB10MS_2MIN))
			{
				su32_FR_DsgOcp_RecNormalCnt = 0;
				su8_FR_DsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	// 以下这个写法很正确，如果充电不能回溯到过压保护消失，则30s打开，如果回溯到消失则立刻打开
	// 要加强标志位的处理还有这个3次恢复正常BUG
	// 讨论完毕，出现跳的情况只会在跨过恢复点的电流值(大电流)，打开后立刻充进大电流电压又虚高又跳
	// 最后结论维持，A：目前场景不会出现打开瞬间出现大电流现象。B，现在没保护立刻打开客户用得挺爽的，如果有个30s可能又要说。
	// 恢复点设差大一些
	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	switch (temp)
	{
	case 1:
		s_Main_Status_VolOvp = CLOSE_MODE;
		if (++su16_FR_OVP_Tcnt >= Driver_Element.u16_10msForceOpenT_Ovp)
		{
			su16_FR_OVP_Tcnt = Driver_Element.u16_10msForceOpenT_Ovp;
			s_Main_Status_VolOvp = OPEN_MODE; // 强制打开。
		}
		if (Driver_Element.u16_CurChg > Driver_Element.u16_VirCur_Chg)
		{ // 打开主接触器，这里才会有电流
			if (++su16_OVPCLOSE_MODECnt > DELAYB10MS_5S)
			{ // 如果立刻关掉，导致Sleep因为1s任务到那时候电流已经几乎为0了
				su16_OVPCLOSE_MODECnt = 0;
				s_Main_Status_VolOvp = CLOSE_MODE;
				if (su16_FR_OVP_Tcnt)
					su16_FR_OVP_Tcnt = 0;
			}
		}
		else
		{
			if (su16_OVPCLOSE_MODECnt)
				su16_OVPCLOSE_MODECnt = 0;
		}

		// if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN == CLOSE_MODE && Status_RelayMain == OPEN_MODE) {	//因为是一次循环就会CLOSE_MODE，所以想下次来不会再被计数
		if (s_Main_Status_VolOvp == CLOSE_MODE && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN))
		{
			if (++su8_FR_OVP_RecTimes >= 3)
			{							 // 关于次数计算，抓住一个点，在主接触器打开的情况下，将要要关闭，算一次
				su8_FR_OVP_RecTimes = 0; // 这个想法完美避免三元里和磷酸铁锂的关于电压回落是否复原，计数次数如何清零的复杂分类讨论操作
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_OV = 1;
			}
		}

		if (su32_FR_OVP_RecNormalCnt)
			su32_FR_OVP_RecNormalCnt = 0;
		break;

	case 0:
		if (su8_FR_OVP_RecTimes)
		{
			if (++su32_FR_OVP_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_OVP_RecNormalCnt = 0;
				su8_FR_OVP_RecTimes = 0;
			}
		}
		if (su16_FR_OVP_Tcnt)
			su16_FR_OVP_Tcnt = 0;
		if (su16_OVPCLOSE_MODECnt)
			su16_OVPCLOSE_MODECnt = 0;
		break;

	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	switch (temp)
	{
	case 1:
		s_Main_Status_VolUvp = CLOSE_MODE;
		if (++su16_FR_UVP_Tcnt >= Driver_Element.u16_10msForceOpenT_Uvp)
		{
			su16_FR_UVP_Tcnt = Driver_Element.u16_10msForceOpenT_Uvp;
			s_Main_Status_VolUvp = OPEN_MODE;
		}
		if (Driver_Element.u16_CurDsg > Driver_Element.u16_VirCur_Dsg)
		{ // 打开主接触器，这里才会有电流
			if (++su16_UVPCLOSE_MODECnt > DELAYB10MS_5S)
			{
				su16_UVPCLOSE_MODECnt = 0;
				s_Main_Status_VolUvp = CLOSE_MODE;
				if (su16_FR_UVP_Tcnt)
					su16_FR_UVP_Tcnt = 0;
			}
		}
		else
		{
			if (su16_UVPCLOSE_MODECnt)
				su16_UVPCLOSE_MODECnt = 0;
		}
		// if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN == CLOSE_MODE\ && Status_RelayMain == OPEN_MODE) { //因为是一次循环就会CLOSE_MODE，所以想下次来不会再被计数
		if (s_Main_Status_VolUvp == CLOSE_MODE && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN))
		{
			if (++su8_FR_UVP_RecTimes >= 3)
			{							 // 关于次数计算，抓住一个点，在主接触器打开的情况下，将要要关闭，算一次
				su8_FR_UVP_RecTimes = 0; // 这个想法完美避免三元里和磷酸铁锂的关于电压回落是否复原，计数次数如何清零的复杂分类讨论操作
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV = 1;
			}
		}

		if (su32_FR_UVP_RecNormalCnt)
			su32_FR_UVP_RecNormalCnt = 0;
		break;

	case 0:
		if (su8_FR_UVP_RecTimes)
		{
			if (++su32_FR_UVP_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_UVP_RecNormalCnt = 0;
				su8_FR_UVP_RecTimes = 0;
			}
		}
		if (su16_FR_UVP_Tcnt)
			su16_FR_UVP_Tcnt = 0;
		if (su16_UVPCLOSE_MODECnt)
			su16_UVPCLOSE_MODECnt = 0;
		break;

	default:
		break;
	}

// Soc处理和低压保护处理一致，只是Soc以后不会再跳了，也就是触发后必定需要30s打开，连续3次的话就完全关掉继电器。
#if 0 // 去掉SOC保护封管子
	else if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		if(++su16_FR_SocUp_Tcnt >= DELAYB10MS_30S && g_stCellInfoReport.u16IDischg <= Virtual_Dsg_C_Inverter) { //这里不用滤波，因为1A以内虚电流已被忽略
			su16_FR_SocUp_Tcnt = DELAYB10MS_30S;
			RelayCtrl_Command = RELAY_PRE_OPEN_MODE;
		}
		else {
			RelayCtrl_Command = RELAY_PRE_CLOSE_MODE;
			if(su16_FR_SocUp_Tcnt >= DELAYB10MS_30S) {
				su16_FR_SocUp_Tcnt = 0;
				if(++su8_FR_SocUp_RecTimes >= 3) {
					su8_FR_SocUp_RecTimes = 0;
					System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0;
				}
			}
		}
		su8_FR_Flag = 1;
	}
#endif

	// 判断结果统筹起来，全部结果为OPEN才能OPEN
	temp = s_Main_Status_Normal & s_Main_Status_Vdelta & s_Main_Status_ChgOcp;
	temp &= s_Main_Status_DsgOcp & s_Main_Status_VolOvp & s_Main_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	Main_Relay_SameDoor_HavePreChg(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN);
}

void RelayCtrl_SameDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	RelayOnOFF_Det_SameDoor_HavePreChg(OnOFF_Ctrl); // 即使在预充继电器动作期间也持续监控

	switch (Relay_Command_SameDoor_HavePreChg)
	{
	case RELAY_PRE_DET:
		// RelayOnOFF_Det_SameDoor_HavePreChg();	//以前那个放出去不行，依据MOS的经验改回来
		break;

	case RELAY_PRE_OPEN_MODE:
		PreRelay_OPEN_MODE_SameDoor_HavePreChg(CONT);
		break;

	case RELAY_PRE_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
		Relay_Command_SameDoor_HavePreChg = RELAY_PRE_DET;
		break;

	case RELAY_MAIN_OPEN_MODE:
		MainRelay_OPEN_MODE_SameDoor_HavePreChg(CONT);
		break;

	case RELAY_MAIN_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = CLOSE_MODE;
		Relay_Command_SameDoor_HavePreChg = RELAY_PRE_DET;
		break;

	case RELAY_ALL_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN = CLOSE_MODE;
		PreRelay_OPEN_MODE_SameDoor_HavePreChg(RECOVER);
		MainRelay_OPEN_MODE_SameDoor_HavePreChg(RECOVER);

		Relay_Command_SameDoor_HavePreChg = RELAY_PRE_DET;
		break;

	default:
		break;
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE, GPIO_PreCHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN, GPIO_MAIN);
}

#endif

#ifdef _RELAY_DIFF_DOOR_NO_PRECHG

void RelayCtrl_DiffDoor_NoPreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp;

	static UINT8 su8_FR_IchgOcp_Flag = 0;
	static UINT8 su8_FR_IchgOcp_RecTimes = 0;
	static UINT32 su32_FR_IchgOcp_RecNormalCnt = 0; // 和预充无关，但也配合配一下吧

	static UINT8 su8_FR_IdsgOcp_Flag = 0;
	static UINT8 su8_FR_IdsgOcp_RecTimes = 0;
	static UINT32 su32_FR_IdsgOcp_RecNormalCnt = 0; // 这个和预充相关，要改为32位

	// Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = OPEN_MODE;		//这种想法先缓一缓，好像更合理更直观一些
	// Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = OPEN_MODE;		//还有关于电流保护不
	// 这个想法最后还是付之行动，害。

	// 重要性从高到低
	static DriversStatus s_RelayCHG_Status_Normal = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_VolUvp = OPEN_MODE;

	static DriversStatus s_RelayDSG_Status_Normal = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_VolUvp = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_RelayCHG_Status_Normal = OPEN_MODE;
	s_RelayCHG_Status_Vdelta = OPEN_MODE;
	s_RelayCHG_Status_ChgOcp = OPEN_MODE;
	s_RelayCHG_Status_DsgOcp = OPEN_MODE;
	s_RelayCHG_Status_VolOvp = OPEN_MODE;
	s_RelayCHG_Status_VolUvp = OPEN_MODE;

	s_RelayDSG_Status_Normal = OPEN_MODE;
	s_RelayDSG_Status_Vdelta = OPEN_MODE;
	s_RelayDSG_Status_ChgOcp = OPEN_MODE;
	s_RelayDSG_Status_DsgOcp = OPEN_MODE;
	s_RelayDSG_Status_VolOvp = OPEN_MODE;
	s_RelayDSG_Status_VolUvp = OPEN_MODE;

	if ((Driver_Element.Fault_Flag.all & 0x2800) != 0 || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_RelayCHG_Status_Normal = CLOSE_MODE;
		s_RelayDSG_Status_Normal = CLOSE_MODE;
	}

	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_RelayCHG_Status_Vdelta = CLOSE_MODE;
		s_RelayDSG_Status_Vdelta = CLOSE_MODE;
		Driver_Element.u8_FuncOFF_Flag = 1; // 只能重启或者上位机打开该功能
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_RelayCHG_Status_ChgOcp = CLOSE_MODE;
		s_RelayDSG_Status_ChgOcp = OPEN_MODE; // 这句话其实可以去掉，但是留着
		// su8_FR_IchgOcp_Flag = 1;					//关闭立刻没电流，没保护，所以得到下面继续处理
		// 不会再出现上面那种情况，因为外部会自动维持这个标志位30s
		if (!su8_FR_IchgOcp_Flag)
		{
			su8_FR_IchgOcp_Flag = 1;
			if (++su8_FR_IchgOcp_RecTimes >= 3)
			{
				su8_FR_IchgOcp_RecTimes = 0;		// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_IchgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_IchgOcp_RecNormalCnt)
			su32_FR_IchgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IchgOcp_Flag = 0; // 复原
		if (su8_FR_IchgOcp_RecTimes >= 1)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_IchgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IchgOcp_RecNormalCnt = 0;
				su8_FR_IchgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_RelayCHG_Status_DsgOcp = OPEN_MODE;
		s_RelayDSG_Status_DsgOcp = CLOSE_MODE;

		if (!su8_FR_IdsgOcp_Flag)
		{
			su8_FR_IdsgOcp_Flag = 1;
			if (++su8_FR_IdsgOcp_RecTimes >= 3)
			{
				su8_FR_IdsgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_IdsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_IdsgOcp_RecNormalCnt)
			su32_FR_IdsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IdsgOcp_Flag = 0;
		if (su8_FR_IdsgOcp_RecTimes >= 1)
		{
			if (++su32_FR_IdsgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IdsgOcp_RecNormalCnt = 0;
				su8_FR_IdsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	if (temp)
	{
		s_RelayCHG_Status_VolOvp = CLOSE_MODE;
		s_RelayDSG_Status_VolOvp = OPEN_MODE;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	if (temp)
	{
		s_RelayCHG_Status_VolUvp = OPEN_MODE;
		s_RelayDSG_Status_VolUvp = CLOSE_MODE;
	}

#if 0 // 去掉SOC保护封管子
	if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		u8FR_SOCup_Flag = 1;
	}
#endif

	temp = s_RelayCHG_Status_Normal & s_RelayCHG_Status_Vdelta & s_RelayCHG_Status_ChgOcp;
	temp &= s_RelayCHG_Status_DsgOcp & s_RelayCHG_Status_VolOvp & s_RelayCHG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = (DriversStatus)temp;

	temp = s_RelayDSG_Status_Normal & s_RelayDSG_Status_Vdelta & s_RelayDSG_Status_ChgOcp;
	temp &= s_RelayDSG_Status_DsgOcp & s_RelayDSG_Status_VolOvp & s_RelayDSG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG, GPIO_CHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG, GPIO_DSG);
}

#endif

#ifdef _RELAY_DIFF_DOOR_HAVE_PRECHG

void CHG_Relay_DiffDoor_HavePreChg(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			// 直接控制，但是放到下面统一管理
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

// 非常巧妙的想法，把整个预充动作看成一个打开接触的逻辑包络，只执行一次便可。
// 这个思维逻辑将会被长期使用。
void DSG_Relay_DiffDoor_HavePreChg(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_OPEN_MODE;
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			Relay_Command_DiffDoor_HavePreChg = RELAY_ALL_CLOSE_MODE; // 预充期间或者预充完毕出现保护现象，均同样处理便可
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

void PreRelay_OPEN_MODE_DiffDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	UINT8 result = 0;

	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = CLOSE_MODE; // 第一次上电上位机显示错误，实际上也要这么干才对
		// Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = OPEN_MODE;

		result = PreChg_Ctrl(CONT);
		if (result != 2)
		{
			Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = (DriversStatus)result;
		}
		else
		{
			Relay_Command_DiffDoor_HavePreChg = RELAY_MAIN_OPEN_MODE;
		}

#if 0
			Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = (DriversStatus)(!Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE);
			if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE == OPEN_MODE) {
				++su32_PreRelayOPEN_MODE_Cnt;
			}		
			if(su32_PreRelayOPEN_MODE_Cnt >= (UINT32)Driver_Element.u16_PreChg_Time) {
				su32_PreRelayOPEN_MODE_Cnt = 0;
				Relay_Command_DiffDoor_HavePreChg = RELAY_MAIN_OPEN_MODE;
			}
#endif
		break;

	case RECOVER:
		PreChg_Ctrl(RECOVER);
		break;

	default:
		break;
	}
}

void MainRelay_OPEN_MODE_DiffDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	static UINT8 su8_PreRelayCLOSE_MODE_Cnt = 0;
	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = OPEN_MODE;
		++su8_PreRelayCLOSE_MODE_Cnt;
		if (su8_PreRelayCLOSE_MODE_Cnt >= PreRelayCLOSE_MODET)
		{
			su8_PreRelayCLOSE_MODE_Cnt = 0;
			Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_CLOSE_MODE;
		}
		break;

	case RECOVER:
		su8_PreRelayCLOSE_MODE_Cnt = 0;
		break;

	default:
		su8_PreRelayCLOSE_MODE_Cnt = 0;
		break;
	}
}

void RelayOnOFF_Det_DiffDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp;

	static UINT8 su8_FR_IchgOcp_Flag = 0;
	static UINT8 su8_FR_IchgOcp_RecTimes = 0;
	static UINT32 su32_FR_IchgOcp_RecNormalCnt = 0; // 和预充无关，但也配合配一下吧

	static UINT8 su8_FR_IdsgOcp_Flag = 0;
	static UINT8 su8_FR_IdsgOcp_RecTimes = 0;
	static UINT32 su32_FR_IdsgOcp_RecNormalCnt = 0; // 这个和预充相关，要改为32位

	// Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = OPEN_MODE;		//这种想法先缓一缓，好像更合理更直观一些
	// Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = OPEN_MODE;		//还有关于电流保护不
	// 这个想法最后还是付之行动，害。

	// 重要性从高到低
	static DriversStatus s_RelayCHG_Status_Normal = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_RelayCHG_Status_VolUvp = OPEN_MODE;

	static DriversStatus s_RelayDSG_Status_Normal = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_RelayDSG_Status_VolUvp = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_RelayCHG_Status_Normal = OPEN_MODE;
	s_RelayCHG_Status_Vdelta = OPEN_MODE;
	s_RelayCHG_Status_ChgOcp = OPEN_MODE;
	s_RelayCHG_Status_DsgOcp = OPEN_MODE;
	s_RelayCHG_Status_VolOvp = OPEN_MODE;
	s_RelayCHG_Status_VolUvp = OPEN_MODE;

	s_RelayDSG_Status_Normal = OPEN_MODE;
	s_RelayDSG_Status_Vdelta = OPEN_MODE;
	s_RelayDSG_Status_ChgOcp = OPEN_MODE;
	s_RelayDSG_Status_DsgOcp = OPEN_MODE;
	s_RelayDSG_Status_VolOvp = OPEN_MODE;
	s_RelayDSG_Status_VolUvp = OPEN_MODE;

	if ((Driver_Element.Fault_Flag.all & 0x2800) != 0 || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_RelayCHG_Status_Normal = CLOSE_MODE;
		s_RelayDSG_Status_Normal = CLOSE_MODE;
	}

	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_RelayCHG_Status_Vdelta = CLOSE_MODE;
		s_RelayDSG_Status_Vdelta = CLOSE_MODE;
		Driver_Element.u8_FuncOFF_Flag = 1; // 只能重启或者上位机打开该功能
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_RelayCHG_Status_ChgOcp = CLOSE_MODE;
		s_RelayDSG_Status_ChgOcp = OPEN_MODE; // 这句话其实可以去掉，但是留着
		// su8_FR_IchgOcp_Flag = 1;					//关闭立刻没电流，没保护，所以得到下面继续处理
		// 不会再出现上面那种情况，因为外部会自动维持这个标志位30s
		if (!su8_FR_IchgOcp_Flag)
		{
			su8_FR_IchgOcp_Flag = 1;
			if (++su8_FR_IchgOcp_RecTimes >= 3)
			{
				su8_FR_IchgOcp_RecTimes = 0;		// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_IchgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_IchgOcp_RecNormalCnt)
			su32_FR_IchgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IchgOcp_Flag = 0; // 复原
		if (su8_FR_IchgOcp_RecTimes >= 1)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_IchgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IchgOcp_RecNormalCnt = 0;
				su8_FR_IchgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_RelayCHG_Status_DsgOcp = OPEN_MODE;
		s_RelayDSG_Status_DsgOcp = CLOSE_MODE;

		if (!su8_FR_IdsgOcp_Flag)
		{
			su8_FR_IdsgOcp_Flag = 1;
			if (++su8_FR_IdsgOcp_RecTimes >= 3)
			{
				su8_FR_IdsgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_IdsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_IdsgOcp_RecNormalCnt)
			su32_FR_IdsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IdsgOcp_Flag = 0;
		if (su8_FR_IdsgOcp_RecTimes >= 1)
		{
			if (++su32_FR_IdsgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IdsgOcp_RecNormalCnt = 0;
				su8_FR_IdsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	if (temp)
	{
		s_RelayCHG_Status_VolOvp = CLOSE_MODE;
		s_RelayDSG_Status_VolOvp = OPEN_MODE;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	if (temp)
	{
		s_RelayCHG_Status_VolUvp = OPEN_MODE;
		s_RelayDSG_Status_VolUvp = CLOSE_MODE;
	}

#if 0 // 去掉SOC保护封管子
	if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		u8FR_SOCup_Flag = 1;
	}
#endif

	temp = s_RelayCHG_Status_Normal & s_RelayCHG_Status_Vdelta & s_RelayCHG_Status_ChgOcp;
	temp &= s_RelayCHG_Status_DsgOcp & s_RelayCHG_Status_VolOvp & s_RelayCHG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = (DriversStatus)temp;

	temp = s_RelayDSG_Status_Normal & s_RelayDSG_Status_Vdelta & s_RelayDSG_Status_ChgOcp;
	temp &= s_RelayDSG_Status_DsgOcp & s_RelayDSG_Status_VolOvp & s_RelayDSG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	CHG_Relay_DiffDoor_HavePreChg(Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG);
	DSG_Relay_DiffDoor_HavePreChg(Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG); // 该函数非常巧妙，简洁处理了以下各种状况
}

void RelayCtrl_DiffDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	RelayOnOFF_Det_DiffDoor_HavePreChg(OnOFF_Ctrl); // 即使在预充继电器动作期间也持续监控

	switch (Relay_Command_DiffDoor_HavePreChg)
	{
	case RELAY_PRE_DET:
		// Relay_Detect_Separate();
		break;

	case RELAY_PRE_OPEN_MODE:
		PreRelay_OPEN_MODE_DiffDoor_HavePreChg(CONT);
		break;

	case RELAY_PRE_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
		Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_DET;
		break;

	case RELAY_MAIN_OPEN_MODE:
		MainRelay_OPEN_MODE_DiffDoor_HavePreChg(CONT);
		break;

	case RELAY_MAIN_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = CLOSE_MODE;
		Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_DET;
		break;

	case RELAY_ALL_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
		PreRelay_OPEN_MODE_DiffDoor_HavePreChg(RECOVER);
		MainRelay_OPEN_MODE_DiffDoor_HavePreChg(RECOVER);

		Relay_Command_DiffDoor_HavePreChg = RELAY_PRE_DET;
		break;

	default:
		break;
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_PRE, GPIO_PreCHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_CHG, GPIO_CHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_Relay_DSG, GPIO_DSG);
}

#endif

// Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE;

#ifdef _MOS_SAME_DOOR_NO_PRECHG

// 关于MOS控制
void MosCtrl_SameDoor_NoPreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp = 0;

	static UINT8 su8_FR_IchgOcp_Flag = 0;
	static UINT8 su8_FR_IchgOcp_RecTimes = 0;
	static UINT32 su32_FR_IchgOcp_RecNormalCnt = 0;

	static UINT8 su8_FR_IdsgOcp_Flag = 0;
	static UINT8 su8_FR_IdsgOcp_RecTimes = 0;
	static UINT32 su32_FR_IdsgOcp_RecNormalCnt = 0;

	// Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;		//这种想法先缓一缓，好像更合理更直观一些
	// Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;		//还有关于电流保护不

	// 重要性从高到低
	static DriversStatus s_MosCHG_Status_Normal = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_VolUvp = OPEN_MODE;

	static DriversStatus s_MosDSG_Status_Normal = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_VolUvp = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_MosCHG_Status_Normal = OPEN_MODE;
	s_MosCHG_Status_Vdelta = OPEN_MODE;
	s_MosCHG_Status_ChgOcp = OPEN_MODE;
	s_MosCHG_Status_DsgOcp = OPEN_MODE;
	s_MosCHG_Status_VolOvp = OPEN_MODE;
	s_MosCHG_Status_VolUvp = OPEN_MODE;

	s_MosDSG_Status_Normal = OPEN_MODE;
	s_MosDSG_Status_Vdelta = OPEN_MODE;
	s_MosDSG_Status_ChgOcp = OPEN_MODE;
	s_MosDSG_Status_DsgOcp = OPEN_MODE;
	s_MosDSG_Status_VolOvp = OPEN_MODE;
	s_MosDSG_Status_VolUvp = OPEN_MODE;

	if ((Driver_Element.Fault_Flag.all & 0x2800) != 0 || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_MosCHG_Status_Normal = CLOSE_MODE;
		s_MosDSG_Status_Normal = CLOSE_MODE;
	}

	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_MosCHG_Status_Vdelta = CLOSE_MODE;
		s_MosDSG_Status_Vdelta = CLOSE_MODE;
		// Driver_Element.u8_FuncOFF_Flag = 1;
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_CLOSE_MODE)
	{
		if (Driver_Element.u16_CurChg > CHG_MOS_OPEN_CUR)
		{
			su8_FR_IdsgOcp_RecTimes = 0;
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
			// s_MosDSG_Status_DsgOcp = OPEN_MODE;
		}
	}
	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_CLOSE_MODE)
	{
		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{ // 如果电流大于2A，则必须立刻打开充电MOS
			// s_MosCHG_Status_ChgOcp = OPEN_MODE; // 即使目前还在30s的过流保护状态
			su8_FR_IchgOcp_RecTimes = 0;
			Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
		}
	}

	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_MosCHG_Status_ChgOcp = CLOSE_MODE;
		s_MosDSG_Status_ChgOcp = OPEN_MODE;

		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{										// 如果电流大于2A，则必须立刻打开充电MOS
			s_MosCHG_Status_ChgOcp = OPEN_MODE; // 即使目前还在30s的过流保护状态
		}

		if (!su8_FR_IchgOcp_Flag)
		{
			su8_FR_IchgOcp_Flag = 1;
			if (++su8_FR_IchgOcp_RecTimes >= 3)
			{
				su8_FR_IchgOcp_RecTimes = 0;
				// Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_IchgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_IchgOcp_RecNormalCnt)
			su32_FR_IchgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IchgOcp_Flag = 0; // 复原
		if (su8_FR_IchgOcp_RecTimes)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_IchgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IchgOcp_RecNormalCnt = 0;
				su8_FR_IchgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_MosCHG_Status_DsgOcp = OPEN_MODE;
		s_MosDSG_Status_DsgOcp = CLOSE_MODE;

		if (Driver_Element.u16_CurChg > CHG_MOS_OPEN_CUR)
		{
			s_MosDSG_Status_DsgOcp = OPEN_MODE;
		}

		if (!su8_FR_IdsgOcp_Flag)
		{
			su8_FR_IdsgOcp_Flag = 1;
			if (++su8_FR_IdsgOcp_RecTimes >= 3)
			{
				su8_FR_IdsgOcp_RecTimes = 0;
				// Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_CLOSE_MODE;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_IdsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_IdsgOcp_RecNormalCnt)
			su32_FR_IdsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IdsgOcp_Flag = 0;
		if (su8_FR_IdsgOcp_RecTimes)
		{
			if (++su32_FR_IdsgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IdsgOcp_RecNormalCnt = 0;
				su8_FR_IdsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	if (temp)
	{
		s_MosCHG_Status_VolOvp = CLOSE_MODE;
		s_MosDSG_Status_VolOvp = OPEN_MODE;
		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{
			s_MosCHG_Status_VolOvp = OPEN_MODE;
		}
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	if (temp)
	{
		s_MosCHG_Status_VolUvp = OPEN_MODE;
		s_MosDSG_Status_VolUvp = CLOSE_MODE;
		if (Driver_Element.u16_CurChg > CHG_MOS_OPEN_CUR)
		{
			s_MosDSG_Status_VolUvp = OPEN_MODE;
		}
	}

#if 0 // 去掉SOC保护封管子
	if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		u8FR_SOCup_Flag = 1;
	}
#endif

	temp = s_MosCHG_Status_Normal & s_MosCHG_Status_Vdelta & s_MosCHG_Status_ChgOcp;
	temp &= s_MosCHG_Status_DsgOcp & s_MosCHG_Status_VolOvp & s_MosCHG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = (DriversStatus)temp;

	temp = s_MosDSG_Status_Normal & s_MosDSG_Status_Vdelta & s_MosDSG_Status_ChgOcp;
	temp &= s_MosDSG_Status_DsgOcp & s_MosDSG_Status_VolOvp & s_MosDSG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG, GPIO_CHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG, GPIO_DSG);
}

#endif

#ifdef _MOS_SAME_DOOR_HAVE_PRECHG

void CHG_MOS_SameDoor_HavePreChg(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			// 直接控制，但是放到下面统一管理
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

// 非常巧妙的想法，把整个预充动作看成一个打开接触的逻辑包络，只执行一次便可。
// 这个思维逻辑将会被长期使用。
void DSG_MOS_SameDoor_HavePreChg(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_OPEN_MODE;
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			MosCtrl_Command_SameDoor_HavePreChg = MOS_ALL_CLOSE_MODE; // 预充期间或者预充完毕出现保护现象，均同样处理便可
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

void PreDsgMOS_OPEN_MODE_SameDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	UINT8 result = 0;
	static UINT32 su32_PreMOSOPEN_MODE_Cnt = 0;

	switch (FuncStatus)
	{
	case CONT:
		// if(Driver_Element.u16_CurChg < Driver_Element.u16_VirCur_Chg) {
		if (Driver_Element.u16_CurChg < CHG_MOS_OPEN_CUR)
		{																	   // 大于2A，写死，因为未来虚电路有可能为0
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE; // 第一次上电上位机显示错误，实际上也要这么干才对
			// Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = OPEN_MODE;

			result = PreChg_Ctrl(CONT);
			if (result != 2)
			{
				Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = (DriversStatus)result;
			}
			else
			{
				MosCtrl_Command_SameDoor_HavePreChg = MOS_MAIN_OPEN_MODE;
			}

#if 0
				Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = (DriversStatus)(!Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE);
				if(Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE == OPEN_MODE) {
					++su32_PreMOSOPEN_MODE_Cnt;
				}
				if(su32_PreMOSOPEN_MODE_Cnt >= (UINT32)Driver_Element.u16_PreChg_Time) {
					su32_PreMOSOPEN_MODE_Cnt = 0;
					MosCtrl_Command_SameDoor_HavePreChg = MOS_MAIN_OPEN_MODE;
				}
#endif
		}
		else
		{																	   // 充电电流过大则跳过预放环节
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE; // 第一次上电上位机显示错误，实际上也要这么干才对
			if (su32_PreMOSOPEN_MODE_Cnt)
				su32_PreMOSOPEN_MODE_Cnt = 0;
			MosCtrl_Command_SameDoor_HavePreChg = MOS_MAIN_OPEN_MODE; // 如果从没打开过预放管子，再运行一次关闭预放管子代码也没问题
		}															  // 如果已经打开预放管子了，必须要运行，很合理。
		break;

	case RECOVER:
		PreChg_Ctrl(RECOVER);
		break;

	default:
		PreChg_Ctrl(RECOVER);
		break;
	}
}

void MainDsgMOS_OPEN_MODE_SameDoor_HavePreChg(FUNC_STATUS FuncStatus)
{
	static UINT8 su8_PreMOSCLOSE_MODE_Cnt = 0;
	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		// MCUO_MOS_DSG = Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG;
		++su8_PreMOSCLOSE_MODE_Cnt;
		if (su8_PreMOSCLOSE_MODE_Cnt >= PreDsgMOSCLOSE_MODET)
		{
			su8_PreMOSCLOSE_MODE_Cnt = 0;
			MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_CLOSE_MODE;
		}
		break;

	case RECOVER:
		su8_PreMOSCLOSE_MODE_Cnt = 0;
		break;

	default:
		su8_PreMOSCLOSE_MODE_Cnt = 0;
		break;
	}
}

void MosOnOFF_Det_SameDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	UINT8 temp = 0;

	static UINT8 su8_FR_IchgOcp_Flag = 0;
	static UINT8 su8_FR_IchgOcp_RecTimes = 0;
	static UINT32 su32_FR_IchgOcp_RecNormalCnt = 0;

	static UINT8 su8_FR_IdsgOcp_Flag = 0;
	static UINT8 su8_FR_IdsgOcp_RecTimes = 0;
	static UINT32 su32_FR_IdsgOcp_RecNormalCnt = 0;

	// Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;		//这种想法先缓一缓，好像更合理更直观一些
	// Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;		//还有关于电流保护不

	// 重要性从高到低
	static DriversStatus s_MosCHG_Status_Normal = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_MosCHG_Status_VolUvp = OPEN_MODE;

	static DriversStatus s_MosDSG_Status_Normal = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_Vdelta = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_ChgOcp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_DsgOcp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_VolOvp = OPEN_MODE;
	static DriversStatus s_MosDSG_Status_VolUvp = OPEN_MODE;

	// 默认都是允许开启的。后面如果检测到错误，自己修改标志位为关闭
	s_MosCHG_Status_Normal = OPEN_MODE;
	s_MosCHG_Status_Vdelta = OPEN_MODE;
	s_MosCHG_Status_ChgOcp = OPEN_MODE;
	s_MosCHG_Status_DsgOcp = OPEN_MODE;
	s_MosCHG_Status_VolOvp = OPEN_MODE;
	s_MosCHG_Status_VolUvp = OPEN_MODE;

	s_MosDSG_Status_Normal = OPEN_MODE;
	s_MosDSG_Status_Vdelta = OPEN_MODE;
	s_MosDSG_Status_ChgOcp = OPEN_MODE;
	s_MosDSG_Status_DsgOcp = OPEN_MODE;
	s_MosDSG_Status_VolOvp = OPEN_MODE;
	s_MosDSG_Status_VolUvp = OPEN_MODE;

	if ((Driver_Element.Fault_Flag.all & 0x2800) != 0 || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		s_MosCHG_Status_Normal = CLOSE_MODE;
		s_MosDSG_Status_Normal = CLOSE_MODE;
	}

	if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		s_MosCHG_Status_Vdelta = CLOSE_MODE;
		s_MosDSG_Status_Vdelta = CLOSE_MODE;
		// Driver_Element.u8_FuncOFF_Flag = 1;
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
	case 1:
		s_MosCHG_Status_ChgOcp = CLOSE_MODE;
		s_MosDSG_Status_ChgOcp = OPEN_MODE;

		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{										// 如果电流大于2A，则必须立刻打开充电MOS
			s_MosCHG_Status_ChgOcp = OPEN_MODE; // 即使目前还在30s的过流保护状态
		}

		if (!su8_FR_IchgOcp_Flag)
		{
			su8_FR_IchgOcp_Flag = 1;
			if (++su8_FR_IchgOcp_RecTimes >= 3)
			{
				su8_FR_IchgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_IchgOcp_Flag = 0;
				return; // 不用再执行下面的代码
			}
		}
		if (su32_FR_IchgOcp_RecNormalCnt)
			su32_FR_IchgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IchgOcp_Flag = 0; // 复原
		if (su8_FR_IchgOcp_RecTimes)
		{ // 如果计数，则2min内不再触发过流保护则清零计算
			if (++su32_FR_IchgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IchgOcp_RecNormalCnt = 0;
				su8_FR_IchgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
	case 1:
		s_MosCHG_Status_DsgOcp = OPEN_MODE;
		s_MosDSG_Status_DsgOcp = CLOSE_MODE;

		if (Driver_Element.u16_CurChg > CHG_MOS_OPEN_CUR)
		{
			s_MosDSG_Status_DsgOcp = OPEN_MODE;
		}

		if (!su8_FR_IdsgOcp_Flag)
		{
			su8_FR_IdsgOcp_Flag = 1;
			if (++su8_FR_IdsgOcp_RecTimes >= 3)
			{
				su8_FR_IdsgOcp_RecTimes = 0;
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_IdsgOcp_Flag = 0;
				return;
			}
		}
		if (su32_FR_IdsgOcp_RecNormalCnt)
			su32_FR_IdsgOcp_RecNormalCnt = 0;
		break;

	case 0:
		su8_FR_IdsgOcp_Flag = 0;
		if (su8_FR_IdsgOcp_RecTimes)
		{
			if (++su32_FR_IdsgOcp_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_IdsgOcp_RecNormalCnt = 0;
				su8_FR_IdsgOcp_RecTimes = 0;
			}
		}
		break;
	default:
		break;
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellChgUtp || Driver_Element.Fault_Flag.bits.b1CellChgOtp);
	if (temp)
	{
		s_MosCHG_Status_VolOvp = CLOSE_MODE;
		s_MosDSG_Status_VolOvp = OPEN_MODE;
		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{
			s_MosCHG_Status_VolOvp = OPEN_MODE;
		}
	}

	temp = (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp);
	temp |= (Driver_Element.Fault_Flag.bits.b1CellDischgOtp || Driver_Element.Fault_Flag.bits.b1CellDischgUtp);
	if (temp)
	{
		s_MosCHG_Status_VolUvp = OPEN_MODE;
		s_MosDSG_Status_VolUvp = CLOSE_MODE;
		if (Driver_Element.u16_CurChg > CHG_MOS_OPEN_CUR)
		{
			s_MosDSG_Status_VolUvp = OPEN_MODE;
		}
	}

#if 0 // 去掉SOC保护封管子
	if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		u8FR_SOCup_Flag = 1;
	}
#endif

	temp = s_MosCHG_Status_Normal & s_MosCHG_Status_Vdelta & s_MosCHG_Status_ChgOcp;
	temp &= s_MosCHG_Status_DsgOcp & s_MosCHG_Status_VolOvp & s_MosCHG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = (DriversStatus)temp;

	temp = s_MosDSG_Status_Normal & s_MosDSG_Status_Vdelta & s_MosDSG_Status_ChgOcp;
	temp &= s_MosDSG_Status_DsgOcp & s_MosDSG_Status_VolOvp & s_MosDSG_Status_VolUvp;
	Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = (DriversStatus)temp;

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	CHG_MOS_SameDoor_HavePreChg(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
	DSG_MOS_SameDoor_HavePreChg(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG); // 该函数非常巧妙，简洁处理了以下各种状况
}

void MosCtrl_SameDoor_HavePreChg(UINT8 OnOFF_Ctrl)
{
	MosOnOFF_Det_SameDoor_HavePreChg(OnOFF_Ctrl); // 即使在预充继电器动作期间也持续监控

	switch (MosCtrl_Command_SameDoor_HavePreChg)
	{
	case MOS_PRE_DET:
		// MOS_OnOFF_Detect();
		break;

	case MOS_PRE_OPEN_MODE:
		PreDsgMOS_OPEN_MODE_SameDoor_HavePreChg(CONT);
		break;

	case MOS_PRE_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
		MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_DET;
		break;

	case MOS_MAIN_OPEN_MODE:
		MainDsgMOS_OPEN_MODE_SameDoor_HavePreChg(CONT);
		break;

	case MOS_MAIN_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_DET;
		break;

	case MOS_ALL_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
		PreDsgMOS_OPEN_MODE_SameDoor_HavePreChg(RECOVER);
		MainDsgMOS_OPEN_MODE_SameDoor_HavePreChg(RECOVER);

		MosCtrl_Command_SameDoor_HavePreChg = MOS_PRE_DET;
		break;

	default:
		break;
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE, GPIO_PreCHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG, GPIO_CHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG, GPIO_DSG);
}

#endif

#ifdef _MOS_BOOTSTRAP_CIR
void CHG_MOS_BootStrap_Cir(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			// 直接控制，但是放到下面统一管理
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			// DriversOnOFF(IoStatus, GPIO_CHG);
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

// 非常巧妙的想法，把整个预充动作看成一个打开接触的逻辑包络，只执行一次便可。
// 这个思维逻辑将会被长期使用。
void DSG_MOS_BootStrap_Cir(DriversStatus IoStatus)
{
	static UINT8 su8_IoStatus = CLOSE_MODE;

	switch (IoStatus)
	{
	case OPEN_MODE:
		if (su8_IoStatus == CLOSE_MODE)
		{
			MosCtrl_Command_BootStrap_Cir = MOS_PRE_OPEN_MODE;
			su8_IoStatus = OPEN_MODE;
		}
		break;

	case CLOSE_MODE:
		if (su8_IoStatus == OPEN_MODE)
		{
			MosCtrl_Command_BootStrap_Cir = MOS_ALL_CLOSE_MODE; // 预充期间或者预充完毕出现保护现象，均同样处理便可
			su8_IoStatus = CLOSE_MODE;
		}
		break;

	default:
		break;
	}
}

void PreDsgMOS_OPEN_MODE_BootStrap_Cir(FUNC_STATUS FuncStatus)
{
	static UINT32 su32_PreMOSOPEN_MODE_Cnt = 0;
	switch (FuncStatus)
	{
	case CONT:
		// if(Driver_Element.u16_CurChg < Driver_Element.u16_VirCur_Chg) {
		if (Driver_Element.u16_CurChg < CHG_MOS_OPEN_CUR)
		{																	   // 大于2A，写死，因为未来虚电路有可能为0
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE; // 第一次上电上位机显示错误，实际上也要这么干才对
			// Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = OPEN_MODE;
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = (DriversStatus)(!Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE);
			if (Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE == OPEN_MODE)
			{
				++su32_PreMOSOPEN_MODE_Cnt;
			}
			if (su32_PreMOSOPEN_MODE_Cnt >= (UINT32)Driver_Element.u16_PreChg_Time)
			{
				su32_PreMOSOPEN_MODE_Cnt = 0;
				MosCtrl_Command_BootStrap_Cir = MOS_MAIN_OPEN_MODE;
			}
		}
		else
		{																	   // 充电电流过大则跳过预放环节
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE; // 第一次上电上位机显示错误，实际上也要这么干才对
			if (su32_PreMOSOPEN_MODE_Cnt)
				su32_PreMOSOPEN_MODE_Cnt = 0;
			MosCtrl_Command_BootStrap_Cir = MOS_MAIN_OPEN_MODE; // 如果从没打开过预放管子，再运行一次关闭预放管子代码也没问题
		}														// 如果已经打开预放管子了，必须要运行，很合理。
		break;

	case RECOVER:
		su32_PreMOSOPEN_MODE_Cnt = 0;
		break;

	default:
		su32_PreMOSOPEN_MODE_Cnt = 0;
		break;
	}
}

void MainDsgMOS_OPEN_MODE_BootStrap_Cir(FUNC_STATUS FuncStatus)
{
	static UINT8 su8_PreMOSCLOSE_MODE_Cnt = 0;
	switch (FuncStatus)
	{
	case CONT:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		// MCUO_MOS_DSG = Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG;
		++su8_PreMOSCLOSE_MODE_Cnt;
		if (su8_PreMOSCLOSE_MODE_Cnt >= PreDsgMOSCLOSE_MODET)
		{
			su8_PreMOSCLOSE_MODE_Cnt = 0;
			MosCtrl_Command_BootStrap_Cir = MOS_PRE_CLOSE_MODE;
		}
		break;

	case RECOVER:
		su8_PreMOSCLOSE_MODE_Cnt = 0;
		break;

	default:
		su8_PreMOSCLOSE_MODE_Cnt = 0;
		break;
	}
}

void MosOnOFF_Det_BootStrap_Cir(UINT8 OnOFF_Ctrl)
{
	static UINT8 su8_FR_Flag = 0;

	static UINT8 su8_FR_ChgOcp_Flag = 0;
	static UINT16 su16_FR_ChgOcp_Tcnt = 0;
	static UINT8 su8_FR_ChgOcp_RecTimes = 0;
	static UINT32 su32_FR_ChgOcp_RecNormalCnt = 0;

	static UINT8 su8_FR_DsgOcp_Flag = 0;
	static UINT16 su16_FR_DsgOcp_Tcnt = 0;
	static UINT8 su8_FR_DsgOcp_RecTimes = 0;
	static UINT32 su32_FR_DsgOcp_RecNormalCnt = 0;

	static UINT16 su16_FR_UVP_Tcnt = 0;
	static UINT8 su8_FR_UVP_RecTimes = 0;
	static UINT32 su32_FR_UVP_RecNormalCnt = 0;

	static UINT16 su16_UVPCLOSE_MODECnt = 0;

	if ((Driver_Element.Fault_Flag.all & 0xEBC0) != 0 || Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag == FORCE_CLOSE_MODE || !OnOFF_Ctrl)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		su8_FR_Flag = 1;
	}
	// 以下的排序就是优先级的排序问题
	else if (Driver_Element.Fault_Flag.bits.b1IchgOcp)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		// Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE; // 实际上这个应该是OPEN才合理，但是因为放电的问题，方便，统一处理
		su8_FR_ChgOcp_Flag = 1;											   // 关闭立刻没电流，没保护，所以得到下面继续处理
		su8_FR_Flag = 1;
	}
	// 自举电路，放电管关闭，充电管必须关闭
	else if (Driver_Element.Fault_Flag.bits.b1IdischgOcp)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		su8_FR_DsgOcp_Flag = 1; // 关闭立刻没电流，没保护，所以得到下面继续处理
		su8_FR_Flag = 1;
	}
#if 1 // 压差过大保护
	else if (Driver_Element.Fault_Flag.bits.b1VcellDeltaBig)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.u8_FuncOFF_Flag = 1;
		Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 1;
		su8_FR_Flag = 1;
	}
#endif
	else if (Driver_Element.Fault_Flag.bits.b1BatOvp || Driver_Element.Fault_Flag.bits.b1CellOvp || Driver_Element.Fault_Flag.bits.b1PackOvp)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		if (Driver_Element.u16_CurDsg > DSG_MOS_OPEN_CUR)
		{
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		}
		su8_FR_Flag = 1;
	}
#if 0 // 去掉SOC保护封管子
	else if(Driver_Element.Fault_Flag.bits.b1SocLow != 0) {
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		su8_FR_Flag = 1;
	}
#endif
	else if (Driver_Element.Fault_Flag.bits.b1BatUvp || Driver_Element.Fault_Flag.bits.b1CellUvp || Driver_Element.Fault_Flag.bits.b1PackUvp)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;

		if (++su16_FR_UVP_Tcnt >= DELAYB10MS_30S)
		{
			su16_FR_UVP_Tcnt = DELAYB10MS_30S;
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		}

		if (Driver_Element.u16_CurDsg > Driver_Element.u16_VirCur_Dsg)
		{
			if (++su16_UVPCLOSE_MODECnt > DELAYB10MS_5S)
			{
				su16_UVPCLOSE_MODECnt = 0;
				Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
				Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
				if (su16_FR_UVP_Tcnt)
					su16_FR_UVP_Tcnt = 0;
			}
		}
		else
		{
			// if(su16_FR_UVP_Tcnt)su16_FR_UVP_Tcnt = 0; 		//巨大错误，这个错误会导致打开20ms又关闭，但是测不出来？明天试试看
			if (su16_UVPCLOSE_MODECnt)
				su16_UVPCLOSE_MODECnt = 0;
		}
		// if(Driver_Element.MosRelay_Status.bits.b1Status_Relay_MAIN == CLOSE_MODE\ && Status_RelayMain == OPEN_MODE) { //因为是一次循环就会CLOSE_MODE，所以想下次来不会再被计数
		if (CLOSE_MODE == Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG && CLOSE_MODE == Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_CHG, Driver_GPIO.PinX_CHG) && OPEN_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_DSG, Driver_GPIO.PinX_DSG))
		{
			if (++su8_FR_UVP_RecTimes >= 3)
			{							 // 关于次数计算，抓住一个点，在主接触器打开的情况下，将要要关闭，算一次
				su8_FR_UVP_RecTimes = 0; // 这个想法完美避免三元里和磷酸铁锂的关于电压回落是否复原，计数次数如何清零的复杂分类讨论操作
				Driver_Element.u8_FuncOFF_Flag = 1;
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV = 1;
			}
		}

		su8_FR_Flag = 1;
	}
	else
	{
		su8_FR_Flag = 0;

		if (su8_FR_ChgOcp_Flag)
		{ // 过流保护断开，然后不保护，然后进来这里
			if (su8_FR_ChgOcp_RecTimes >= 3)
			{
				su8_FR_ChgOcp_RecTimes = 0;			// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 1;
				su8_FR_ChgOcp_Flag = 0;			 // 必须清零，漏了这个，如果不是开机复位，则会平白无故多了30s和一次计数
				su32_FR_ChgOcp_RecNormalCnt = 0; // 这个是否清零？清零也无妨
				return;
			}
			if (++su16_FR_ChgOcp_Tcnt >= DELAYB10MS_30S)
			{ // 30s再打开管子
				su16_FR_ChgOcp_Tcnt = 0;
				su8_FR_ChgOcp_Flag = 0;
				++su8_FR_ChgOcp_RecTimes;
			}
			// if(su16_FR_Ocp_RecNormalCnt)su16_FR_Ocp_RecNormalCnt = 0;	//必须要求打开管子5s完全没问题
		}
		else if (su8_FR_DsgOcp_Flag)
		{ // 过流保护断开，然后不保护，然后进来这里
			if (su8_FR_DsgOcp_RecTimes >= 3)
			{
				su8_FR_DsgOcp_RecTimes = 0;			// 又漏了这句话
				Driver_Element.u8_FuncOFF_Flag = 1; // 第三次打开然后再进来立刻over
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 1;
				su8_FR_DsgOcp_Flag = 0;			 // 必须清零，漏了这个，如果不是开机复位，则会平白无故多了30s和一次计数
				su32_FR_DsgOcp_RecNormalCnt = 0; // 这个是否清零？清零也无妨
				return;
			}
			if (++su16_FR_DsgOcp_Tcnt >= DELAYB10MS_30S)
			{ // 30s再打开管子
				su16_FR_DsgOcp_Tcnt = 0;
				su8_FR_DsgOcp_Flag = 0;
				++su8_FR_DsgOcp_RecTimes;
			}
			// if(su16_FR_Ocp_RecNormalCnt)su16_FR_Ocp_RecNormalCnt = 0;	//必须要求打开管子5s完全没问题
		}
		else
		{
			if (su8_FR_ChgOcp_RecTimes >= 1)
			{ // 这么做是为了防止不是连续的过流保护导致的关闭功能问题
				if (++su32_FR_ChgOcp_RecNormalCnt > DELAYB10MS_2MIN)
				{									 // 10s秒钟正常则去除电流保护统计次数
					su32_FR_ChgOcp_RecNormalCnt = 0; // 这段时间内就算出现别的保护也没问题，只要等待其正常打开管子10s便可
					su8_FR_ChgOcp_RecTimes = 0;		 // 不用担忧别的保护纠缠的问题
				}									 // 还是要担忧这个问题，假设打开管子3s，然后出现低压保护。然后恢复打开，这一次算不算呢
			}										 // 这样，必须要求打开管子10s完全没问题再清，这样就可以了。

			if (su8_FR_DsgOcp_RecTimes >= 1)
			{ // 这么做是为了防止不是连续的过流保护导致的关闭功能问题
				if (++su32_FR_DsgOcp_RecNormalCnt > DELAYB10MS_2MIN)
				{									 // 10s秒钟正常则去除电流保护统计次数
					su32_FR_DsgOcp_RecNormalCnt = 0; // 这段时间内就算出现别的保护也没问题，只要等待其正常打开管子10s便可
					su8_FR_DsgOcp_RecTimes = 0;		 // 不用担忧别的保护纠缠的问题
				}									 // 还是要担忧这个问题，假设打开管子3s，然后出现低压保护。然后恢复打开，这一次算不算呢
			}										 // 这样，必须要求打开管子10s完全没问题再清，这样就可以了。

			Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
		}
	}

	if (su8_FR_UVP_RecTimes > 0)
	{
		if (OPEN_MODE == Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG && OPEN_MODE == Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG)
		{
			if (++su32_FR_UVP_RecNormalCnt > DELAYB10MS_2MIN)
			{
				su32_FR_UVP_RecNormalCnt = 0;
				su8_FR_UVP_RecTimes = 0;
			}
		}
		else
		{
			if (su32_FR_UVP_RecNormalCnt)
				su32_FR_UVP_RecNormalCnt = 0;
		}
	}

	if (su8_FR_Flag)
	{
		if (su32_FR_ChgOcp_RecNormalCnt)
			su32_FR_ChgOcp_RecNormalCnt = 0; // 必须要求打开管子10s完全没问题
		if (su32_FR_DsgOcp_RecNormalCnt)
			su32_FR_DsgOcp_RecNormalCnt = 0; // 必须要求打开管子10s完全没问题
	}
	else
	{
		if (su16_FR_UVP_Tcnt)
		{
			su16_FR_UVP_Tcnt = 0;
		}
	}

	// 如果两个都关闭，充电管要打开的时候，必须先打开放电管10ms
	if (CLOSE_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_CHG, Driver_GPIO.PinX_CHG) && CLOSE_MODE == GPIO_ReadOutputDataBit(Driver_GPIO.GPIOx_DSG, Driver_GPIO.PinX_DSG) && OPEN_MODE == Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG)
	{

		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE; // 下个周期再打开，很巧妙，这里依然可以用
	}

	// DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG, GPIO_CHG);
	// DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG, GPIO_DSG);
	// 如果要调用预充，貌似直接复制粘贴，然后改下函数名和MosCtrl_Command_BootStrap_Cir这个命令名字便可以了
	CHG_MOS_BootStrap_Cir(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
	DSG_MOS_BootStrap_Cir(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG); // 该函数非常巧妙，简洁处理了以下各种状况
}

void MosCtrl_BootStrap_Cir(UINT8 OnOFF_Ctrl)
{
	MosOnOFF_Det_BootStrap_Cir(OnOFF_Ctrl); // 即使在预充继电器动作期间也持续监控

	switch (MosCtrl_Command_BootStrap_Cir)
	{
	case MOS_PRE_DET:
		// MOS_OnOFF_Detect();
		break;

	case MOS_PRE_OPEN_MODE:
		PreDsgMOS_OPEN_MODE_BootStrap_Cir(CONT);
		break;

	case MOS_PRE_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
		MosCtrl_Command_BootStrap_Cir = MOS_PRE_DET;
		break;

	case MOS_MAIN_OPEN_MODE:
		MainDsgMOS_OPEN_MODE_BootStrap_Cir(CONT);
		break;

	case MOS_MAIN_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		MosCtrl_Command_BootStrap_Cir = MOS_PRE_DET;
		break;

	case MOS_ALL_CLOSE_MODE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
		PreDsgMOS_OPEN_MODE_BootStrap_Cir(RECOVER);
		MainDsgMOS_OPEN_MODE_BootStrap_Cir(RECOVER);

		MosCtrl_Command_BootStrap_Cir = MOS_PRE_DET;
		break;

	default:
		break;
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_KEEP_MODE)
	{
		// KEEP
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_OPEN_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = OPEN_MODE;
	}
	else if (Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG == FORCE_CLOSE_MODE)
	{
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
	}
	else
	{
		// Wrong错了
	}

	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_PRE, GPIO_PreCHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG, GPIO_CHG);
	DriversOnOFF(Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG, GPIO_DSG);
}

#endif

// 1：完成预充。0，还要继续
// 这样写的话，同时打开的100ms就没用了。
UINT8 PreChg_Ctrl(FUNC_STATUS FuncStatus)
{
	UINT8 DriversStatus_result = 0;

	static UINT8 su8_StartUp = 0;
	static UINT16 su16_Cal_Cnt = 0;
	static UINT16 su16_Time_Cnt = 0;

	static UINT16 su16_On_Tcnt = 0;
	// static UINT16 su16_OFF_Tcnt = 0;
	static UINT16 su16_Period_Tcnt = 0;

	// Driver_Element.u16_PreChg_Duty;
	// Driver_Element.u16_PreChg_Time;
	// Driver_Element.u16_PreChg_Period;

	switch (su8_StartUp)
	{
	case 0:
		su16_Period_Tcnt = Driver_Element.u16_PreChg_Period * 10; // 10ms的总次数
		UPDNLMT16(su16_On_Tcnt, 65000, 10);						  // 最少是10次
		su16_On_Tcnt = su16_Period_Tcnt * Driver_Element.u16_PreChg_Duty / 100;
		UPDNLMT16(su16_On_Tcnt, 6500, 1); // 最少是1次
		// su16_OFF_Tcnt = su16_Period_Tcnt - su16_On_Tcnt;

		su8_StartUp++;
		break;

	case 1:
		++su16_Cal_Cnt;
		if (su16_Cal_Cnt <= su16_On_Tcnt)
		{ // 打开次数统计
			DriversStatus_result = OPEN_MODE;
		}
		else if (su16_Cal_Cnt <= su16_Period_Tcnt)
		{ // 一个周期统计
			DriversStatus_result = CLOSE_MODE;
		}

		if (su16_Cal_Cnt >= su16_Period_Tcnt)
		{ // 执行完一个周期的下一个周期开端或者结束。
			su16_Cal_Cnt = 0;
			if (++su16_Time_Cnt >= Driver_Element.u16_PreChg_Time)
			{
				su16_Time_Cnt = 0;
				DriversStatus_result = 2; // 预充结束
				su8_StartUp = 0;		  // 返回
			}
		}
		break;

	default:
		break;
	}

	if (FuncStatus == RECOVER)
	{
		su8_StartUp = 0;
		su16_Cal_Cnt = 0;
		su16_Time_Cnt = 0;
		su16_On_Tcnt = 0;
		su16_Period_Tcnt = 0;
	}

	return DriversStatus_result;
}

void DriversOnOFF(DriversStatus IoStatus, GPIO_Type GpioType)
{
	switch (Driver_Element.u8_DriverCtrl_Right)
	{
	case 0:
		switch (GpioType)
		{
		case GPIO_PreCHG:
			switch (IoStatus)
			{
			case OPEN_MODE:
				GPIO_SetBits(Driver_GPIO.GPIOx_PreChg, Driver_GPIO.PinX_PreChg);
				break;
			case CLOSE_MODE:
				GPIO_ResetBits(Driver_GPIO.GPIOx_PreChg, Driver_GPIO.PinX_PreChg);
				break;
			default:
				break;
			}
			break;

		case GPIO_CHG:
			switch (IoStatus)
			{
			case OPEN_MODE:
				GPIO_SetBits(Driver_GPIO.GPIOx_CHG, Driver_GPIO.PinX_CHG);
				break;
			case CLOSE_MODE:
				GPIO_ResetBits(Driver_GPIO.GPIOx_CHG, Driver_GPIO.PinX_CHG);
				break;
			default:
				break;
			}
			break;

		case GPIO_DSG:
			switch (IoStatus)
			{
			case OPEN_MODE:
				GPIO_SetBits(Driver_GPIO.GPIOx_DSG, Driver_GPIO.PinX_DSG);
				break;
			case CLOSE_MODE:
				GPIO_ResetBits(Driver_GPIO.GPIOx_DSG, Driver_GPIO.PinX_DSG);
				break;
			default:
				break;
			}
			break;

		case GPIO_MAIN:
			switch (IoStatus)
			{
			case OPEN_MODE:
				GPIO_SetBits(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN);
				break;
			case CLOSE_MODE:
				GPIO_ResetBits(Driver_GPIO.GPIOx_MAIN, Driver_GPIO.PinX_MAIN);
				break;
			default:
				break;
			}
			break;

		default:
			break;
		}
		break;

	case 1:
		// 不作处理
		break;

	default:
		break;
	}
}

// 是不是要写一个纠错功能健壮一下？
void InitDrivers_GPIO(GPIO_TypeDef *GPIOx, UINT16 GPIO_Pin_x, GPIO_Type GpioType)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	switch (GpioType)
	{
	case GPIO_PreCHG:
		Driver_GPIO.GPIOx_PreChg = GPIOx;
		Driver_GPIO.PinX_PreChg = GPIO_Pin_x;
		break;

	case GPIO_CHG:
		Driver_GPIO.GPIOx_CHG = GPIOx;
		Driver_GPIO.PinX_CHG = GPIO_Pin_x;
		break;

	case GPIO_DSG:
		Driver_GPIO.GPIOx_DSG = GPIOx;
		Driver_GPIO.PinX_DSG = GPIO_Pin_x;
		break;

	case GPIO_MAIN:
		Driver_GPIO.GPIOx_MAIN = GPIOx;
		Driver_GPIO.PinX_MAIN = GPIO_Pin_x;
		break;

	default:
		break;
	}

	switch ((UINT32)GPIOx)
	{
	case (UINT32)GPIOA:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
		break;

	case (UINT32)GPIOB:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
		;
		break;

	case (UINT32)GPIOC:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);
		break;

	case (UINT32)GPIOD:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE);
		break;

	case (UINT32)GPIOE:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
		break;

	case (UINT32)GPIOF:
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE);
		break;

	default:
		break;
	}

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_x;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOx, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOx, GPIO_Pin_x);

	Driver_Element.DriverForceExt.bits.b2_Force_MOS_PRE = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_Relay_PRE = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_Relay_CHG = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_Relay_DSG = FORCE_KEEP_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_Relay_MAIN = FORCE_KEEP_MODE;
}

void Drivers_RelaySameDoor_NoPreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	RelayCtrl_SameDoor_NoPreChg(OnOFF_Ctrl);
}

void Drivers_RelaySameDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	RelayCtrl_SameDoor_HavePreChg(OnOFF_Ctrl);
}

void Drivers_RelayDiffDoor_NoPreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	RelayCtrl_DiffDoor_NoPreChg(OnOFF_Ctrl);
}

void Drivers_RelayDiffDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	RelayCtrl_DiffDoor_HavePreChg(OnOFF_Ctrl);
}

void Drivers_MosSameDoor_NoPreChg(UINT8 OnOFF_Ctrl)
{
	MosCtrl_SameDoor_NoPreChg(OnOFF_Ctrl);
}

void Drivers_MosSameDoor_HavePreChg(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	MosCtrl_SameDoor_HavePreChg(OnOFF_Ctrl);
}

void Drivers_MosBootStrap_Cir(UINT8 TimeBase_10ms, UINT8 OnOFF_Ctrl)
{
	if (0 == TimeBase_10ms)
	{
		return;
	}
	MosCtrl_BootStrap_Cir(OnOFF_Ctrl);
}
